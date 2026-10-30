/* test_init.c --- init validation, cfg copy, buffer bounds, re-initialization */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_log.h"

#define BUF_SIZE 64u

static elib_log_ctx_t ctx;
static char buf[BUF_SIZE];
static unsigned out_count;
static void capture(const char *s, void *ud) { (void)s; (void)ud; out_count++; }

static elib_log_cfg_t make_cfg(void) {
    elib_log_cfg_t cfg = {
        .buf = buf,
        .buf_size = BUF_SIZE,
        .output = capture,
        .timestamp = NULL,
        .user_data = NULL,
        .level = ELIB_LOG_INFO,
        .flags = 0u,
    };
    return cfg;
}

static elib_log_timestamp_t clock_cb(void *ud) {
    elib_log_timestamp_t ts = { 0u, 0u };
    (void)ud;
    return ts;
}

static void hook_a(void) { }
static void hook_b(void) { }

void test_version(void) {
    printf("Test: version macros... ");
    assert(strcmp(ELIB_LOG_VERSION, "2.0.0") == 0);
    assert(ELIB_LOG_VERSION_MAJOR == 2);
    assert(ELIB_LOG_VERSION_MINOR == 0);
    assert(ELIB_LOG_VERSION_PATCH == 0);
    printf("PASSED\n");
}

void test_level_values(void) {
    printf("Test: severity ascends with the numeric value... ");
    assert(ELIB_LOG_DEBUG == 0);
    assert(ELIB_LOG_INFO == 1);
    assert(ELIB_LOG_WARNING == 2);
    assert(ELIB_LOG_ERROR == 3);
    assert(ELIB_LOG_MIN_BUF_SIZE == 8u);
    printf("PASSED\n");
}

void test_init_copies_cfg(void) {
    printf("Test: init copies every cfg field into the context... ");
    int marker = 0;
    elib_log_cfg_t cfg = make_cfg();
    cfg.timestamp = clock_cb;
    cfg.user_data = &marker;
    cfg.level = ELIB_LOG_WARNING;
    cfg.flags = ELIB_LOG_FLAG_COLOR | ELIB_LOG_FLAG_EOL_LF;

    memset(&ctx, 0, sizeof(ctx));
    assert(elib_log_init(&ctx, &cfg, hook_a, hook_b) == ELIB_LOG_OK);

    assert(ctx.buf == buf);
    assert(ctx.buf_size == BUF_SIZE);
    assert(ctx.output == capture);
    assert(ctx.timestamp == clock_cb);
    assert(ctx.user_data == &marker);
    assert(ctx.flags == (ELIB_LOG_FLAG_COLOR | ELIB_LOG_FLAG_EOL_LF));
    assert(ctx.level == ELIB_LOG_WARNING);
    assert(ctx.enter_critical == hook_a);
    assert(ctx.exit_critical == hook_b);
    assert(ctx.initialized == 1u);
    assert(ctx.busy == 0u);
    printf("PASSED\n");
}

void test_init_cfg_is_a_value(void) {
    printf("Test: a stack cfg can go away after init... ");
    memset(&ctx, 0, sizeof(ctx));
    {
        elib_log_cfg_t cfg = make_cfg();
        assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
        memset(&cfg, 0xAA, sizeof(cfg));   /* destroy the original */
    }
    assert(ctx.buf == buf);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "alive") == ELIB_LOG_OK);
    assert(out_count == 1u);
    printf("PASSED\n");
}

void test_init_rejects_null(void) {
    printf("Test: init rejects NULL ctx, NULL cfg and a NULL buffer... ");
    elib_log_cfg_t cfg = make_cfg();

    assert(elib_log_init(NULL, &cfg, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);
    assert(elib_log_init(&ctx, NULL, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);

    cfg = make_cfg();
    cfg.buf = NULL;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);
    printf("PASSED\n");
}

void test_init_buffer_size(void) {
    printf("Test: init enforces ELIB_LOG_MIN_BUF_SIZE... ");
    elib_log_cfg_t cfg = make_cfg();

    cfg.buf_size = 0u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);

    cfg.buf_size = ELIB_LOG_MIN_BUF_SIZE - 1u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);

    cfg.buf_size = ELIB_LOG_MIN_BUF_SIZE;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
    assert(ctx.buf_size == ELIB_LOG_MIN_BUF_SIZE);
    printf("PASSED\n");
}

void test_init_requires_output(void) {
    printf("Test: init requires an output callback... ");
    elib_log_cfg_t cfg = make_cfg();
    cfg.output = NULL;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);
    printf("PASSED\n");
}

void test_init_level_range(void) {
    printf("Test: init rejects an out-of-range level... ");
    elib_log_cfg_t cfg = make_cfg();

    cfg.level = (elib_log_level_t)4;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);

    cfg.level = (elib_log_level_t)(-1);
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);

    cfg.level = ELIB_LOG_DEBUG;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
    printf("PASSED\n");
}

void test_init_hook_pair(void) {
    printf("Test: init rejects a one-sided critical-hook pair... ");
    elib_log_cfg_t cfg = make_cfg();

    assert(elib_log_init(&ctx, &cfg, hook_a, NULL) == ELIB_LOG_ERR_INVALID_PARAM);
    assert(elib_log_init(&ctx, &cfg, NULL, hook_b) == ELIB_LOG_ERR_INVALID_PARAM);
    assert(elib_log_init(&ctx, &cfg, hook_a, hook_b) == ELIB_LOG_OK);
    assert(ctx.enter_critical == hook_a);
    assert(ctx.exit_critical == hook_b);
    printf("PASSED\n");
}

void test_init_flags(void) {
    printf("Test: init rejects unknown flags and conflicting end-of-line flags... ");
    elib_log_cfg_t cfg = make_cfg();

    cfg.flags = 1u << 5;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);

    cfg.flags = ELIB_LOG_FLAG_EOL_CRLF | ELIB_LOG_FLAG_EOL_LF;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);

    cfg.flags = ELIB_LOG_FLAG_EOL_LF | ELIB_LOG_FLAG_EOL_NONE;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);

    cfg.flags = ELIB_LOG_FLAG_EOL_CRLF | ELIB_LOG_FLAG_EOL_LF |
                ELIB_LOG_FLAG_EOL_NONE;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);

    cfg.flags = ELIB_LOG_FLAG_EOL_CRLF;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
    cfg.flags = ELIB_LOG_FLAG_EOL_LF;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
    cfg.flags = ELIB_LOG_FLAG_EOL_NONE;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
    printf("PASSED\n");
}

void test_init_timestamp_requires_clock(void) {
    printf("Test: init rejects the timestamp flag without a clock... ");
    elib_log_cfg_t cfg = make_cfg();

    cfg.flags = ELIB_LOG_FLAG_TIMESTAMP;
    cfg.timestamp = NULL;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_ERR_INVALID_PARAM);

    cfg.timestamp = clock_cb;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
    printf("PASSED\n");
}

void test_init_reinitializes_dirty_ctx(void) {
    printf("Test: re-init overwrites a dirty context unconditionally... ");
    elib_log_cfg_t cfg = make_cfg();
    memset(&ctx, 0xAA, sizeof(ctx));

    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
    assert(ctx.initialized == 1u);
    assert(ctx.busy == 0u);
    assert(ctx.level == ELIB_LOG_INFO);
    assert(ctx.flags == 0u);
    assert(ctx.buf == buf);
    printf("PASSED\n");
}

void test_init_accepts_optional_pieces(void) {
    printf("Test: init accepts no timestamp and no hooks... ");
    elib_log_cfg_t cfg = make_cfg();
    cfg.timestamp = NULL;
    cfg.flags = 0u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
    assert(ctx.timestamp == NULL);
    assert(ctx.enter_critical == NULL);
    assert(ctx.exit_critical == NULL);
    printf("PASSED\n");
}

void test_write_stays_inside_buf_size(void) {
    printf("Test: writes never touch a byte at or beyond buf_size... ");
    static char storage[96];
    elib_log_cfg_t cfg = make_cfg();
    cfg.buf = storage;
    cfg.buf_size = 32u;
    cfg.flags = 0u;

    memset(storage, 0xAA, sizeof(storage));
    memset(&ctx, 0, sizeof(ctx));
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);

    out_count = 0u;
    (void)elib_log_write(&ctx, ELIB_LOG_INFO, "TAG", "%s",
                         "0123456789012345678901234567890123456789");
    (void)elib_log_hex(&ctx, ELIB_LOG_INFO, "TAG", (const uint8_t *)storage, 40u);

    for (size_t i = 32u; i < sizeof(storage); i++) {
        assert((unsigned char)storage[i] == 0xAAu);
    }
    assert(out_count == 2u);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_init ===\n");
    test_version();
    test_level_values();
    test_init_copies_cfg();
    test_init_cfg_is_a_value();
    test_init_rejects_null();
    test_init_buffer_size();
    test_init_requires_output();
    test_init_level_range();
    test_init_hook_pair();
    test_init_flags();
    test_init_timestamp_requires_clock();
    test_init_reinitializes_dirty_ctx();
    test_init_accepts_optional_pieces();
    test_write_stays_inside_buf_size();
    printf("=== test_init: all PASSED ===\n");
    return 0;
}
