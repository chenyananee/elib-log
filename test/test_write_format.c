/* test_write_format.c --- message assembly, head handling, printf pass-through */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_log.h"

#define BUF_SIZE 192u

static elib_log_ctx_t ctx;
static char buf[BUF_SIZE];
static char last[BUF_SIZE];
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

static void reset(void) {
    elib_log_cfg_t cfg = {
        .buf = buf,
        .buf_size = BUF_SIZE,
        .output = capture,
        .timestamp = NULL,
        .user_data = NULL,
        .level = ELIB_LOG_DEBUG,
        .flags = 0u,
    };
    memset(buf, 0, sizeof(buf));
    memset(last, 0, sizeof(last));
    memset(&ctx, 0, sizeof(ctx));
    out_count = 0u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
}

void test_head_prefix(void) {
    printf("Test: head renders as \"[HEAD]:\" in front of the message... ");
    reset();
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "TAG", "hello") == ELIB_LOG_OK);
    assert(strcmp(last, "[TAG]:hello\r\n") == 0);
    assert(out_count == 1u);
    printf("PASSED\n");
}

void test_null_head_omits_prefix(void) {
    printf("Test: a NULL head omits the prefix entirely... ");
    reset();
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, NULL, "hello") == ELIB_LOG_OK);
    assert(strcmp(last, "hello\r\n") == 0);
    printf("PASSED\n");
}

void test_empty_head(void) {
    printf("Test: an empty head still renders the brackets... ");
    reset();
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "", "x") == ELIB_LOG_OK);
    assert(strcmp(last, "[]:x\r\n") == 0);
    printf("PASSED\n");
}

void test_printf_conversions(void) {
    printf("Test: printf conversions reach the line verbatim... ");
    reset();
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "%d|%u|%s|%c|%x|%%",
                          42, 7u, "str", 'Z', 0xABu) == ELIB_LOG_OK);
    assert(strcmp(last, "[T]:42|7|str|Z|ab|%\r\n") == 0);

    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "%d", -7) == ELIB_LOG_OK);
    assert(strcmp(last, "[T]:-7\r\n") == 0);

    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "%lu", (unsigned long)12345678ul)
           == ELIB_LOG_OK);
    assert(strcmp(last, "[T]:12345678\r\n") == 0);
    printf("PASSED\n");
}

void test_empty_format(void) {
    printf("Test: an empty format yields just the prefix and terminator... ");
    reset();
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "") == ELIB_LOG_OK);
    assert(strcmp(last, "[T]:\r\n") == 0);
    printf("PASSED\n");
}

void test_null_format_rejected(void) {
    printf("Test: a NULL format is rejected before anything is formatted... ");
    reset();
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", NULL)
           == ELIB_LOG_ERR_INVALID_PARAM);
    assert(out_count == 0u);
    printf("PASSED\n");
}

void test_one_output_per_call(void) {
    printf("Test: every call produces exactly one complete line... ");
    reset();
    for (unsigned i = 0u; i < 5u; i++) {
        assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "line %u", i)
               == ELIB_LOG_OK);
        assert(out_count == i + 1u);
    }
    assert(strcmp(last, "[T]:line 4\r\n") == 0);
    printf("PASSED\n");
}

void test_message_content_is_verbatim(void) {
    printf("Test: embedded whitespace and newlines are passed through... ");
    reset();
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "a\tb c") == ELIB_LOG_OK);
    assert(strcmp(last, "[T]:a\tb c\r\n") == 0);
    printf("PASSED\n");
}

void test_message_that_fits_is_not_truncated(void) {
    printf("Test: a message that fits returns OK... ");
    reset();
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "%s",
                          "0123456789012345678901234567890123456789")
           == ELIB_LOG_OK);
    assert(strcmp(last,
                  "[T]:0123456789012345678901234567890123456789\r\n") == 0);
    printf("PASSED\n");
}

void test_line_is_nul_terminated(void) {
    printf("Test: the emitted line is NUL-terminated inside the buffer... ");
    reset();
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "TAG", "abc") == ELIB_LOG_OK);
    assert(strlen(last) == strlen("[TAG]:abc\r\n"));
    assert(buf[strlen("[TAG]:abc\r\n")] == '\0');
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_write_format ===\n");
    test_head_prefix();
    test_null_head_omits_prefix();
    test_empty_head();
    test_printf_conversions();
    test_empty_format();
    test_null_format_rejected();
    test_one_output_per_call();
    test_message_content_is_verbatim();
    test_message_that_fits_is_not_truncated();
    test_line_is_nul_terminated();
    printf("=== test_write_format: all PASSED ===\n");
    return 0;
}
