/* test_elib_log.c - Log Module Unit Tests */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_log.h"

/* ---- Mock infrastructure ---- */

static char mock_buf[4096];
static uint16_t mock_pos;
static int mock_output_call_count;

static void mock_output(const char *str)
{
    uint16_t len = (uint16_t)strlen(str);
    memcpy(mock_buf + mock_pos, str, len);
    mock_pos += len;
    mock_buf[mock_pos] = '\0';
    mock_output_call_count++;
}

static elib_log_timestamp_t mock_timestamp(void)
{
    elib_log_timestamp_t ts = { .s = 12323, .ms = 678 };
    return ts;
}

static void mock_reset(void)
{
    mock_buf[0] = '\0';
    mock_pos = 0;
    mock_output_call_count = 0;
}

static char fmt_buf[256];

#define RUN_TEST(fn) do { \
    mock_reset(); \
    fn(); \
    printf("  PASS: %s\n", #fn); \
} while(0)

/* ---- Init/Deinit tests ---- */

static void test_init_valid(void)
{
    elib_log_ctx_t ctx;
    elib_log_cfg_t cfg = {
        .fmt_buf = fmt_buf,
        .fmt_buf_size = sizeof(fmt_buf),
        .output = mock_output,
        .timestamp = NULL,
    };
    assert(elib_log_init(&ctx, &cfg) == ELIB_LOG_OK);
    assert(ctx.bit_flags.initialized == 1);
    assert(ctx.min_level == ELIB_LOG_INFO);
    elib_log_deinit(&ctx);
}

static void test_init_null_ctx(void)
{
    elib_log_cfg_t cfg = {
        .fmt_buf = fmt_buf,
        .fmt_buf_size = sizeof(fmt_buf),
        .output = mock_output,
        .timestamp = NULL,
    };
    assert(elib_log_init(NULL, &cfg) == ELIB_LOG_ERR_INVALID_PARAM);
}

static void test_init_null_cfg(void)
{
    elib_log_ctx_t ctx;
    assert(elib_log_init(&ctx, NULL) == ELIB_LOG_ERR_INVALID_PARAM);
}

static void test_init_null_output(void)
{
    elib_log_ctx_t ctx;
    elib_log_cfg_t cfg = {
        .fmt_buf = fmt_buf,
        .fmt_buf_size = sizeof(fmt_buf),
        .output = NULL,
        .timestamp = NULL,
    };
    assert(elib_log_init(&ctx, &cfg) == ELIB_LOG_ERR_INVALID_PARAM);
}

static void test_init_null_fmt_buf(void)
{
    elib_log_ctx_t ctx;
    elib_log_cfg_t cfg = {
        .fmt_buf = NULL,
        .fmt_buf_size = 100,
        .output = mock_output,
        .timestamp = NULL,
    };
    assert(elib_log_init(&ctx, &cfg) == ELIB_LOG_ERR_INVALID_PARAM);
}

static void test_init_zero_buf_size(void)
{
    elib_log_ctx_t ctx;
    char buf[10];
    elib_log_cfg_t cfg = {
        .fmt_buf = buf,
        .fmt_buf_size = 0,
        .output = mock_output,
        .timestamp = NULL,
    };
    assert(elib_log_init(&ctx, &cfg) == ELIB_LOG_ERR_INVALID_PARAM);
}

static void test_init_small_buf(void)
{
    elib_log_ctx_t ctx;
    char buf[7];
    elib_log_cfg_t cfg = {
        .fmt_buf = buf,
        .fmt_buf_size = 7,
        .output = mock_output,
        .timestamp = NULL,
    };
    assert(elib_log_init(&ctx, &cfg) == ELIB_LOG_ERR_INVALID_PARAM);
}

/* ---- Deinit tests ---- */

static void test_deinit_valid(void)
{
    elib_log_ctx_t ctx;
    elib_log_cfg_t cfg = {
        .fmt_buf = fmt_buf,
        .fmt_buf_size = sizeof(fmt_buf),
        .output = mock_output,
        .timestamp = NULL,
    };
    elib_log_init(&ctx, &cfg);
    elib_log_deinit(&ctx);
    assert(ctx.bit_flags.initialized == 0);
}

static void test_deinit_null(void)
{
    elib_log_deinit(NULL);
}

/* ---- Not initialized tests ---- */

static void test_log_not_initialized(void)
{
    elib_log_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    assert(elib_log_log(&ctx, ELIB_LOG_INFO, "Tag", "hello") == ELIB_LOG_ERR_NOT_INITIALIZED);
}

static void test_hex_not_initialized(void)
{
    elib_log_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    uint8_t data[] = {0x01};
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, "Tag", data, 1) == ELIB_LOG_ERR_NOT_INITIALIZED);
}

static void test_set_level_not_initialized(void)
{
    elib_log_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    assert(elib_log_set_level(&ctx, ELIB_LOG_DEBUG) == ELIB_LOG_ERR_NOT_INITIALIZED);
}

/* ---- Log output tests ---- */

static elib_log_ctx_t g_ctx;
static void init_ctx_with_timestamp(void)
{
    elib_log_cfg_t cfg = {
        .fmt_buf = fmt_buf,
        .fmt_buf_size = sizeof(fmt_buf),
        .output = mock_output,
        .timestamp = mock_timestamp,
    };
    elib_log_init(&g_ctx, &cfg);
}

static void init_ctx_no_timestamp(void)
{
    elib_log_cfg_t cfg = {
        .fmt_buf = fmt_buf,
        .fmt_buf_size = sizeof(fmt_buf),
        .output = mock_output,
        .timestamp = NULL,
    };
    elib_log_init(&g_ctx, &cfg);
}

static void test_log_basic(void)
{
    init_ctx_no_timestamp();
    elib_log_log(&g_ctx, ELIB_LOG_INFO, "Sensor", "data is %d.%d", 23, 6);
    assert(strstr(mock_buf, "\r\n") == mock_buf);
    assert(strstr(mock_buf, "[Sensor]:data is 23.6") != NULL);
    assert(mock_output_call_count == 1);
    elib_log_deinit(&g_ctx);
}

static void test_log_with_timestamp(void)
{
    init_ctx_with_timestamp();
    elib_log_log(&g_ctx, ELIB_LOG_INFO, "Sensor", "hello");
    assert(strstr(mock_buf, "[12323.678]") != NULL);
    assert(strstr(mock_buf, "[Sensor]:hello") != NULL);
    assert(mock_output_call_count == 1);
    elib_log_deinit(&g_ctx);
}

static void test_log_without_timestamp(void)
{
    init_ctx_no_timestamp();
    elib_log_log(&g_ctx, ELIB_LOG_INFO, "Sensor", "test");
    assert(strstr(mock_buf, "[12323.678]") == NULL);
    assert(strstr(mock_buf, "[Sensor]:test") != NULL);
    assert(mock_output_call_count == 1);
    elib_log_deinit(&g_ctx);
}

static void test_log_level_color(void)
{
    init_ctx_no_timestamp();

    mock_reset();
    elib_log_log(&g_ctx, ELIB_LOG_ERROR, "T", "e");
    assert(strstr(mock_buf, "\033[31m") != NULL);

    mock_reset();
    elib_log_log(&g_ctx, ELIB_LOG_WARNING, "T", "w");
    assert(strstr(mock_buf, "\033[33m") != NULL);

    mock_reset();
    elib_log_log(&g_ctx, ELIB_LOG_INFO, "T", "i");
    assert(strstr(mock_buf, "\033[32m") != NULL);

    elib_log_set_level(&g_ctx, ELIB_LOG_DEBUG);
    mock_reset();
    elib_log_log(&g_ctx, ELIB_LOG_DEBUG, "T", "d");
    assert(strstr(mock_buf, "\033[90m") != NULL);

    elib_log_deinit(&g_ctx);
}

static void test_log_truncation(void)
{
    char small_buf[16];
    elib_log_cfg_t cfg = {
        .fmt_buf = small_buf,
        .fmt_buf_size = sizeof(small_buf),
        .output = mock_output,
        .timestamp = NULL,
    };
    elib_log_ctx_t ctx;
    elib_log_init(&ctx, &cfg);
    elib_log_log(&ctx, ELIB_LOG_INFO, "T", "long message truncated");
    /* output called once, content truncated but color_reset preserved */
    assert(mock_output_call_count == 1);
    assert(strstr(mock_buf, "\033[0m") != NULL);
    elib_log_deinit(&ctx);
}

/* ---- Level filter tests ---- */

static void test_log_level_filter(void)
{
    init_ctx_no_timestamp();

    elib_log_set_level(&g_ctx, ELIB_LOG_WARNING);

    mock_reset();
    elib_log_log(&g_ctx, ELIB_LOG_DEBUG, "T", "debug");
    assert(mock_pos == 0);

    mock_reset();
    elib_log_log(&g_ctx, ELIB_LOG_INFO, "T", "info");
    assert(mock_pos == 0);

    mock_reset();
    elib_log_log(&g_ctx, ELIB_LOG_WARNING, "T", "warn");
    assert(mock_pos > 0);
    assert(strstr(mock_buf, "[T]:warn") != NULL);

    mock_reset();
    elib_log_log(&g_ctx, ELIB_LOG_ERROR, "T", "err");
    assert(mock_pos > 0);
    assert(strstr(mock_buf, "[T]:err") != NULL);

    elib_log_deinit(&g_ctx);
}

static void test_set_level_valid(void)
{
    init_ctx_no_timestamp();
    assert(elib_log_set_level(&g_ctx, ELIB_LOG_ERROR) == ELIB_LOG_OK);
    assert(elib_log_get_level(&g_ctx) == ELIB_LOG_ERROR);
    assert(elib_log_set_level(&g_ctx, ELIB_LOG_WARNING) == ELIB_LOG_OK);
    assert(elib_log_get_level(&g_ctx) == ELIB_LOG_WARNING);
    assert(elib_log_set_level(&g_ctx, ELIB_LOG_INFO) == ELIB_LOG_OK);
    assert(elib_log_get_level(&g_ctx) == ELIB_LOG_INFO);
    assert(elib_log_set_level(&g_ctx, ELIB_LOG_DEBUG) == ELIB_LOG_OK);
    assert(elib_log_get_level(&g_ctx) == ELIB_LOG_DEBUG);
    elib_log_deinit(&g_ctx);
}

static void test_set_level_invalid(void)
{
    init_ctx_no_timestamp();
    assert(elib_log_set_level(&g_ctx, (elib_log_level_t)-1) == ELIB_LOG_ERR_INVALID_PARAM);
    assert(elib_log_set_level(&g_ctx, (elib_log_level_t)99) == ELIB_LOG_ERR_INVALID_PARAM);
    elib_log_deinit(&g_ctx);
}

static void test_get_level_not_initialized(void)
{
    elib_log_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    assert(elib_log_get_level(&ctx) == ELIB_LOG_ERROR);
}

static void test_get_level_null(void)
{
    assert(elib_log_get_level(NULL) == ELIB_LOG_ERROR);
}

/* ---- Hex dump tests ---- */

static void test_hex_basic(void)
{
    init_ctx_with_timestamp();
    uint8_t data[] = {0x02, 0x89, 0xA6, 0xFF, 0x5E};
    elib_log_hex(&g_ctx, ELIB_LOG_INFO, "Sensor", data, 5);
    assert(strstr(mock_buf, "[12323.678]") != NULL);
    assert(strstr(mock_buf, "[Sensor]:02 89 A6 FF 5E") != NULL);
    assert(mock_output_call_count == 1);
    elib_log_deinit(&g_ctx);
}

static void test_hex_empty(void)
{
    init_ctx_no_timestamp();
    elib_log_hex(&g_ctx, ELIB_LOG_INFO, "Tag", NULL, 0);
    assert(strstr(mock_buf, "[Tag]:") != NULL);
    assert(mock_output_call_count == 1);
    elib_log_deinit(&g_ctx);
}

static void test_hex_single_byte(void)
{
    init_ctx_no_timestamp();
    uint8_t data[] = {0xAB};
    elib_log_hex(&g_ctx, ELIB_LOG_INFO, "Tag", data, 1);
    assert(strstr(mock_buf, "[Tag]:AB") != NULL);
    assert(strstr(mock_buf, "[Tag]: AB") == NULL);
    elib_log_deinit(&g_ctx);
}

static void test_hex_with_timestamp(void)
{
    init_ctx_with_timestamp();
    uint8_t data[] = {0x02, 0x89};
    elib_log_hex(&g_ctx, ELIB_LOG_ERROR, "UART", data, 2);
    assert(strstr(mock_buf, "[12323.678]") != NULL);
    assert(strstr(mock_buf, "[UART]:02 89") != NULL);
    assert(strstr(mock_buf, "\033[31m") != NULL);
    elib_log_deinit(&g_ctx);
}

static void test_hex_level_filter(void)
{
    init_ctx_no_timestamp();
    elib_log_set_level(&g_ctx, ELIB_LOG_WARNING);
    uint8_t data[] = {0x01};
    mock_reset();
    elib_log_hex(&g_ctx, ELIB_LOG_DEBUG, "T", data, 1);
    assert(mock_pos == 0);
    elib_log_deinit(&g_ctx);
}

static void test_hex_null_data(void)
{
    init_ctx_no_timestamp();
    assert(elib_log_hex(&g_ctx, ELIB_LOG_INFO, "T", NULL, 5) == ELIB_LOG_ERR_INVALID_PARAM);
    elib_log_deinit(&g_ctx);
}

/* ---- Macro tests ---- */

static void test_macros(void)
{
    init_ctx_with_timestamp();

    mock_reset();
    ELIB_LOGE(&g_ctx, "App", "error %d", 1);
    assert(strstr(mock_buf, "\033[31m") != NULL);
    assert(strstr(mock_buf, "[App]:error 1") != NULL);

    mock_reset();
    ELIB_LOGW(&g_ctx, "App", "warn %d", 2);
    assert(strstr(mock_buf, "\033[33m") != NULL);
    assert(strstr(mock_buf, "[App]:warn 2") != NULL);

    mock_reset();
    ELIB_LOGI(&g_ctx, "App", "info %d", 3);
    assert(strstr(mock_buf, "\033[32m") != NULL);
    assert(strstr(mock_buf, "[App]:info 3") != NULL);

    elib_log_set_level(&g_ctx, ELIB_LOG_DEBUG);
    mock_reset();
    ELIB_LOGD(&g_ctx, "App", "debug %d", 4);
    assert(strstr(mock_buf, "\033[90m") != NULL);
    assert(strstr(mock_buf, "[App]:debug 4") != NULL);

    elib_log_deinit(&g_ctx);
}

/* ---- Null head: skip head prefix entirely ---- */

static void test_null_head(void)
{
    init_ctx_no_timestamp();
    elib_log_log(&g_ctx, ELIB_LOG_INFO, NULL, "message");
    assert(strstr(mock_buf, "[") == NULL);
    assert(strstr(mock_buf, "]:") == NULL);
    assert(strstr(mock_buf, "message") != NULL);
    assert(mock_output_call_count == 1);
    elib_log_deinit(&g_ctx);
}

static void test_null_head_with_timestamp(void)
{
    init_ctx_with_timestamp();
    elib_log_log(&g_ctx, ELIB_LOG_INFO, NULL, "msg");
    assert(strstr(mock_buf, "[12323.678]") != NULL);
    assert(strstr(mock_buf, "]:") == NULL);
    assert(strstr(mock_buf, "msg") != NULL);
    elib_log_deinit(&g_ctx);
}

static void test_hex_null_head(void)
{
    init_ctx_no_timestamp();
    uint8_t data[] = {0xAB};
    elib_log_hex(&g_ctx, ELIB_LOG_INFO, NULL, data, 1);
    assert(strstr(mock_buf, "[") == NULL);
    assert(strstr(mock_buf, "]:") == NULL);
    assert(strstr(mock_buf, "AB") != NULL);
    elib_log_deinit(&g_ctx);
}

/* ---- Reinit test ---- */

static void test_reinit_after_deinit(void)
{
    elib_log_cfg_t cfg = {
        .fmt_buf = fmt_buf,
        .fmt_buf_size = sizeof(fmt_buf),
        .output = mock_output,
        .timestamp = NULL,
    };
    elib_log_ctx_t ctx;
    elib_log_init(&ctx, &cfg);
    elib_log_log(&ctx, ELIB_LOG_INFO, "T", "first");
    elib_log_deinit(&ctx);

    mock_reset();
    elib_log_init(&ctx, &cfg);
    elib_log_log(&ctx, ELIB_LOG_INFO, "T", "second");
    assert(strstr(mock_buf, "[T]:second") != NULL);
    elib_log_deinit(&ctx);
}

/* ---- CRLF at beginning test ---- */

static void test_crlf_at_beginning(void)
{
    init_ctx_no_timestamp();
    elib_log_log(&g_ctx, ELIB_LOG_INFO, "T", "test");
    assert(mock_buf[0] == '\r');
    assert(mock_buf[1] == '\n');
    elib_log_deinit(&g_ctx);
}

/* ---- Single output call test ---- */

static void test_single_output_call(void)
{
    init_ctx_with_timestamp();
    uint8_t data[] = {0x01, 0x02};

    mock_reset();
    elib_log_log(&g_ctx, ELIB_LOG_INFO, "T", "hello");
    assert(mock_output_call_count == 1);

    mock_reset();
    elib_log_hex(&g_ctx, ELIB_LOG_INFO, "T", data, 2);
    assert(mock_output_call_count == 1);

    elib_log_deinit(&g_ctx);
}

/* ---- Main ---- */

int main(void)
{
    printf("elib-log test suite\n");

    RUN_TEST(test_init_valid);
    RUN_TEST(test_init_null_ctx);
    RUN_TEST(test_init_null_cfg);
    RUN_TEST(test_init_null_output);
    RUN_TEST(test_init_null_fmt_buf);
    RUN_TEST(test_init_zero_buf_size);
    RUN_TEST(test_init_small_buf);
    RUN_TEST(test_deinit_valid);
    RUN_TEST(test_deinit_null);
    RUN_TEST(test_log_not_initialized);
    RUN_TEST(test_hex_not_initialized);
    RUN_TEST(test_set_level_not_initialized);
    RUN_TEST(test_log_basic);
    RUN_TEST(test_log_with_timestamp);
    RUN_TEST(test_log_without_timestamp);
    RUN_TEST(test_log_level_color);
    RUN_TEST(test_log_truncation);
    RUN_TEST(test_log_level_filter);
    RUN_TEST(test_set_level_valid);
    RUN_TEST(test_set_level_invalid);
    RUN_TEST(test_get_level_not_initialized);
    RUN_TEST(test_get_level_null);
    RUN_TEST(test_hex_basic);
    RUN_TEST(test_hex_empty);
    RUN_TEST(test_hex_single_byte);
    RUN_TEST(test_hex_with_timestamp);
    RUN_TEST(test_hex_level_filter);
    RUN_TEST(test_hex_null_data);
    RUN_TEST(test_macros);
    RUN_TEST(test_null_head);
    RUN_TEST(test_null_head_with_timestamp);
    RUN_TEST(test_hex_null_head);
    RUN_TEST(test_reinit_after_deinit);
    RUN_TEST(test_crlf_at_beginning);
    RUN_TEST(test_single_output_call);

    printf("\nAll 35 tests passed.\n");
    return 0;
}
