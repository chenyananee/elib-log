/* elib_log_types.h - Log Module Type Definitions */

#ifndef ELIB_LOG_TYPES_H
#define ELIB_LOG_TYPES_H

#include <stdint.h>
#include "elib_log_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ELIB_LOG_ERROR = 0,
    ELIB_LOG_WARNING,
    ELIB_LOG_INFO,
    ELIB_LOG_DEBUG,
} elib_log_level_t;

typedef struct {
    uint32_t s;     /* seconds */
    uint32_t ms;    /* milliseconds (0-999) */
} elib_log_timestamp_t;

typedef void (*elib_log_output_fn)(const char *str);
typedef elib_log_timestamp_t (*elib_log_timestamp_fn)(void);

typedef struct {
    char                  *fmt_buf;
    uint16_t               fmt_buf_size;
    elib_log_output_fn     output;
    elib_log_timestamp_fn  timestamp;
} elib_log_cfg_t;

typedef struct {
    const elib_log_cfg_t *cfg;
    elib_log_level_t      min_level;
    int                   initialized;
} elib_log_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* ELIB_LOG_TYPES_H */
