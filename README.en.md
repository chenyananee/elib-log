[简体中文](README.md) | **English**

# elib-log

A zero-dynamic-memory logging library for resource-constrained embedded systems: you supply the context and the formatting buffer statically, pure C99, single umbrella-header API.

Current version: **2.0.0** (`ELIB_LOG_VERSION`), single header `include/elib_log.h`.

## Features

- **Zero dynamic memory / no globals**: both `ctx` and the formatting buffer are user-supplied static storage; the library holds no state of its own
- **Single umbrella-header API**: all public types and functions live in `elib_log.h`; no sub-headers
- **One assembly, one output**: the whole line is assembled in your buffer before `output` is called, so the transport layer never sees half a line
- **Never out of bounds**: color escape sequences and the line ending are reserved at `init` time at a fixed cost; content can only use the remaining space and can never be written past `buf_size`
- **Observable truncation**: when the content does not fit, `ELIB_LOG_ERR_TRUNCATED` is returned, the line ending is preserved, and escape sequences are never cut
- **Intuitive level semantics**: severity increases with value (`DEBUG=0 … ERROR=3`); `level` is the "lowest severity that passes"
- **Color is decided per ctx**: enabled explicitly with `ELIB_LOG_FLAG_COLOR`; by default no ANSI escape is emitted at all
- **Selectable line ending**: CRLF by default; LF or "no line ending" available
- **Optional timestamp**: `[seconds.milliseconds]` prefix, with the clock callback supplied by you
- **Optional critical-section hooks**: `init` rejects a one-sided hook; `output` runs outside the critical section
- **Reentrancy guard**: logging again inside the `output` callback returns `ELIB_LOG_ERR_BUSY` and does not corrupt the buffer
- **Compile-time trimming**: `ELIB_LOG_COMPILE_LEVEL` makes lower-level macros, together with their arguments, disappear from the binary
- **Pure C99 implementation**, C++ compatible (`extern "C"`)

## Quick Start

### 1. Statically Define Buffers and Context

```c
#include "elib_log.h"   /* add -I<repo>/include at build time */

static char            g_log_buf[256];
static elib_log_ctx_t  g_log_ctx;

static void log_output(const char *str, void *user_data)
{
    (void)user_data;
    /* str is valid only for the duration of this call: copy before
     * sending asynchronously. Here the whole line goes to UART / RTT / a file */
    uart_write_line(str);
}

static elib_log_timestamp_t log_clock(void *user_data)
{
    (void)user_data;
    elib_log_timestamp_t ts;
    ts.s  = g_uptime_ms / 1000u;
    ts.ms = g_uptime_ms % 1000u;
    return ts;
}

static void app_log_init(void)
{
    elib_log_cfg_t cfg = {
        .buf       = g_log_buf,
        .buf_size  = sizeof(g_log_buf),
        .output    = log_output,          /* required */
        .timestamp = log_clock,           /* optional, may be NULL */
        .user_data = NULL,                /* passed through to both callbacks */
        .level     = ELIB_LOG_INFO,       /* emit only INFO / WARNING / ERROR */
        .flags     = ELIB_LOG_FLAG_EOL_CRLF,   /* see "Layout Flags" */
    };

    /* cfg is copied by value and may live on the stack; the buffer must outlive the context.
     * For hooks see "Concurrency Model"; with a single context pass NULL, NULL */
    elib_log_init(&g_log_ctx, &cfg, crit_enter, crit_exit);
}
```

### 2. Log

```c
ELIB_LOGI(&g_log_ctx, "Sensor", "temperature is %d.%01d C", t_int, t_frac);
ELIB_LOGW(&g_log_ctx, "Comm",   "retry %u of %u", n, max);
ELIB_LOGE(&g_log_ctx, "Flash",  "erase failed at 0x%08lX", (unsigned long)addr);
ELIB_LOGD(&g_log_ctx, "Loop",   "tick %lu", (unsigned long)tick);

/* Without the convenience macros, use the raw interface; the level is a runtime value */
elib_log_write(&g_log_ctx, ELIB_LOG_INFO, "Boot", "version %s", fw_version);

/* Hex dump: uppercase, single-space separated, no trailing space */
ELIB_LOG_HEX(&g_log_ctx, ELIB_LOG_INFO, "Sensor", payload, payload_len);
```

Output (default CRLF, color off):

```
[Sensor]:temperature is 23.6 C
[Comm]:retry 2 of 5
[Sensor]:02 89 A6 FF 5E
```

### 3. Adjust the Level at Runtime

```c
elib_log_set_level(&g_log_ctx, ELIB_LOG_DEBUG);    /* everything on */
elib_log_set_level(&g_log_ctx, ELIB_LOG_ERROR);    /* keep only ERROR */

elib_log_level_t level;
if (elib_log_get_level(&g_log_ctx, &level) == ELIB_LOG_OK) {
    /* ... */
}
```

### 4. Deinitialize

```c
elib_log_deinit(&g_log_ctx);   /* afterwards every API except init returns NOT_INITIALIZED */
```

## API Overview

| Group | Functions | Description |
|-------|-----------|-------------|
| Lifetime | `elib_log_init(ctx, cfg, enter_critical, exit_critical)` | Initialize (memsets ctx; copies cfg by value; rejects illegal configurations) |
| | `elib_log_deinit(ctx)` | Deinitialize (NULL-safe; afterwards everything except init is rejected) |
| Output | `elib_log_write(ctx, level, head, fmt, ...)` | Format and emit one line (the prefix is omitted when `head` is NULL) |
| | `elib_log_hex(ctx, level, head, data, len)` | Hex-dump one line |
| Filtering | `elib_log_set_level(ctx, level)` | Set the lowest severity that passes |
| | `elib_log_get_level(ctx, &level)` | Read the current threshold |
| Macros | `ELIB_LOGE / ELIB_LOGW / ELIB_LOGI / ELIB_LOGD(ctx, head, ...)` | Convenience macros; `fmt` is required (C99 variadic) |
| | `ELIB_LOG_HEX(ctx, level, head, data, len)` | Hex convenience macro (the level is a runtime value; no compile-time trimming) |

Error codes (5, `elib_log_err_t`):

| Error code | Meaning |
|------------|---------|
| `ELIB_LOG_OK` | Success; **also covers lines filtered out by level** (nothing was written) |
| `ELIB_LOG_ERR_INVALID_PARAM` | NULL pointer, illegal level, illegal flag combination, buffer too small |
| `ELIB_LOG_ERR_NOT_INITIALIZED` | Not `init`ed, or already `deinit`ed |
| `ELIB_LOG_ERR_TRUNCATED` | Content did not fit; the best possible output was emitted (line ending intact) |
| `ELIB_LOG_ERR_BUSY` | The previous line is still inside the `output` callback and the buffer is still being read |

## Output Layout

```
[<ESC>color m][seconds.milliseconds][HEAD]:body[<ESC>0m]<line ending>
```

| Fragment | When it appears | Description |
|----------|-----------------|-------------|
| Color on/off | `ELIB_LOG_FLAG_COLOR` | Decided by the ctx at `init` time, **not** a per-call parameter |
| `[seconds.milliseconds]` | `ELIB_LOG_FLAG_TIMESTAMP` | Milliseconds are always zero-padded to three digits, e.g. `[7.005]`, `[0.000]` |
| `[HEAD]:` | `head != NULL` | `head` must be a NUL-terminated string; passing `""` yields `[]:` |
| Body | Always | `printf` style; `elib_log_hex` is uppercase, single-space separated |
| Line ending | Decided by the EOL flag | CRLF by default |

Byte-by-byte sample (`head = "T"`, body `hi`):

| Configuration | Actual bytes |
|---------------|--------------|
| no flag | `[T]:hi` `0D 0A` |
| `EOL_LF` | `[T]:hi` `0A` |
| `EOL_NONE` | `[T]:hi` |
| `COLOR` + INFO | `1B 5B 33 32 6D` `[T]:hi` `1B 5B 30 6D` `0D 0A` |
| `COLOR` + ERROR + `EOL_LF` | `1B 5B 33 31 6D` `[T]:hi` `1B 5B 30 6D` `0A` |
| `TIMESTAMP` (7s005ms) | `[7.005][T]:hi` `0D 0A` |

Color table (only effective under `ELIB_LOG_FLAG_COLOR`):

| Level | Sequence |
|-------|----------|
| DEBUG | `ESC[90m` (gray) |
| INFO | `ESC[32m` (green) |
| WARNING | `ESC[33m` (yellow) |
| ERROR | `ESC[31m` (red) |

### Layout Flags (`cfg.flags`)

| Flag | Effect |
|------|--------|
| `ELIB_LOG_FLAG_COLOR` | Emit ANSI color; when omitted, no escape bytes are emitted at all, which suits log files and non-ANSI terminals |
| `ELIB_LOG_FLAG_TIMESTAMP` | Emit the timestamp prefix; a `timestamp` callback is **required**, otherwise `init` rejects the config |
| `ELIB_LOG_FLAG_EOL_CRLF` | CRLF line ending (also the default when no EOL flag is given) |
| `ELIB_LOG_FLAG_EOL_LF` | LF line ending |
| `ELIB_LOG_FLAG_EOL_NONE` | No line ending |

The three EOL flags are **mutually exclusive**; supplying two or more makes `init` reject the config, and unknown flag bits are rejected as well.

## Level Semantics

Severity rises with the value: `DEBUG(0) < INFO(1) < WARNING(2) < ERROR(3)`. `level` is the **lowest severity that passes**.

| Threshold | DEBUG | INFO | WARNING | ERROR |
|-----------|-------|------|---------|-------|
| `ELIB_LOG_DEBUG` | ✅ | ✅ | ✅ | ✅ |
| `ELIB_LOG_INFO` (recommended default) | — | ✅ | ✅ | ✅ |
| `ELIB_LOG_WARNING` | — | — | ✅ | ✅ |
| `ELIB_LOG_ERROR` | — | — | — | ✅ |

- When the threshold is `X`, lines whose level **equals** `X` are still emitted
- Filtered lines return `ELIB_LOG_OK` and are **not formatted, not locked, do not touch the buffer, and do not read the clock**
- Out-of-range levels (`(elib_log_level_t)9`, `-1`) return `ELIB_LOG_ERR_INVALID_PARAM`; they are not treated as "filtered"

### Compile-Time Trimming

```c
#define ELIB_LOG_COMPILE_LEVEL ELIB_LOG_LEVEL_WARNING   /* or 2 */
#include "elib_log.h"
```

`ELIB_LOGD` / `ELIB_LOGI` expand to `((void)0)`, **not even evaluating their arguments**, which is ideal for removing debug expressions from the binary entirely:

```c
ELIB_LOGD(&ctx, "Loop", "state=%s cost=%lu", state_name(s), measure_cost());
/* Below the compile-time threshold neither state_name nor measure_cost is called */
```

The default is `ELIB_LOG_LEVEL_DEBUG` (everything on). Compile-time trimming and the runtime threshold can be stacked.

## Truncation

Fixed costs (color on/off + line ending + NUL) are reserved at `init` time; content can only use what remains:

```
available body space = buf_size - (COLOR ? 9 : 0) - line-ending bytes - 1
```

Therefore:

- Truncation **can only cut content**; color escape sequences are never split in half (no orphan `ESC` is left behind)
- The line ending and the NUL are always intact: `strlen(str) <= buf_size - 1`
- `ELIB_LOG_ERR_TRUNCATED` is returned, so the caller can degrade (for example enlarge the buffer or shorten `head`)
- An exact fit returns `ELIB_LOG_OK` (using exactly the available space is not truncation)
- `init` rejects buffers that cannot hold even "fixed cost + 1 content byte":

| Configuration | Minimum usable buffer |
|---------------|-----------------------|
| color off + CRLF | 8 (`ELIB_LOG_MIN_BUF_SIZE` is also 8) |
| color off + LF | 8 |
| color off + no line ending | 8 |
| color on + CRLF | 13 |
| color on + LF | 12 |
| color on + no line ending | 11 |

Hex dump follows the same rules: it stops when space runs out, returns `TRUNCATED`, and never reads past `data`.

## Concurrency Model (ISR + Main Loop)

The formatting buffer is shared, so the whole line is assembled inside the **optional critical-section hooks**, while the `output` callback runs **outside the critical section**:

```c
/* Example: save/restore PRIMASK on Cortex-M */
static uint32_t g_crit_save;

static void crit_enter(void)
{
    g_crit_save = __get_PRIMASK();
    __disable_irq();
}

static void crit_exit(void)
{
    __set_PRIMASK(g_crit_save);
}

elib_log_init(&g_log_ctx, &cfg, crit_enter, crit_exit);
```

| Rule | Description |
|------|-------------|
| Hooks must be paired | Supplying only one makes `init` reject the config (`INVALID_PARAM`) |
| Hooks must nest | Use save/restore interrupt masking, not an unconditional re-enable |
| The clock callback runs inside the critical section | It is part of assembly and must be short and non-blocking |
| `output` runs outside the critical section | It may do slower work, but must not log again |
| Reentrancy → `BUSY` | Logging inside `output`, or another context logging while this one is outputting, returns `BUSY` and that line is dropped |
| `set_level` / `get_level` are always available | They do not touch the buffer, so they are safe inside the `output` callback |
| With a single context pass `NULL, NULL` | This declares "no concurrency"; there is no extra overhead |

> **How to log from an ISR**: treat `BUSY` as "this line was lost" rather than "wait a while". Logging must never block an interrupt; if a line must not be lost, have the ISR set a flag and let the main loop log it.

## Callback Contract

```c
typedef void (*elib_log_output_fn)(const char *str, void *user_data);
typedef elib_log_timestamp_t (*elib_log_timestamp_fn)(void *user_data);
```

- `str` points to the **library-reused user buffer** and is valid only for the duration of the call: copy it before sending asynchronously
- `user_data` is passed through from the cfg given to `init`; both callbacks receive the same pointer
- `output` is required, `timestamp` is optional; a supplied `timestamp` is not called unless the `TIMESTAMP` flag is set
- Each line calls `output` exactly once and reads `timestamp` exactly once

## Lifetime and Memory

- `init` **unconditionally** memsets `ctx`: any previous state can be reinitialized, and `busy` is cleared too
- `cfg` is a **value struct**: copied field by field, no pointers retained, so `cfg` can live on the stack
- The buffer pointed to by `cfg.buf` must **outlive** the context (its lifetime ≥ the context's); the library does not zero it
- `head` must be a NUL-terminated string whose lifetime covers the call; the library does not validate its length
- `deinit` only clears `initialized` (and conveniently `busy`); configuration and buffer stay where they are but are no longer usable
- Re-running `init` replaces flags / level / callbacks / hooks wholesale

## On `%f` and "Zero Dynamic Memory"

The library itself performs no allocation, but the body is produced by `vsnprintf()`. On newlib, floating-point conversion (`%f`) goes `_dtoa_r()` → `_Balloc()` → `_malloc_r()` and **does allocate on the heap**; integer, character, and string conversions do not allocate.

If the application must be strictly allocation-free (for example an MCU project that forbids the heap), **do not pass floating-point values to this library**; project them to integers or strings yourself:

```c
/* Don't: ELIB_LOGI(&ctx, "T", "%.3f", value); */
int32_t milli = (int32_t)(value * 1000.0f);
ELIB_LOGI(&ctx, "T", "%ld.%03ld", (long)(milli / 1000), (long)(milli % 1000));
```

## Building

Add `include/` to the header search path and compile the source as an ordinary C99 object file:

```bash
arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -Iinclude \
    -c src/elib_log_core.c -o elib_log_core.o
```

The source file includes the umbrella header directly (`#include "elib_log.h"`), depends on no relative paths, and ships no build system of its own.

## Testing

`test/` holds 11 independent test files (112 cases in total); each has its own `main()` and can be compiled and run separately:

```bash
for t in test/test_*.c; do
    gcc -std=c99 -Wall -Wextra -Werror -Wpedantic -Iinclude \
        "$t" src/elib_log_core.c -o "${t%.c}.exe"
    "./${t%.c}.exe"
done
```

| File | Coverage |
|------|----------|
| `test_init.c` | Version macros, cfg copied field by field, stack cfg may be destroyed, rejection of illegal configurations (insufficient fixed cost, EOL conflicts, unknown flags, timestamp without clock), reinitialization from a dirty ctx, sentinel checks that writes never cross `buf_size` |
| `test_level.c` | 4×4 filter matrix, zero side effects for filtered lines, the threshold itself passes, hex is filtered the same way, illegal levels report an error instead of being silently dropped, macros go through the same filtering |
| `test_write_format.c` | Three shapes of `head`, printf conversions, one line per call, empty format, NUL termination |
| `test_write_flags.c` | CRLF / LF / NONE defaults, per-level color and reset, timestamp zero-padding across the full uint32 range, all flag combinations, zero escape bytes when no flag is set, the clock is not read when the flag is absent, hex uses the same layout |
| `test_truncation.c` | Truncation points for each flag combination, line ending preserved, escapes not cut, exact fit vs. one byte over, minimum colored buffer, rejection when the fixed cost cannot fit, repeated truncation always stays in bounds |
| `test_hex.c` | Documented sample `02 89 A6 FF 5E`, uppercase and single space, a single byte, zero length, `data` NULL with `len>0`, `len` above 65535, one output per dump |
| `test_callbacks.c` | `user_data` for both callbacks, the clock runs before output, one clock read per line, filtered lines do not read the clock, `output` receives the user buffer, two contexts do not interfere |
| `test_critical.c` | Hooks stored in pairs, enter/exit balanced for write/hex/truncated lines, `output` outside the critical section, the clock inside it, filtered lines take no lock |
| `test_reentrancy.c` | Logging inside the callback returns `BUSY`, the outer line is not corrupted, the guard is released afterwards, the level can be queried inside the callback, filtering happens before the guard (returns OK) |
| `test_level_api.c` | set/get round-trip, out-of-range rejected while the old value is kept, with `ELIB_LOG_COMPILE_LEVEL = WARNING` DEBUG/INFO and their arguments are compiled out, runtime threshold stacks |
| `test_lifecycle.c` | `deinit` is NULL-safe, clears `busy`, everything is rejected after death, the initialization check precedes filtering, re-init revives and replaces the configuration |

Behavior consistency is additionally cross-checked against an independent Python reference model: **222409** assertions, including

- The exact line content asserted case by case in the C tests (expected `strcmp` values recomputed independently)
- Full cross-checking of the two independent assembly algorithms (string slicing vs. a character-by-character bounded writer) over `buf_size 8..40 × 10 flag sets × 4 levels × 4 head shapes × 6 body lengths`
- Invariants: `len(line) <= buf_size - 1`, truncated lines are always full, non-truncated lines have complete bodies, color escapes are always complete pairs, zero `ESC` bytes when color is off
- `init` acceptance boundaries and the filter matrix

Result: `TOTAL FAILS: 0`.

## License

MIT License (see `LICENSE`).
