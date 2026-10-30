/* ============================================================================
 * elib_log.h --- elib log (elib-log) unified umbrella header
 * ============================================================================
 * The complete public API and public types of this library live in this single
 * header. All sub-headers were removed as of v2.
 * Version: ELIB_LOG_VERSION (currently 2.0.0)
 *
 * Game rules (elib family v2 common principles):
 *   1. C99; must compile warning-free with
 *      arm-gcc -std=c99 -Wall -Wextra -Werror -Iinclude.
 *   2. Zero dynamic memory, zero globals: the ctx and the formatting buffer are
 *      provided statically by the user; the library owns no state of its own.
 *      NOTE: the message body is produced by vsnprintf(), so only conversion
 *      specifications that are implemented without allocation are truly
 *      allocation-free (integers, characters, strings). On newlib a floating
 *      point conversion such as %f goes through _dtoa_r(), which calls
 *      _malloc_r(); if the application must be strictly allocation-free, never
 *      pass a float to this library (project the value to an integer or to a
 *      string first).
 *   3. init memset-overwrites and re-initializes unconditionally; a ctx in any
 *      historical state is accepted; illegal configurations (NULL ctx/cfg, a
 *      NULL buffer, a buffer smaller than ELIB_LOG_MIN_BUF_SIZE, a buffer too
 *      small for the reserved fixed cost of the selected flags, a missing
 *      output callback, a one-sided critical-hook pair, more than one
 *      end-of-line flag, unknown flag bits, an out-of-range level, or
 *      ELIB_LOG_FLAG_TIMESTAMP without a timestamp callback) are rejected.
 *   4. Sources include this umbrella header directly: #include "elib_log.h",
 *      built with -Iinclude; relative-path cross-layer includes are forbidden.
 *   5. No build system ships with the library.
 *
 * Output model:
 *   One log call assembles the whole line in the caller-provided buffer and
 *   invokes the output callback exactly once, so the transport never sees a
 *   partially written line:
 *
 *     [<ESC>color m][12.345][HEAD]:message[<ESC>0m]<CR><LF>
 *
 *   - the ANSI opener/closer is emitted only when ELIB_LOG_FLAG_COLOR was given
 *     to init; the colour is therefore a per-context decision fixed at init
 *     time, never a per-call parameter;
 *   - the timestamp prefix is emitted only when ELIB_LOG_FLAG_TIMESTAMP was
 *     given and a timestamp callback was registered;
 *   - the "[HEAD]:" prefix is emitted only when head is not NULL;
 *   - the end of line is appended after the message, selected by
 *     ELIB_LOG_FLAG_EOL_CRLF / _EOL_LF / _EOL_NONE (CRLF when none is given).
 *
 * Level model:
 *   Severity ascends with the numeric value: DEBUG(0) < INFO(1) < WARNING(2) <
 *   ERROR(3). ctx->level is the minimum severity that passes, so setting it to
 *   ELIB_LOG_WARNING keeps WARNING and ERROR and drops INFO and DEBUG. The
 *   comparison happens before anything is formatted, so a filtered call costs
 *   one branch. ELIB_LOG_COMPILE_LEVEL additionally removes levels at compile
 *   time: the convenience macros expand to nothing below that threshold.
 *
 * Truncation:
 *   The unconditional part of the line (colour opener and reset plus the line
 *   terminator) is reserved when init is called, so a cut can only ever remove
 *   variable content: an escape sequence is never split and the terminator is
 *   always present. The line stays NUL-terminated inside the buffer and
 *   ELIB_LOG_ERR_TRUNCATED tells the caller that something was lost.
 *
 * Concurrency model:
 *   The formatting buffer is shared and therefore not re-entrant. The optional
 *   critical-hook pair passed to init (both NULL or both non-NULL) fences the
 *   buffer assembly; the output callback runs with the critical section
 *   released. A log call issued while another context is inside the output
 *   callback is rejected with ELIB_LOG_ERR_BUSY instead of corrupting the
 *   buffer, so an ISR must tolerate a dropped line rather than block.
 * ==========================================================================*/
#ifndef ELIB_LOG_H
#define ELIB_LOG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------- version */
#define ELIB_LOG_VERSION        "2.0.0"
#define ELIB_LOG_VERSION_MAJOR  2
#define ELIB_LOG_VERSION_MINOR  0
#define ELIB_LOG_VERSION_PATCH  0

/* --------------------------------------------------------------- error codes */
typedef enum {
    ELIB_LOG_OK = 0,                   /* Success (also: filtered out) */
    ELIB_LOG_ERR_INVALID_PARAM,        /* Illegal parameter */
    ELIB_LOG_ERR_NOT_INITIALIZED,      /* Context not initialized, or deinitialized */
    ELIB_LOG_ERR_TRUNCATED,            /* Line did not fit, output was cut */
    ELIB_LOG_ERR_BUSY,                 /* Re-entered while a line was being emitted */
} elib_log_err_t;

/* --------------------------------------------------------------- level */
/* Numeric values are plain macros so that ELIB_LOG_COMPILE_LEVEL can be used
 * inside #if; the enum reuses them so C code keeps a distinct type. */
#define ELIB_LOG_LEVEL_DEBUG    0
#define ELIB_LOG_LEVEL_INFO     1
#define ELIB_LOG_LEVEL_WARNING  2
#define ELIB_LOG_LEVEL_ERROR    3

/* Severity ascends with the value; the value is also the index into the
 * internal colour table */
typedef enum {
    ELIB_LOG_DEBUG   = ELIB_LOG_LEVEL_DEBUG,
    ELIB_LOG_INFO    = ELIB_LOG_LEVEL_INFO,
    ELIB_LOG_WARNING = ELIB_LOG_LEVEL_WARNING,
    ELIB_LOG_ERROR   = ELIB_LOG_LEVEL_ERROR,
} elib_log_level_t;

/* --------------------------------------------------------------- flags */
/* Colour is a per-context decision taken at init: without this flag no ANSI
 * escape sequence is ever written, so plain terminals and log files stay clean */
#define ELIB_LOG_FLAG_COLOR       (1u << 0)
/* Emit the "[seconds.milliseconds]" prefix (requires a timestamp callback) */
#define ELIB_LOG_FLAG_TIMESTAMP   (1u << 1)
/* Line terminator selection; at most one may be set, CRLF when none is given */
#define ELIB_LOG_FLAG_EOL_CRLF    (1u << 2)
#define ELIB_LOG_FLAG_EOL_LF      (1u << 3)
#define ELIB_LOG_FLAG_EOL_NONE    (1u << 4)

/* Smallest accepted formatting buffer: enough for the reserved tail plus a few
 * content bytes, so a tiny buffer degrades into TRUNCATED instead of overrun */
#define ELIB_LOG_MIN_BUF_SIZE     8u

/* --------------------------------------------------------------- types */
typedef struct {
    uint32_t s;                        /* Seconds */
    uint32_t ms;                       /* Milliseconds, 0..999 by convention */
} elib_log_timestamp_t;

/* Transport. Receives one NUL-terminated line; the string stays valid only for
 * the duration of the call, copy it when the transport is asynchronous. Runs
 * with the critical section released. */
typedef void (*elib_log_output_fn)(const char *str, void *user_data);

/* Optional clock; NULL disables the timestamp prefix */
typedef elib_log_timestamp_t (*elib_log_timestamp_fn)(void *user_data);

/* Configuration (value struct; init copies every field, no pointer is kept) */
typedef struct {
    char *buf;                         /* Caller-provided formatting buffer */
    uint32_t buf_size;                 /* Its size, at least ELIB_LOG_MIN_BUF_SIZE */
    elib_log_output_fn output;         /* Mandatory transport */
    elib_log_timestamp_fn timestamp;   /* Optional, may be NULL */
    void *user_data;                   /* Opaque pointer passed to both callbacks */
    elib_log_level_t level;            /* Minimum severity that passes */
    uint32_t flags;                    /* ELIB_LOG_FLAG_* combination */
} elib_log_cfg_t;

/* Context (statically allocated by the user, initialized by init) */
typedef struct {
    /* Filled in by init (copied from cfg) */
    char *buf;
    uint32_t buf_size;
    elib_log_output_fn output;
    elib_log_timestamp_fn timestamp;
    void *user_data;
    uint32_t flags;
    void (*enter_critical)(void);      /* Optional critical-section hooks; */
    void (*exit_critical)(void);       /* both NULL or both non-NULL */

    /* Runtime state */
    volatile elib_log_level_t level;   /* Minimum severity that passes */
    volatile uint8_t initialized;      /* init/deinit flag */
    volatile uint8_t busy;             /* Re-entrancy / concurrency guard */
} elib_log_ctx_t;

/* --------------------------------------------------------------- lifecycle */
/**
 * @brief Initialize the logger
 * @param ctx User-provided context pointer (any historical state; memset-overwritten)
 * @param cfg Configuration (copied, no pointer is kept)
 * @param enter_critical Optional critical-section entry hook (pair with exit_critical)
 * @param exit_critical Optional critical-section exit hook (pair with enter_critical)
 * @return OK / INVALID_PARAM (see the header banner for the rejected cases)
 */
elib_log_err_t elib_log_init(elib_log_ctx_t *ctx,
                             const elib_log_cfg_t *cfg,
                             void (*enter_critical)(void),
                             void (*exit_critical)(void));

/**
 * @brief Deinitialize the logger (NULL-safe; buffer and configuration stay in
 *        place but every other API rejects afterwards)
 */
void elib_log_deinit(elib_log_ctx_t *ctx);

/* --------------------------------------------------------------- output */
/**
 * @brief Format and emit one line
 * @param ctx Context
 * @param level Severity of this line
 * @param head Optional label rendered as "[HEAD]:"; NULL omits the prefix
 * @param fmt printf-style format, must not be NULL
 * @return OK (emitted or filtered out) / INVALID_PARAM / NOT_INITIALIZED /
 *         TRUNCATED / BUSY
 */
elib_log_err_t elib_log_write(elib_log_ctx_t *ctx,
                              elib_log_level_t level,
                              const char *head,
                              const char *fmt, ...);

/**
 * @brief Emit a hex dump line: "[HEAD]:02 89 A6 FF 5E" (upper case, single
 *        space separated, no trailing space)
 * @param data Bytes to dump; may be NULL only when len == 0
 * @param len Number of bytes
 * @return OK (emitted or filtered out) / INVALID_PARAM / NOT_INITIALIZED /
 *         TRUNCATED / BUSY
 */
elib_log_err_t elib_log_hex(elib_log_ctx_t *ctx,
                            elib_log_level_t level,
                            const char *head,
                            const uint8_t *data,
                            uint32_t len);

/* --------------------------------------------------------------- filtering */
/**
 * @brief Set the minimum severity that passes
 * @return OK / INVALID_PARAM / NOT_INITIALIZED
 */
elib_log_err_t elib_log_set_level(elib_log_ctx_t *ctx, elib_log_level_t level);

/**
 * @brief Query the current minimum severity that passes
 * @return OK / INVALID_PARAM / NOT_INITIALIZED
 */
elib_log_err_t elib_log_get_level(elib_log_ctx_t *ctx, elib_log_level_t *out_level);

/* --------------------------------------------------------------- macros */
/* Levels below ELIB_LOG_COMPILE_LEVEL are removed at compile time: the macro
 * arguments are not even evaluated, so an expensive argument disappears from the
 * binary. Define it to ELIB_LOG_LEVEL_INFO (or higher) in the build to strip the
 * verbose levels. The runtime filter still applies on top of it. */
#ifndef ELIB_LOG_COMPILE_LEVEL
#define ELIB_LOG_COMPILE_LEVEL ELIB_LOG_LEVEL_DEBUG
#endif

#if (ELIB_LOG_COMPILE_LEVEL <= ELIB_LOG_LEVEL_ERROR)
#define ELIB_LOGE(ctx, head, ...)                                             \
    ((void)elib_log_write((ctx), ELIB_LOG_ERROR, (head), __VA_ARGS__))
#else
#define ELIB_LOGE(ctx, head, ...)   ((void)0)
#endif

#if (ELIB_LOG_COMPILE_LEVEL <= ELIB_LOG_LEVEL_WARNING)
#define ELIB_LOGW(ctx, head, ...)                                             \
    ((void)elib_log_write((ctx), ELIB_LOG_WARNING, (head), __VA_ARGS__))
#else
#define ELIB_LOGW(ctx, head, ...)   ((void)0)
#endif

#if (ELIB_LOG_COMPILE_LEVEL <= ELIB_LOG_LEVEL_INFO)
#define ELIB_LOGI(ctx, head, ...)                                             \
    ((void)elib_log_write((ctx), ELIB_LOG_INFO, (head), __VA_ARGS__))
#else
#define ELIB_LOGI(ctx, head, ...)   ((void)0)
#endif

#if (ELIB_LOG_COMPILE_LEVEL <= ELIB_LOG_LEVEL_DEBUG)
#define ELIB_LOGD(ctx, head, ...)                                             \
    ((void)elib_log_write((ctx), ELIB_LOG_DEBUG, (head), __VA_ARGS__))
#else
#define ELIB_LOGD(ctx, head, ...)   ((void)0)
#endif

/* The level argument of a hex dump is a runtime value, so this macro cannot be
 * culled at compile time; elib_log_hex() applies the runtime filter instead */
#define ELIB_LOG_HEX(ctx, level, head, data, len)                             \
    ((void)elib_log_hex((ctx), (level), (head), (data), (len)))

#ifdef __cplusplus
}
#endif

#endif /* ELIB_LOG_H */
