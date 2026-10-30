/* test_level.c --- severity filtering: the threshold keeps everything at or
 * above it and drops everything below it */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_log.h"

#define BUF_SIZE 128u

static elib_log_ctx_t ctx;
static char buf[BUF_SIZE];
static unsigned out_count;

static void capture(const char *s, void *ud) { (void)s; (void)ud; out_count++; }

static void reset(elib_log_level_t level) {
    elib_log_cfg_t cfg = {
        .buf = buf,
        .buf_size = BUF_SIZE,
        .output = capture,
        .timestamp = NULL,
        .user_data = NULL,
        .level = level,
        .flags = 0u,
    };
    memset(buf, 0, sizeof(buf));
    memset(&ctx, 0, sizeof(ctx));
    out_count = 0u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
}

/* Emit all four levels; return how many lines reached the transport */
static unsigned emit_all(void) {
    unsigned before = out_count;
    assert(elib_log_write(&ctx, ELIB_LOG_DEBUG, "L", "debug") == ELIB_LOG_OK);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "L", "info") == ELIB_LOG_OK);
    assert(elib_log_write(&ctx, ELIB_LOG_WARNING, "L", "warning") == ELIB_LOG_OK);
    assert(elib_log_write(&ctx, ELIB_LOG_ERROR, "L", "error") == ELIB_LOG_OK);
    return out_count - before;
}

void test_threshold_debug(void) {
    printf("Test: threshold DEBUG emits all four levels... ");
    reset(ELIB_LOG_DEBUG);
    assert(emit_all() == 4u);
    printf("PASSED\n");
}

void test_threshold_info(void) {
    printf("Test: threshold INFO drops DEBUG only... ");
    reset(ELIB_LOG_INFO);
    assert(emit_all() == 3u);
    printf("PASSED\n");
}

void test_threshold_warning(void) {
    printf("Test: threshold WARNING drops INFO and DEBUG... ");
    reset(ELIB_LOG_WARNING);
    assert(emit_all() == 2u);
    printf("PASSED\n");
}

void test_threshold_error(void) {
    printf("Test: threshold ERROR drops everything but ERROR... ");
    reset(ELIB_LOG_ERROR);
    assert(emit_all() == 1u);
    printf("PASSED\n");
}

void test_error_is_never_filtered_by_default(void) {
    printf("Test: the default threshold still lets ERROR and WARNING through... ");
    reset(ELIB_LOG_INFO);
    assert(elib_log_write(&ctx, ELIB_LOG_ERROR, "T", "boom") == ELIB_LOG_OK);
    assert(elib_log_write(&ctx, ELIB_LOG_WARNING, "T", "careful") == ELIB_LOG_OK);
    assert(out_count == 2u);
    printf("PASSED\n");
}

void test_filtered_call_is_cheap_and_clean(void) {
    printf("Test: a filtered call returns OK, writes nothing and leaves buf untouched... ");
    reset(ELIB_LOG_ERROR);
    memset(buf, 0x5A, sizeof(buf));

    assert(elib_log_write(&ctx, ELIB_LOG_DEBUG, "T", "%d", 1) == ELIB_LOG_OK);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "%d", 2) == ELIB_LOG_OK);
    assert(elib_log_hex(&ctx, ELIB_LOG_DEBUG, "T", (const uint8_t *)"abc", 3u)
           == ELIB_LOG_OK);

    assert(out_count == 0u);
    for (size_t i = 0u; i < sizeof(buf); i++) {
        assert((unsigned char)buf[i] == 0x5Au);
    }
    printf("PASSED\n");
}

void test_filter_boundary_is_inclusive(void) {
    printf("Test: the threshold level itself is emitted... ");
    reset(ELIB_LOG_WARNING);
    assert(elib_log_write(&ctx, ELIB_LOG_WARNING, "T", "edge") == ELIB_LOG_OK);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "below") == ELIB_LOG_OK);
    assert(out_count == 1u);
    printf("PASSED\n");
}

void test_hex_is_filtered_too(void) {
    printf("Test: hex dumps obey the same threshold... ");
    reset(ELIB_LOG_WARNING);
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, "T", (const uint8_t *)"\x01", 1u)
           == ELIB_LOG_OK);
    assert(out_count == 0u);
    assert(elib_log_hex(&ctx, ELIB_LOG_ERROR, "T", (const uint8_t *)"\x01", 1u)
           == ELIB_LOG_OK);
    assert(out_count == 1u);
    printf("PASSED\n");
}

void test_invalid_level_is_not_filtered_silently(void) {
    printf("Test: an invalid level is an error, not a filtered line... ");
    reset(ELIB_LOG_INFO);
    assert(elib_log_write(&ctx, (elib_log_level_t)9, "T", "x")
           == ELIB_LOG_ERR_INVALID_PARAM);
    assert(elib_log_hex(&ctx, (elib_log_level_t)(-1), "T", NULL, 0u)
           == ELIB_LOG_ERR_INVALID_PARAM);
    assert(out_count == 0u);
    printf("PASSED\n");
}

void test_macros_respect_the_runtime_threshold(void) {
    printf("Test: the convenience macros feed the same filter... ");
    reset(ELIB_LOG_WARNING);
    ELIB_LOGE(&ctx, "T", "e%d", 1);
    ELIB_LOGW(&ctx, "T", "w%d", 2);
    ELIB_LOGI(&ctx, "T", "i%d", 3);
    ELIB_LOGD(&ctx, "T", "d%d", 4);
    assert(out_count == 2u);

    reset(ELIB_LOG_DEBUG);
    ELIB_LOGE(&ctx, "T", "e");
    ELIB_LOGW(&ctx, "T", "w");
    ELIB_LOGI(&ctx, "T", "i");
    ELIB_LOGD(&ctx, "T", "d");
    assert(out_count == 4u);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_level ===\n");
    test_threshold_debug();
    test_threshold_info();
    test_threshold_warning();
    test_threshold_error();
    test_error_is_never_filtered_by_default();
    test_filtered_call_is_cheap_and_clean();
    test_filter_boundary_is_inclusive();
    test_hex_is_filtered_too();
    test_invalid_level_is_not_filtered_silently();
    test_macros_respect_the_runtime_threshold();
    printf("=== test_level: all PASSED ===\n");
    return 0;
}
