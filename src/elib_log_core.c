/* elib_log_core.c - Log Module Core Implementation */

#include "elib_log_core.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static const char *level_colors[] = {
    "\033[31m",  /* ERROR   red    */
    "\033[33m",  /* WARNING yellow */
    "\033[32m",  /* INFO    green  */
    "\033[90m",  /* DEBUG   gray   */
};

static const char *color_reset = "\033[0m";
#define COLOR_RESET_LEN 4

static uint16_t buf_append(char *buf, uint16_t buf_size, uint16_t pos,
                           uint16_t reserve, const char *fmt, ...)
{
    uint16_t avail = (buf_size > reserve) ? (buf_size - reserve) : 0;
    if (pos >= avail) {
        return pos;
    }
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + pos, avail - pos, fmt, ap);
    va_end(ap);
    if (n < 0) {
        return pos;
    }
    if ((uint16_t)n >= avail - pos) {
        return avail;
    }
    return pos + (uint16_t)n;
}

elib_log_err_t elib_log_init(elib_log_ctx_t *ctx, const elib_log_cfg_t *cfg)
{
    if (ctx == NULL || cfg == NULL) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    if (cfg->output == NULL || cfg->fmt_buf == NULL || cfg->fmt_buf_size == 0) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    if (cfg->fmt_buf_size < 8) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }

    memset(ctx, 0, sizeof(elib_log_ctx_t));
    ctx->cfg = cfg;
    ctx->min_level = ELIB_LOG_INFO;
    ctx->bit_flags.initialized = 1;

    return ELIB_LOG_OK;
}

void elib_log_deinit(elib_log_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->bit_flags.initialized = 0;
}

elib_log_err_t elib_log_log(elib_log_ctx_t *ctx, elib_log_level_t level,
                            const char *head, const char *fmt, ...)
{
    if (ctx == NULL) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_LOG_ERR_NOT_INITIALIZED;
    }
    if (level < ctx->min_level) {
        return ELIB_LOG_OK;
    }

    char *buf = ctx->cfg->fmt_buf;
    uint16_t size = ctx->cfg->fmt_buf_size;
    uint16_t pos = 0;

    pos = buf_append(buf, size, pos, COLOR_RESET_LEN, "\r\n");
    if ((int)level >= 0 && (int)level <= ELIB_LOG_DEBUG) {
        pos = buf_append(buf, size, pos, COLOR_RESET_LEN, "%s", level_colors[level]);
    }

    if (ctx->cfg->timestamp != NULL) {
        elib_log_timestamp_t ts = ctx->cfg->timestamp();
        pos = buf_append(buf, size, pos, COLOR_RESET_LEN, "[%lu.%03lu]",
                         (unsigned long)ts.s, (unsigned long)ts.ms);
    }
    if (head != NULL) {
        pos = buf_append(buf, size, pos, COLOR_RESET_LEN, "[%s]:", head);
    }

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + pos, (size - COLOR_RESET_LEN > pos) ? size - COLOR_RESET_LEN - pos : 0, fmt, ap);
    va_end(ap);
    if (n > 0 && (uint16_t)n < size - COLOR_RESET_LEN - pos) {
        pos += (uint16_t)n;
    } else if (n > 0) {
        pos = size - COLOR_RESET_LEN;
    }

    memcpy(buf + pos, color_reset, COLOR_RESET_LEN);
    buf[pos + COLOR_RESET_LEN] = '\0';

    ctx->cfg->output(buf);
    return ELIB_LOG_OK;
}

elib_log_err_t elib_log_hex(elib_log_ctx_t *ctx, elib_log_level_t level,
                            const char *head, const uint8_t *data, uint16_t len)
{
    if (ctx == NULL) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_LOG_ERR_NOT_INITIALIZED;
    }
    if (len > 0 && data == NULL) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    if (level < ctx->min_level) {
        return ELIB_LOG_OK;
    }

    char *buf = ctx->cfg->fmt_buf;
    uint16_t size = ctx->cfg->fmt_buf_size;
    uint16_t pos = 0;

    pos = buf_append(buf, size, pos, COLOR_RESET_LEN, "\r\n");
    if ((int)level >= 0 && (int)level <= ELIB_LOG_DEBUG) {
        pos = buf_append(buf, size, pos, COLOR_RESET_LEN, "%s", level_colors[level]);
    }

    if (ctx->cfg->timestamp != NULL) {
        elib_log_timestamp_t ts = ctx->cfg->timestamp();
        pos = buf_append(buf, size, pos, COLOR_RESET_LEN, "[%lu.%03lu]",
                         (unsigned long)ts.s, (unsigned long)ts.ms);
    }
    if (head != NULL) {
        pos = buf_append(buf, size, pos, COLOR_RESET_LEN, "[%s]:", head);
    }

    for (uint16_t i = 0; i < len; i++) {
        if (pos >= size - COLOR_RESET_LEN) {
            break;
        }
        if (i > 0) {
            pos = buf_append(buf, size, pos, COLOR_RESET_LEN, " ");
        }
        pos = buf_append(buf, size, pos, COLOR_RESET_LEN, "%02X", data[i]);
    }

    memcpy(buf + pos, color_reset, COLOR_RESET_LEN);
    buf[pos + COLOR_RESET_LEN] = '\0';

    ctx->cfg->output(buf);
    return ELIB_LOG_OK;
}

elib_log_err_t elib_log_set_level(elib_log_ctx_t *ctx, elib_log_level_t min_level)
{
    if (ctx == NULL) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_LOG_ERR_NOT_INITIALIZED;
    }
    if ((int)min_level < 0 || (int)min_level > ELIB_LOG_DEBUG) {
        return ELIB_LOG_ERR_INVALID_PARAM;
    }
    ctx->min_level = min_level;
    return ELIB_LOG_OK;
}

elib_log_level_t elib_log_get_level(elib_log_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->bit_flags.initialized) {
        return ELIB_LOG_ERROR;
    }
    return ctx->min_level;
}
