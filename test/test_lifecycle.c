/* test_lifecycle.c --- deinit gating, re-initialization, stale contexts */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_log.h"

#define BUF_SIZE 128u

static elib_log_ctx_t ctx;
static char buf[BUF_SIZE];
static unsigned out_count;

static void capture(const char *s, void *ud) { (void)s; (void)ud; out_count++; }

static void reset(void) {
    elib_log_cfg_t cfg = {
        .buf = buf,
        .buf_size = BUF_SIZE,
        .output = capture,
        .timestamp = NULL,
        .user_data = NULL,
        .level = ELIB_LOG_DEBUG,
        .flags = ELIB_LOG_FLAG_COLOR,
    };
    memset(buf, 0, sizeof(buf));
    memset(&ctx, 0, sizeof(ctx));
    out_count = 0u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
}

void test_deinit_is_null_safe(void) {
    printf("Test: deinit(NULL) does nothing... ");
    elib_log_deinit(NULL);
    printf("PASSED\n");
}

void test_deinit_clears_the_flags(void) {
    printf("Test: deinit clears initialized and busy... ");
    reset();
    assert(ctx.initialized == 1u);
    ctx.busy = 1u;                  /* simulate a stuck guard */

    elib_log_deinit(&ctx);
    assert(ctx.initialized == 0u);
    assert(ctx.busy == 0u);
    printf("PASSED\n");
}

void test_deinit_twice(void) {
    printf("Test: a second deinit is harmless... ");
    reset();
    elib_log_deinit(&ctx);
    elib_log_deinit(&ctx);
    assert(ctx.initialized == 0u);
    printf("PASSED\n");
}

void test_every_api_rejects_after_deinit(void) {
    printf("Test: every API except init rejects a deinitialized context... ");
    static const uint8_t data[2] = { 0x01u, 0x02u };
    elib_log_level_t level;
    reset();
    elib_log_deinit(&ctx);

    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "x")
           == ELIB_LOG_ERR_NOT_INITIALIZED);
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, "T", data, 2u)
           == ELIB_LOG_ERR_NOT_INITIALIZED);
    assert(elib_log_set_level(&ctx, ELIB_LOG_DEBUG)
           == ELIB_LOG_ERR_NOT_INITIALIZED);
    assert(elib_log_get_level(&ctx, &level) == ELIB_LOG_ERR_NOT_INITIALIZED);
    assert(out_count == 0u);
    printf("PASSED\n");
}

void test_initialization_is_checked_before_filtering(void) {
    printf("Test: a filtered call on a dead context still reports the state... ");
    reset();
    assert(elib_log_set_level(&ctx, ELIB_LOG_ERROR) == ELIB_LOG_OK);
    elib_log_deinit(&ctx);

    assert(elib_log_write(&ctx, ELIB_LOG_DEBUG, "T", "x")
           == ELIB_LOG_ERR_NOT_INITIALIZED);
    printf("PASSED\n");
}

void test_null_ctx_is_rejected(void) {
    printf("Test: a NULL context is rejected by every API... ");
    static const uint8_t data[1] = { 0x00u };
    elib_log_level_t level;

    assert(elib_log_write(NULL, ELIB_LOG_INFO, "T", "x")
           == ELIB_LOG_ERR_INVALID_PARAM);
    assert(elib_log_hex(NULL, ELIB_LOG_INFO, "T", data, 1u)
           == ELIB_LOG_ERR_INVALID_PARAM);
    assert(elib_log_set_level(NULL, ELIB_LOG_INFO)
           == ELIB_LOG_ERR_INVALID_PARAM);
    assert(elib_log_get_level(NULL, &level) == ELIB_LOG_ERR_INVALID_PARAM);
    printf("PASSED\n");
}

void test_reinit_after_deinit(void) {
    printf("Test: init revives a deinitialized context... ");
    reset();
    elib_log_deinit(&ctx);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "x")
           == ELIB_LOG_ERR_NOT_INITIALIZED);

    elib_log_cfg_t cfg = {
        .buf = buf,
        .buf_size = BUF_SIZE,
        .output = capture,
        .timestamp = NULL,
        .user_data = NULL,
        .level = ELIB_LOG_INFO,
        .flags = 0u,
    };
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "back") == ELIB_LOG_OK);
    assert(out_count == 1u);
    printf("PASSED\n");
}

void test_reinit_drops_previous_configuration(void) {
    printf("Test: re-init replaces flags, level and hooks... ");
    reset();                        /* colour on, DEBUG */
    elib_log_cfg_t cfg = {
        .buf = buf,
        .buf_size = BUF_SIZE,
        .output = capture,
        .timestamp = NULL,
        .user_data = NULL,
        .level = ELIB_LOG_ERROR,
        .flags = ELIB_LOG_FLAG_EOL_NONE,
    };
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
    assert(ctx.flags == ELIB_LOG_FLAG_EOL_NONE);
    assert(ctx.level == ELIB_LOG_ERROR);
    assert(ctx.enter_critical == NULL);
    assert(ctx.exit_critical == NULL);

    out_count = 0u;
    assert(elib_log_write(&ctx, ELIB_LOG_WARNING, "T", "w") == ELIB_LOG_OK);
    assert(out_count == 0u);
    assert(elib_log_write(&ctx, ELIB_LOG_ERROR, "T", "e") == ELIB_LOG_OK);
    assert(out_count == 1u);
    assert(strcmp(buf, "[T]:e") == 0);      /* no colour, no terminator */
    printf("PASSED\n");
}

void test_deinit_keeps_the_configuration_readable(void) {
    printf("Test: deinit leaves the copied configuration in place... ");
    reset();
    elib_log_deinit(&ctx);
    assert(ctx.buf == buf);
    assert(ctx.buf_size == BUF_SIZE);
    assert(ctx.output == capture);
    assert(ctx.flags == ELIB_LOG_FLAG_COLOR);
    assert(ctx.initialized == 0u);
    printf("PASSED\n");
}

void test_reset_guard_after_a_failed_line(void) {
    printf("Test: a fully truncated coloured line still releases the guard... ");
    static char small[BUF_SIZE];
    elib_log_cfg_t cfg = {
        .buf = small,
        .buf_size = 13u,
        .output = capture,
        .timestamp = NULL,
        .user_data = NULL,
        .level = ELIB_LOG_DEBUG,
        .flags = ELIB_LOG_FLAG_COLOR,
    };
    memset(&ctx, 0, sizeof(ctx));
    out_count = 0u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);

    assert(elib_log_write(&ctx, ELIB_LOG_INFO, NULL, "abcdef")
           == ELIB_LOG_ERR_TRUNCATED);
    assert(ctx.busy == 0u);
    /* one content byte is available, so a single character fits exactly */
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, NULL, "x") == ELIB_LOG_OK);
    assert(ctx.busy == 0u);
    assert(out_count == 2u);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_lifecycle ===\n");
    test_deinit_is_null_safe();
    test_deinit_clears_the_flags();
    test_deinit_twice();
    test_every_api_rejects_after_deinit();
    test_initialization_is_checked_before_filtering();
    test_null_ctx_is_rejected();
    test_reinit_after_deinit();
    test_reinit_drops_previous_configuration();
    test_deinit_keeps_the_configuration_readable();
    test_reset_guard_after_a_failed_line();
    printf("=== test_lifecycle: all PASSED ===\n");
    return 0;
}
