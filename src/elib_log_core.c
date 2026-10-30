/* ============================================================================
 * elib_log_core.c --- elib log implementation (v2)
 * ============================================================================
 * The single source file; includes the umbrella header directly. The semantic
 * contract lives in the header banner of elib_log.h:
 *   - one call assembles the complete line in the caller buffer and invokes the
 *     output callback exactly once;
 *   - the tail (colour reset plus line terminator) is reserved up front, so the
 *     buffer is never overrun and the line is always NUL-terminated;
 *   - truncation is reported, never silent;
 *   - assembly is fenced by the optional critical hooks, callbacks run outside.
 * ==========================================================================*/
#include "elib_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ constants */

/* Indexed by the validated level value (0 = DEBUG ... 3 = ERROR) */
static const char *const log_level_colors[4] = {
    "\033[90m",   /* DEBUG   gray   */
    "\033[32m",   /* INFO    green  */
    "\033[33m",   /* WARNING yellow */
    "\033[31m",   /* ERROR   red    */
};

#define LOG_COLOR_RESET      "\033[0m"
#define LOG_COLOR_RESET_LEN  4u
#define LOG_COLOR_OPEN_LEN   5u
#define LOG_HEX_DIGITS       "0123456789ABCDEF"

/* ------------------------------------------------------------------ writer */

/* Bounded writer over the assembled line. cap is the position limit, with the
 * reserved tail excluded, so the assembly can never claim the bytes it needs
 * later. */
typedef struct {
    char *buf;
    uint32_t cap;                      /* position limit for prefix + content */
    uint32_t pos;                      /* bytes already placed */
    uint8_t truncated;                 /* set once something did not fit */
} log_writer_t;

static void log_putc(log_writer_t *w, char c) {
    if (w->pos < w->cap) {
        w->buf[w->pos] = c;
        w->pos++;
    } else {
        w->truncated = 1u;
    }
}

/* Format into the remaining assembly area. One extra byte is offered to
 * vsnprintf for its NUL; that byte sits inside the reserved tail region and is
 * overwritten by the tail afterwards (and it is always in bounds because
 * cap <= buf_size - tail_len - 1). */
static void log_vprintf(log_writer_t *w, const char *fmt, va_list ap) {
    uint32_t space = w->cap - w->pos;
    int n = vsnprintf(w->buf + w->pos, (size_t)space + 1u, fmt, ap);

    if (n < 0) {
        w->truncated = 1u;
        w->pos = w->cap;
        return;
    }
    if ((uint32_t)n > space) {
        w->truncated = 1u;
        w->pos = w->cap;
        return;
    }
    w->pos += (uint32_t)n;
}

/* Variadic front end for the prefix pieces */
static void log_printf(log_writer_t *w, const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    log_vprintf(w, fmt, ap);
    va_end(ap);
}

/* ------------------------------------------------------------------ helpers */

/* Level range check written as a switch so that no range comparison can be
 * optimised into a tautology warning on either compiler */
static int log_level_valid(elib_log_level_t level) {
    switch (level) {
    case ELIB_LOG_DEBUG:
    case ELIB_LOG_INFO:
    case ELIB_LOG_WARNING:
    case ELIB_LOG_ERROR:
        return 1;
    default:
        return 0;
    }
}

static uint32_t log_eol_len(uint32_t flags) {
    if ((flags & ELIB_LOG_FLAG_EOL_CRLF) != 0u) {
        return 2u;
    }
    if ((flags & ELIB_LOG_FLAG_EOL_LF) != 0u) {
        return 1u;
    }
    if ((flags & ELIB_LOG_FLAG_EOL_NONE) != 0u) {
        return 0u;
    }
    return 2u;                         /* documented default: CRLF */
}

static uint32_t log_tail_len(const elib_log_ctx_t *ctx) {
    uint32_t tail = log_eol_len(ctx->flags);

    if ((ctx->flags & ELIB_LOG_FLAG_COLOR) != 0u) {
        tail += LOG_COLOR_RESET_LEN;
    }
    return tail;
}

/* Length of the colour opener, which is written before the variable content */
static uint32_t log_open_len(const elib_log_ctx_t *ctx) {
    return ((ctx->flags & ELIB_LOG_FLAG_COLOR) != 0u) ? LOG_COLOR_OPEN_LEN : 0u;
}

/* Every byte the library writes unconditionally: opener, reset, terminator and
 * the NUL. A buffer must be able to hold all of them plus at least one content
 * byte, otherwise a coloured line could not be produced without splitting an
 * escape sequence. */
static uint32_t log_fixed_len(uint32_t flags) {
    uint32_t fixed = log_eol_len(flags) + 1u;          /* terminator + NUL */

    if ((flags & ELIB_LOG_FLAG_COLOR) != 0u) {
        fixed += LOG_COLOR_OPEN_LEN + LOG_COLOR_RESET_LEN;
    }
    return fixed;
}

/* Enter the optional user critical section (init guarantees a matched pair) */
static void log_lock(elib_log_ctx_t *ctx) {
    if (ctx->enter_critical != NULL) {
        ctx->enter_critical();
    }
}

/* Leave the optional user critical section */
static void log_unlock(elib_log_ctx_t *ctx) {
    if (ctx->exit_critical != NULL) {
        ctx->exit_critical();
    }
}

/* Common prefix: timestamp and head label. The colour opener is not written
 * here: it lives before the variable content and is copied in by log_enter. */
static void log_assemble_head(log_writer_t *w, elib_log_ctx_t *ctx,
                              const char *head) {
    if ((ctx->flags & ELIB_LOG_FLAG_TIMESTAMP) != 0u) {
        /* init rejects this flag without a callback, so the pointer is valid */
        elib_log_timestamp_t ts = ctx->timestamp(ctx->user_data);
        log_printf(w, "[%lu.%03lu]",
                   (unsigned long)ts.s, (unsigned long)ts.ms);
    }
    if (head != NULL) {
        log_printf(w, "[%s]:", head);
    }
}

/* Append the reserved tail, terminate and hand the line to the transport.
 * Returns TRUNCATED when the content did not fully fit. */
static elib_log_err_t log_emit(log_writer_t *w, elib_log_ctx_t *ctx) {
    uint8_t truncated = w->truncated;

    if ((ctx->flags & ELIB_LOG_FLAG_COLOR) != 0u) {
        memcpy(w->buf + w->pos, LOG_COLOR_RESET, LOG_COLOR_RESET_LEN);
        w->pos += LOG_COLOR_RESET_LEN;
    }
    if ((ctx->flags & ELIB_LOG_FLAG_EOL_NONE) != 0u) {
        /* no terminator requested */
    } else if ((ctx->flags & ELIB_LOG_FLAG_EOL_LF) != 0u) {
        w->buf[w->pos] = '\n';
        w->pos++;
    } else {
        /* EOL_CRLF, and the documented default when no EOL flag is given */
        w->buf[w->pos] = '\r';
        w->pos++;
        w->buf[w->pos] = '\n';
        w->pos++;
    }

    /* pos == content + tail <= (buf_size - tail - 1) + tail = buf_size - 1 */
    w->buf[w->pos] = '\0';

    if (ctx->output != NULL) {
        ctx->output(w->buf, ctx->user_data);
    }

    return (truncated != 0u) ? ELIB_LOG_ERR_TRUNCATED : ELIB_LOG_OK;
}

/* Shared entry gate: ctx, initialization, level, severity filter and the
 * concurrency guard. Returns OK with *run == 0 when the line is filtered out
 * (nothing must be written), and OK with *run == 1 after entering the critical
 * section, in which case the caller must unlock once the buffer work is done. */
static elib_log_err_t log_enter(elib_log_ctx_t *ctx, elib_log_level_t level,
                                log_writer_t *w, uint8_t *run) {
    *run = 0u;

    if (ctx == NULL) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_LOG_ERR_NOT_INITIALIZED;
    }
    if (!log_level_valid(level)) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    /* Filter first: a suppressed level costs no formatting and no locking */
    if ((unsigned int)level < (unsigned int)ctx->level) {
        return ELIB_LOG_OK;
    }

    log_lock(ctx);
    if (ctx->busy != 0u) {
        /* Another context is inside the output callback and still reads the
         * buffer, so this line cannot be assembled without corruption */
        log_unlock(ctx);
        return ELIB_LOG_ERR_BUSY;
    }
    ctx->busy = 1u;

    uint32_t open = log_open_len(ctx);
    w->buf = ctx->buf;
    /* cap is the position limit for the assembled prefix and content; the
     * opener already occupies [0, open) of that region */
    w->cap = ctx->buf_size - log_tail_len(ctx) - 1u;
    w->pos = open;
    w->truncated = 0u;

    if (open != 0u) {
        /* Reserved up front, so the escape sequence is never split by a cut */
        memcpy(ctx->buf, log_level_colors[level], open);
    }

    *run = 1u;
    return ELIB_LOG_OK;
}

/* Leave the assembly critical section. The busy flag stays set until the output
 * callback has returned, so the buffer is protected while it is being read. */
static void log_leave_output(elib_log_ctx_t *ctx) {
    ctx->busy = 0u;
}

/* ------------------------------------------------------------------ lifecycle */

elib_log_err_t elib_log_init(elib_log_ctx_t *ctx,
                             const elib_log_cfg_t *cfg,
                             void (*enter_critical)(void),
                             void (*exit_critical)(void)) {
    if (ctx == NULL || cfg == NULL) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    if (cfg->buf == NULL || cfg->buf_size < ELIB_LOG_MIN_BUF_SIZE) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    if (cfg->output == NULL) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    if (!log_level_valid(cfg->level)) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    /* A one-sided hook pair would leave the interrupt state inconsistent */
    if ((enter_critical == NULL) != (exit_critical == NULL)) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    /* Unknown flag bits would silently change the output contract */
    uint32_t known = ELIB_LOG_FLAG_COLOR | ELIB_LOG_FLAG_TIMESTAMP |
                     ELIB_LOG_FLAG_EOL_CRLF | ELIB_LOG_FLAG_EOL_LF |
                     ELIB_LOG_FLAG_EOL_NONE;
    if ((cfg->flags & ~known) != 0u) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    /* At most one line terminator */
    uint32_t eol_flags = cfg->flags & (ELIB_LOG_FLAG_EOL_CRLF |
                                       ELIB_LOG_FLAG_EOL_LF |
                                       ELIB_LOG_FLAG_EOL_NONE);
    if (eol_flags != 0u && (eol_flags & (eol_flags - 1u)) != 0u) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    /* A timestamp prefix without a clock cannot be produced */
    if ((cfg->flags & ELIB_LOG_FLAG_TIMESTAMP) != 0u &&
        cfg->timestamp == NULL) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    /* The unconditional part of the line (colour opener and reset plus the
     * terminator) must fit with at least one content byte to spare */
    if (cfg->buf_size < log_fixed_len(cfg->flags) + 1u) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }

    /* Unconditional re-initialization: the caller's historical state is irrelevant */
    memset(ctx, 0, sizeof(*ctx));

    ctx->buf = cfg->buf;
    ctx->buf_size = cfg->buf_size;
    ctx->output = cfg->output;
    ctx->timestamp = cfg->timestamp;
    ctx->user_data = cfg->user_data;
    ctx->flags = cfg->flags;
    ctx->enter_critical = enter_critical;
    ctx->exit_critical = exit_critical;
    ctx->level = cfg->level;
    ctx->busy = 0u;
    ctx->initialized = 1u;

    return ELIB_LOG_OK;
}

void elib_log_deinit(elib_log_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    /* Configuration is kept in place but becomes inaccessible because every
     * other API checks initialized */
    ctx->busy = 0u;
    ctx->initialized = 0u;
}

/* ------------------------------------------------------------------ output */

elib_log_err_t elib_log_write(elib_log_ctx_t *ctx,
                              elib_log_level_t level,
                              const char *head,
                              const char *fmt, ...) {
    if (fmt == NULL) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }

    log_writer_t w;
    uint8_t run = 0u;
    elib_log_err_t err = log_enter(ctx, level, &w, &run);
    if (err != ELIB_LOG_OK || run == 0u) {
        return err;                    /* filtered: nothing is written */
    }

    log_assemble_head(&w, ctx, head);

    va_list ap;
    va_start(ap, fmt);
    log_vprintf(&w, fmt, ap);
    va_end(ap);

    log_unlock(ctx);
    err = log_emit(&w, ctx);
    log_leave_output(ctx);

    return err;
}

elib_log_err_t elib_log_hex(elib_log_ctx_t *ctx,
                            elib_log_level_t level,
                            const char *head,
                            const uint8_t *data,
                            uint32_t len) {
    if (len > 0u && data == NULL) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }

    log_writer_t w;
    uint8_t run = 0u;
    elib_log_err_t err = log_enter(ctx, level, &w, &run);
    if (err != ELIB_LOG_OK || run == 0u) {
        return err;                    /* filtered: nothing is written */
    }

    log_assemble_head(&w, ctx, head);

    for (uint32_t i = 0u; i < len; i++) {
        if (i > 0u) {
            log_putc(&w, ' ');
        }
        log_putc(&w, LOG_HEX_DIGITS[(data[i] >> 4) & 0x0Fu]);
        log_putc(&w, LOG_HEX_DIGITS[data[i] & 0x0Fu]);
        if (w.truncated != 0u) {
            /* Nothing further can fit; stop formatting the remaining bytes */
            break;
        }
    }

    log_unlock(ctx);
    err = log_emit(&w, ctx);
    log_leave_output(ctx);

    return err;
}

/* ------------------------------------------------------------------ filtering */

elib_log_err_t elib_log_set_level(elib_log_ctx_t *ctx, elib_log_level_t level) {
    if (ctx == NULL) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_LOG_ERR_NOT_INITIALIZED;
    }
    if (!log_level_valid(level)) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }

    ctx->level = level;

    return ELIB_LOG_OK;
}

elib_log_err_t elib_log_get_level(elib_log_ctx_t *ctx,
                                  elib_log_level_t *out_level) {
    if (ctx == NULL || out_level == NULL) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_LOG_ERR_NOT_INITIALIZED;
    }

    *out_level = ctx->level;

    return ELIB_LOG_OK;
}
