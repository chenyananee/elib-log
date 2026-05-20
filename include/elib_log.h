/* elib_log.h - Log Module Public API */

#ifndef ELIB_LOG_H
#define ELIB_LOG_H

#include "elib_log_err.h"
#include "elib_log_types.h"

#ifdef __cplusplus
extern "C" {
#endif

elib_log_err_t  elib_log_init(elib_log_ctx_t *ctx, const elib_log_cfg_t *cfg);
void            elib_log_deinit(elib_log_ctx_t *ctx);
elib_log_err_t  elib_log_log(elib_log_ctx_t *ctx, elib_log_level_t level,
                             const char *head, const char *fmt, ...);
elib_log_err_t  elib_log_hex(elib_log_ctx_t *ctx, elib_log_level_t level,
                             const char *head, const uint8_t *data, uint16_t len);
elib_log_err_t  elib_log_set_level(elib_log_ctx_t *ctx, elib_log_level_t min_level);
elib_log_level_t elib_log_get_level(elib_log_ctx_t *ctx);

#define ELIB_LOGE(ctx, head, fmt, ...) \
    elib_log_log(ctx, ELIB_LOG_ERROR, head, fmt, ##__VA_ARGS__)

#define ELIB_LOGW(ctx, head, fmt, ...) \
    elib_log_log(ctx, ELIB_LOG_WARNING, head, fmt, ##__VA_ARGS__)

#define ELIB_LOGI(ctx, head, fmt, ...) \
    elib_log_log(ctx, ELIB_LOG_INFO, head, fmt, ##__VA_ARGS__)

#define ELIB_LOGD(ctx, head, fmt, ...) \
    elib_log_log(ctx, ELIB_LOG_DEBUG, head, fmt, ##__VA_ARGS__)

#define ELIB_LOG_HEX(ctx, level, head, data, len) \
    elib_log_hex(ctx, level, head, data, len)

#ifdef __cplusplus
}
#endif

#endif /* ELIB_LOG_H */
