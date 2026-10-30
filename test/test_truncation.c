/* test_truncation.c --- the line is cut, reported, and never overruns */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_log.h"

/* Storage is larger than the size handed to init, so an overrun would be
 * visible as a modified byte after buf_size */
#define STORAGE_SIZE 96u
#define GUARD_BYTE   0xAAu

static elib_log_ctx_t ctx;
static char storage[STORAGE_SIZE];
static char last[STORAGE_SIZE];
static unsigned out_count;

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
    elib_log_timestamp_t ts = { 1u, 2u };
    (void)ud;
    return ts;
}

static void reset(uint32_t buf_size, uint32_t flags, elib_log_level_t level) {
    elib_log_cfg_t cfg = {
        .buf = storage,
        .buf_size = buf_size,
        .output = capture,
        .timestamp = ((flags & ELIB_LOG_FLAG_TIMESTAMP) != 0u) ? clock_cb : NULL,
        .user_data = NULL,
        .level = level,
        .flags = flags,
    };
    memset(storage, GUARD_BYTE, sizeof(storage));
    memset(last, 0, sizeof(last));
    memset(&ctx, 0, sizeof(ctx));
    out_count = 0u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
}

static void check_guard(uint32_t buf_size) {
    for (uint32_t i = buf_size; i < STORAGE_SIZE; i++) {
        assert((unsigned char)storage[i] == GUARD_BYTE);
    }
}

void test_long_message_is_truncated(void) {
    printf("Test: an oversized message is cut and reports TRUNCATED... ");
    /* 16 - 0 (no colour) - 2 (CRLF) - 1 (NUL) = 13 content bytes */
    reset(16u, 0u, ELIB_LOG_DEBUG);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hello world")
           == ELIB_LOG_ERR_TRUNCATED);
    assert(strcmp(last, "[T]:hello wor\r\n") == 0);
    assert(out_count == 1u);
    assert(strlen(last) == 15u);
    assert(storage[15] == '\0');
    check_guard(16u);
    printf("PASSED\n");
}

void test_exact_fit_is_not_truncated(void) {
    printf("Test: content that exactly fills the content area returns OK... ");
    reset(16u, 0u, ELIB_LOG_DEBUG);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "%s", "123456789")
           == ELIB_LOG_OK);
    assert(strcmp(last, "[T]:123456789\r\n") == 0);
    printf("PASSED\n");
}

void test_one_byte_over_is_truncated(void) {
    printf("Test: one byte too many is reported... ");
    reset(16u, 0u, ELIB_LOG_DEBUG);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "%s", "1234567890")
           == ELIB_LOG_ERR_TRUNCATED);
    assert(strcmp(last, "[T]:123456789\r\n") == 0);
    printf("PASSED\n");
}

void test_terminator_survives_truncation(void) {
    printf("Test: the terminator is preserved when the message is cut... ");
    reset(16u, ELIB_LOG_FLAG_EOL_LF, ELIB_LOG_DEBUG);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "0123456789012345")
           == ELIB_LOG_ERR_TRUNCATED);
    assert(last[strlen(last) - 1u] == '\n');
    assert(strcmp(last, "[T]:0123456789\n") == 0);
    printf("PASSED\n");
}

void test_no_terminator_with_eol_none(void) {
    printf("Test: EOL_NONE lines are cut without adding anything... ");
    reset(16u, ELIB_LOG_FLAG_EOL_NONE, ELIB_LOG_DEBUG);
    /* 16 - 0 (no terminator) - 1 (NUL) = 15 content bytes */
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "0123456789012345")
           == ELIB_LOG_ERR_TRUNCATED);
    assert(strcmp(last, "[T]:01234567890") == 0);
    printf("PASSED\n");
}

void test_colour_escapes_are_never_split(void) {
    printf("Test: a cut keeps the colour opener and reset intact... ");
    /* 16 - 5 (opener) - 4 (reset) - 2 (CRLF) - 1 (NUL) = 4 content bytes */
    reset(16u, ELIB_LOG_FLAG_COLOR, ELIB_LOG_DEBUG);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, NULL, "hello")
           == ELIB_LOG_ERR_TRUNCATED);
    assert(strcmp(last, "\033[32mhell\033[0m\r\n") == 0);
    assert(strlen(last) == 15u);
    assert(storage[15] == '\0');
    check_guard(16u);
    printf("PASSED\n");
}

void test_minimal_colour_buffer(void) {
    printf("Test: the smallest usable coloured buffer still never overruns... ");
    /* opener(5) + reset(4) + CRLF(2) + NUL(1) = 12 fixed, plus one content byte */
    reset(13u, ELIB_LOG_FLAG_COLOR, ELIB_LOG_DEBUG);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, NULL, "abc")
           == ELIB_LOG_ERR_TRUNCATED);
    assert(strcmp(last, "\033[32ma\033[0m\r\n") == 0);
    assert(strlen(last) == 12u);          /* 12 + NUL == 13 == buf_size */
    check_guard(13u);
    printf("PASSED\n");
}

void test_colour_buffer_below_fixed_cost_is_rejected(void) {
    printf("Test: init rejects a coloured buffer below the fixed cost... ");
    elib_log_cfg_t cfg = {
        .buf = storage,
        .buf_size = 12u,               /* fixed cost only, no content byte */
        .output = capture,
        .timestamp = NULL,
        .user_data = NULL,
        .level = ELIB_LOG_DEBUG,
        .flags = ELIB_LOG_FLAG_COLOR,
    };
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);

    cfg.buf_size = 13u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);

    /* without colour the same 12 bytes are acceptable */
    cfg.buf_size = 12u;
    cfg.flags = 0u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);

    /* the minimum plain buffer is far smaller still */
    cfg.buf_size = ELIB_LOG_MIN_BUF_SIZE;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
    printf("PASSED\n");
}

void test_minimal_plain_buffer(void) {
    printf("Test: the smallest buffer fits three characters plus CRLF... ");
    reset(ELIB_LOG_MIN_BUF_SIZE, 0u, ELIB_LOG_DEBUG);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, NULL, "abc") == ELIB_LOG_OK);
    assert(strcmp(last, "abc\r\n") == 0);

    assert(elib_log_write(&ctx, ELIB_LOG_INFO, NULL, "abcdef")
           == ELIB_LOG_ERR_TRUNCATED);
    assert(strcmp(last, "abcde\r\n") == 0);
    assert(strlen(last) == 7u);           /* 7 + NUL == 8 == buf_size */
    check_guard(ELIB_LOG_MIN_BUF_SIZE);
    printf("PASSED\n");
}

void test_timestamp_prefix_is_kept(void) {
    printf("Test: the timestamp prefix is part of the reserved content... ");
    /* 24 - 2 (CRLF) - 1 (NUL) = 21 content bytes: "[1.002][T]:" is 11 */
    reset(24u, ELIB_LOG_FLAG_TIMESTAMP, ELIB_LOG_DEBUG);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "%s", "0123456789ABC")
           == ELIB_LOG_ERR_TRUNCATED);
    assert(strcmp(last, "[1.002][T]:0123456789\r\n") == 0);
    assert(strlen(last) == 23u);
    check_guard(24u);
    printf("PASSED\n");
}

void test_hex_dump_is_truncated(void) {
    printf("Test: an oversized hex dump is cut and reported... ");
    static const uint8_t data[8] = {
        0x00u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u
    };
    reset(16u, 0u, ELIB_LOG_DEBUG);
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, NULL, data, 8u)
           == ELIB_LOG_ERR_TRUNCATED);
    /* content area is 13 bytes: "00 11 22 33 4" */
    assert(strcmp(last, "00 11 22 33 4\r\n") == 0);
    check_guard(16u);
    printf("PASSED\n");
}

void test_repeated_truncation_never_grows(void) {
    printf("Test: repeated truncation keeps writing inside the same bounds... ");
    reset(16u, ELIB_LOG_FLAG_COLOR | ELIB_LOG_FLAG_TIMESTAMP, ELIB_LOG_DEBUG);
    for (unsigned i = 0u; i < 20u; i++) {
        assert(elib_log_write(&ctx, ELIB_LOG_ERROR, "HEAD", "%u-%u-%u",
                              i, i, i) == ELIB_LOG_ERR_TRUNCATED);
        check_guard(16u);
        assert(strlen(last) < 16u);
    }
    assert(out_count == 20u);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_truncation ===\n");
    test_long_message_is_truncated();
    test_exact_fit_is_not_truncated();
    test_one_byte_over_is_truncated();
    test_terminator_survives_truncation();
    test_no_terminator_with_eol_none();
    test_colour_escapes_are_never_split();
    test_minimal_colour_buffer();
    test_colour_buffer_below_fixed_cost_is_rejected();
    test_minimal_plain_buffer();
    test_timestamp_prefix_is_kept();
    test_hex_dump_is_truncated();
    test_repeated_truncation_never_grows();
    printf("=== test_truncation: all PASSED ===\n");
    return 0;
}
