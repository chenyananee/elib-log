/* test_write_flags.c --- per-context layout: colour, timestamp and line ending */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_log.h"

#define BUF_SIZE 192u

static elib_log_ctx_t ctx;
static char buf[BUF_SIZE];
static char last[BUF_SIZE];
static unsigned out_count;

static unsigned ts_calls;
static elib_log_timestamp_t now;

static void capture(const char *s, void *ud) {
    size_t i = 0u;
    (void)ud;
    while (s[i] != '\0' && i + 1u < sizeof(last)) {
        last[i] = s[i];
        i++;
    }
    last[i] = '\0';
    out_count++;
}

static elib_log_timestamp_t clock_cb(void *ud) {
    (void)ud;
    ts_calls++;
    return now;
}

static void reset(uint32_t flags, elib_log_level_t level, int with_clock) {
    elib_log_cfg_t cfg = {
        .buf = buf,
        .buf_size = BUF_SIZE,
        .output = capture,
        .timestamp = with_clock ? clock_cb : NULL,
        .user_data = NULL,
        .level = level,
        .flags = flags,
    };
    memset(buf, 0, sizeof(buf));
    memset(last, 0, sizeof(last));
    memset(&ctx, 0, sizeof(ctx));
    out_count = 0u;
    ts_calls = 0u;
    now.s = 7u;
    now.ms = 5u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
}

void test_default_is_crlf(void) {
    printf("Test: no end-of-line flag defaults to CRLF... ");
    reset(0u, ELIB_LOG_DEBUG, 0);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(strcmp(last, "[T]:hi\r\n") == 0);
    printf("PASSED\n");
}

void test_eol_lf(void) {
    printf("Test: EOL_LF emits a single LF... ");
    reset(ELIB_LOG_FLAG_EOL_LF, ELIB_LOG_DEBUG, 0);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(strcmp(last, "[T]:hi\n") == 0);
    printf("PASSED\n");
}

void test_eol_none(void) {
    printf("Test: EOL_NONE emits no terminator... ");
    reset(ELIB_LOG_FLAG_EOL_NONE, ELIB_LOG_DEBUG, 0);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(strcmp(last, "[T]:hi") == 0);
    printf("PASSED\n");
}

void test_eol_crlf_explicit(void) {
    printf("Test: EOL_CRLF matches the default... ");
    reset(ELIB_LOG_FLAG_EOL_CRLF, ELIB_LOG_DEBUG, 0);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(strcmp(last, "[T]:hi\r\n") == 0);
    printf("PASSED\n");
}

void test_no_color_by_default(void) {
    printf("Test: without FLAG_COLOR no escape byte is ever emitted... ");
    reset(ELIB_LOG_FLAG_TIMESTAMP, ELIB_LOG_DEBUG, 1);
    assert(elib_log_write(&ctx, ELIB_LOG_ERROR, "T", "plain") == ELIB_LOG_OK);
    assert(memchr(last, 0x1B, strlen(last)) == NULL);
    assert(strcmp(last, "[7.005][T]:plain\r\n") == 0);
    printf("PASSED\n");
}

void test_color_per_level(void) {
    printf("Test: colour is chosen per severity and closed with the reset... ");
    reset(ELIB_LOG_FLAG_COLOR, ELIB_LOG_DEBUG, 0);

    assert(elib_log_write(&ctx, ELIB_LOG_DEBUG, "T", "d") == ELIB_LOG_OK);
    assert(strcmp(last, "\033[90m[T]:d\033[0m\r\n") == 0);

    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "i") == ELIB_LOG_OK);
    assert(strcmp(last, "\033[32m[T]:i\033[0m\r\n") == 0);

    assert(elib_log_write(&ctx, ELIB_LOG_WARNING, "T", "w") == ELIB_LOG_OK);
    assert(strcmp(last, "\033[33m[T]:w\033[0m\r\n") == 0);

    assert(elib_log_write(&ctx, ELIB_LOG_ERROR, "T", "e") == ELIB_LOG_OK);
    assert(strcmp(last, "\033[31m[T]:e\033[0m\r\n") == 0);
    printf("PASSED\n");
}

void test_color_without_terminator(void) {
    printf("Test: colour reset still closes the line with EOL_NONE... ");
    reset(ELIB_LOG_FLAG_COLOR | ELIB_LOG_FLAG_EOL_NONE, ELIB_LOG_DEBUG, 0);
    assert(elib_log_write(&ctx, ELIB_LOG_ERROR, "T", "hi") == ELIB_LOG_OK);
    assert(strcmp(last, "\033[31m[T]:hi\033[0m") == 0);
    printf("PASSED\n");
}

void test_timestamp_prefix(void) {
    printf("Test: the timestamp flag renders \"[s.mmm]\" with zero padding... ");
    reset(ELIB_LOG_FLAG_TIMESTAMP, ELIB_LOG_DEBUG, 1);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(strcmp(last, "[7.005][T]:hi\r\n") == 0);
    assert(ts_calls == 1u);

    now.s = 0u;
    now.ms = 0u;
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(strcmp(last, "[0.000][T]:hi\r\n") == 0);

    now.s = 4294967295ul;
    now.ms = 999u;
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(strcmp(last, "[4294967295.999][T]:hi\r\n") == 0);
    printf("PASSED\n");
}

void test_timestamp_requires_the_flag(void) {
    printf("Test: a registered clock stays unused without the flag... ");
    reset(0u, ELIB_LOG_DEBUG, 1);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(strcmp(last, "[T]:hi\r\n") == 0);
    assert(ts_calls == 0u);
    printf("PASSED\n");
}

void test_all_flags_combined(void) {
    printf("Test: colour, timestamp and LF combine in one order... ");
    reset(ELIB_LOG_FLAG_COLOR | ELIB_LOG_FLAG_TIMESTAMP | ELIB_LOG_FLAG_EOL_LF,
          ELIB_LOG_WARNING, 1);
    assert(elib_log_write(&ctx, ELIB_LOG_WARNING, "T", "hi") == ELIB_LOG_OK);
    assert(strcmp(last, "\033[33m[7.005][T]:hi\033[0m\n") == 0);
    printf("PASSED\n");
}

void test_head_omitted_with_all_flags(void) {
    printf("Test: a NULL head leaves timestamp and colour adjacent... ");
    reset(ELIB_LOG_FLAG_COLOR | ELIB_LOG_FLAG_TIMESTAMP, ELIB_LOG_DEBUG, 1);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, NULL, "hi") == ELIB_LOG_OK);
    assert(strcmp(last, "\033[32m[7.005]hi\033[0m\r\n") == 0);
    printf("PASSED\n");
}

void test_hex_respects_layout_flags(void) {
    printf("Test: hex dumps use the same colour, timestamp and terminator... ");
    const uint8_t data[2] = { 0x00u, 0xFFu };
    reset(ELIB_LOG_FLAG_COLOR | ELIB_LOG_FLAG_TIMESTAMP, ELIB_LOG_DEBUG, 1);
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, "H", data, 2u) == ELIB_LOG_OK);
    assert(strcmp(last, "\033[32m[7.005][H]:00 FF\033[0m\r\n") == 0);
    assert(ts_calls == 1u);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_write_flags ===\n");
    test_default_is_crlf();
    test_eol_lf();
    test_eol_none();
    test_eol_crlf_explicit();
    test_no_color_by_default();
    test_color_per_level();
    test_color_without_terminator();
    test_timestamp_prefix();
    test_timestamp_requires_the_flag();
    test_all_flags_combined();
    test_head_omitted_with_all_flags();
    test_hex_respects_layout_flags();
    printf("=== test_write_flags: all PASSED ===\n");
    return 0;
}
