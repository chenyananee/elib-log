/* test_reentrancy.c --- a log call from inside the transport is refused */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_log.h"

#define BUF_SIZE 128u

static elib_log_ctx_t ctx;
static char buf[BUF_SIZE];
static char last[BUF_SIZE];
static unsigned out_count;

static unsigned nested_mode;          /* 0 = off, 1 = nested write, 2 = filtered */
static elib_log_err_t nested_err;
static unsigned nested_calls;
static elib_log_level_t nested_seen_level;

static void capture(const char *s, void *ud) {
    size_t i = 0u;
    (void)ud;

    /* Snapshot the line the transport was given: a nested call must not be
     * able to rewrite it underneath us */
    while (s[i] != '\0' && i + 1u < sizeof(last)) {
        last[i] = s[i];
        i++;
    }
    last[i] = '\0';
    out_count++;

    if (nested_mode != 0u && nested_calls == 0u) {
        nested_calls++;
        if (nested_mode == 1u) {
            nested_err = elib_log_write(&ctx, ELIB_LOG_ERROR, "IN", "nested");
        } else {
            assert(elib_log_set_level(&ctx, ELIB_LOG_ERROR) == ELIB_LOG_OK);
            /* DEBUG is now below the threshold, so this is filtered out before
             * the guard is consulted and must therefore report OK */
            nested_err = elib_log_write(&ctx, ELIB_LOG_DEBUG, "IN", "nested");
        }
        assert(elib_log_get_level(&ctx, &nested_seen_level) == ELIB_LOG_OK);
    }
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
    nested_mode = 0u;
    nested_err = ELIB_LOG_OK;
    nested_calls = 0u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
}

void test_nested_write_is_busy(void) {
    printf("Test: logging from the transport returns BUSY... ");
    reset();
    nested_mode = 1u;
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "outer") == ELIB_LOG_OK);
    assert(nested_calls == 1u);
    assert(nested_err == ELIB_LOG_ERR_BUSY);
    assert(out_count == 1u);            /* only the outer line was delivered */
    printf("PASSED\n");
}

void test_outer_line_survives_the_nested_attempt(void) {
    printf("Test: the outer line is not corrupted by the refused call... ");
    reset();
    nested_mode = 1u;
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "outer text") == ELIB_LOG_OK);
    assert(strcmp(last, "[T]:outer text\r\n") == 0);
    assert(strcmp(buf, "[T]:outer text\r\n") == 0);
    printf("PASSED\n");
}

void test_guard_is_held_during_a_hex_dump(void) {
    printf("Test: a hex dump holds the guard for its whole line... ");
    reset();
    nested_mode = 1u;
    static const uint8_t data[2] = { 0x01u, 0x02u };
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, "H", data, 2u) == ELIB_LOG_OK);
    assert(nested_err == ELIB_LOG_ERR_BUSY);   /* the nested call was a write */
    assert(out_count == 1u);
    printf("PASSED\n");
}

void test_busy_is_cleared_after_the_call(void) {
    printf("Test: the guard is released once the line is emitted... ");
    reset();
    nested_mode = 1u;
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "outer") == ELIB_LOG_OK);
    assert(ctx.busy == 0u);

    nested_mode = 0u;
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "after") == ELIB_LOG_OK);
    assert(strcmp(last, "[T]:after\r\n") == 0);
    assert(out_count == 2u);
    printf("PASSED\n");
}

void test_level_api_is_allowed_from_the_callback(void) {
    printf("Test: querying the level from the transport works... ");
    reset();
    nested_mode = 1u;
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "outer") == ELIB_LOG_OK);
    assert(nested_seen_level == ELIB_LOG_DEBUG);
    printf("PASSED\n");
}

void test_truncated_line_still_holds_the_guard(void) {
    printf("Test: the guard covers truncated lines too... ");
    static char small[BUF_SIZE];
    elib_log_cfg_t cfg = {
        .buf = small,
        .buf_size = 12u,
        .output = capture,
        .timestamp = NULL,
        .user_data = NULL,
        .level = ELIB_LOG_DEBUG,
        .flags = 0u,
    };
    memset(&ctx, 0, sizeof(ctx));
    memset(last, 0, sizeof(last));
    out_count = 0u;
    nested_mode = 0u;
    nested_calls = 0u;
    nested_err = ELIB_LOG_OK;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);

    nested_mode = 1u;
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "TAG", "0123456789")
           == ELIB_LOG_ERR_TRUNCATED);
    assert(nested_err == ELIB_LOG_ERR_BUSY);
    assert(ctx.busy == 0u);
    printf("PASSED\n");
}

void test_filtered_call_from_callback_is_ok(void) {
    printf("Test: a filtered nested call is OK, not BUSY... ");
    reset();
    nested_mode = 2u;              /* raise the level, then log DEBUG */
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "outer") == ELIB_LOG_OK);
    assert(nested_calls == 1u);
    assert(nested_err == ELIB_LOG_OK);
    assert(out_count == 1u);       /* the nested line was filtered, not emitted */
    assert(ctx.level == ELIB_LOG_ERROR);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_reentrancy ===\n");
    test_nested_write_is_busy();
    test_outer_line_survives_the_nested_attempt();
    test_guard_is_held_during_a_hex_dump();
    test_busy_is_cleared_after_the_call();
    test_level_api_is_allowed_from_the_callback();
    test_truncated_line_still_holds_the_guard();
    test_filtered_call_from_callback_is_ok();
    printf("=== test_reentrancy: all PASSED ===\n");
    return 0;
}
