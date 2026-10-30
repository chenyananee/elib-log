/* test_level_api.c --- set/get level plus compile-time level removal
 *
 * ELIB_LOG_COMPILE_LEVEL is defined before the umbrella header on purpose:
 * everything below WARNING must vanish from the translation unit. */
#define ELIB_LOG_COMPILE_LEVEL 2       /* ELIB_LOG_LEVEL_WARNING */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_log.h"

#define BUF_SIZE 128u

static elib_log_ctx_t ctx;
static char buf[BUF_SIZE];
static unsigned out_count;

static void capture(const char *s, void *ud) { (void)s; (void)ud; out_count++; }

static unsigned side_effects;

static int expensive(int value) {
    side_effects++;
    return value;
}

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
    side_effects = 0u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
}

void test_compile_level_is_warning(void) {
    printf("Test: ELIB_LOG_COMPILE_LEVEL is WARNING in this unit... ");
    assert(ELIB_LOG_COMPILE_LEVEL == ELIB_LOG_LEVEL_WARNING);
    printf("PASSED\n");
}

void test_get_level_reports_the_init_value(void) {
    printf("Test: get_level reports the value copied from cfg... ");
    reset(ELIB_LOG_WARNING);
    elib_log_level_t level = ELIB_LOG_DEBUG;
    assert(elib_log_get_level(&ctx, &level) == ELIB_LOG_OK);
    assert(level == ELIB_LOG_WARNING);
    printf("PASSED\n");
}

void test_set_level_changes_the_filter(void) {
    printf("Test: set_level takes effect immediately... ");
    reset(ELIB_LOG_DEBUG);

    elib_log_level_t level = ELIB_LOG_DEBUG;
    assert(elib_log_set_level(&ctx, ELIB_LOG_ERROR) == ELIB_LOG_OK);
    assert(elib_log_get_level(&ctx, &level) == ELIB_LOG_OK);
    assert(level == ELIB_LOG_ERROR);

    assert(elib_log_write(&ctx, ELIB_LOG_WARNING, "T", "w") == ELIB_LOG_OK);
    assert(out_count == 0u);

    assert(elib_log_set_level(&ctx, ELIB_LOG_DEBUG) == ELIB_LOG_OK);
    assert(elib_log_write(&ctx, ELIB_LOG_DEBUG, "T", "d") == ELIB_LOG_OK);
    assert(out_count == 1u);
    printf("PASSED\n");
}

void test_set_level_rejects_out_of_range(void) {
    printf("Test: set_level rejects an out-of-range value and keeps the old one... ");
    reset(ELIB_LOG_INFO);
    assert(elib_log_set_level(&ctx, (elib_log_level_t)4)
           == ELIB_LOG_ERR_INVALID_PARAM);
    assert(elib_log_set_level(&ctx, (elib_log_level_t)(-1))
           == ELIB_LOG_ERR_INVALID_PARAM);

    elib_log_level_t level = ELIB_LOG_DEBUG;
    assert(elib_log_get_level(&ctx, &level) == ELIB_LOG_OK);
    assert(level == ELIB_LOG_INFO);
    printf("PASSED\n");
}

void test_null_parameters(void) {
    printf("Test: NULL ctx and NULL out pointer are rejected... ");
    reset(ELIB_LOG_INFO);
    elib_log_level_t level = ELIB_LOG_INFO;

    assert(elib_log_set_level(NULL, ELIB_LOG_DEBUG) == ELIB_LOG_ERR_INVALID_PARAM);
    assert(elib_log_get_level(NULL, &level) == ELIB_LOG_ERR_INVALID_PARAM);
    assert(elib_log_get_level(&ctx, NULL) == ELIB_LOG_ERR_INVALID_PARAM);
    printf("PASSED\n");
}

void test_compile_level_removes_debug_and_info(void) {
    printf("Test: DEBUG and INFO macros compile to nothing... ");
    reset(ELIB_LOG_DEBUG);

    ELIB_LOGD(&ctx, "T", "d%d", expensive(1));
    ELIB_LOGI(&ctx, "T", "i%d", expensive(2));

    assert(out_count == 0u);
    assert(side_effects == 0u);      /* the arguments were never evaluated */
    printf("PASSED\n");
}

void test_compile_level_keeps_warning_and_error(void) {
    printf("Test: WARNING and ERROR macros still emit... ");
    reset(ELIB_LOG_DEBUG);

    ELIB_LOGW(&ctx, "T", "w%d", expensive(3));
    ELIB_LOGE(&ctx, "T", "e%d", expensive(4));

    assert(out_count == 2u);
    assert(side_effects == 2u);
    printf("PASSED\n");
}

void test_runtime_filter_still_applies(void) {
    printf("Test: the runtime filter stacks on top of the compile level... ");
    reset(ELIB_LOG_ERROR);

    ELIB_LOGW(&ctx, "T", "w");
    assert(out_count == 0u);

    ELIB_LOGE(&ctx, "T", "e");
    assert(out_count == 1u);
    printf("PASSED\n");
}

void test_level_survives_a_line(void) {
    printf("Test: logging does not change the configured level... ");
    reset(ELIB_LOG_DEBUG);
    assert(elib_log_write(&ctx, ELIB_LOG_ERROR, "T", "e") == ELIB_LOG_OK);

    elib_log_level_t level = ELIB_LOG_ERROR;
    assert(elib_log_get_level(&ctx, &level) == ELIB_LOG_OK);
    assert(level == ELIB_LOG_DEBUG);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_level_api ===\n");
    test_compile_level_is_warning();
    test_get_level_reports_the_init_value();
    test_set_level_changes_the_filter();
    test_set_level_rejects_out_of_range();
    test_null_parameters();
    test_compile_level_removes_debug_and_info();
    test_compile_level_keeps_warning_and_error();
    test_runtime_filter_still_applies();
    test_level_survives_a_line();
    printf("=== test_level_api: all PASSED ===\n");
    return 0;
}
