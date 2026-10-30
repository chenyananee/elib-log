/* test_critical.c --- critical-hook pairing, balance and what runs inside */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_log.h"

#define BUF_SIZE 128u

static elib_log_ctx_t ctx;
static char buf[BUF_SIZE];
static char last[BUF_SIZE];
static unsigned out_count;

static unsigned hook_enter;
static unsigned hook_exit;
static void enter_hook(void) { hook_enter++; }
static void exit_hook(void) { hook_exit++; }

static unsigned balance_at_output;
static unsigned balance_at_clock;

static elib_log_timestamp_t clock_cb(void *ud) {
    elib_log_timestamp_t ts = { 9u, 8u };
    (void)ud;
    balance_at_clock = hook_enter - hook_exit;
    return ts;
}

static void capture(const char *s, void *ud) {
    size_t i = 0u;
    (void)ud;
    balance_at_output = hook_enter - hook_exit;
    while (s[i] != '\0' && i + 1u < sizeof(last)) {
        last[i] = s[i];
        i++;
    }
    last[i] = '\0';
    out_count++;
}

static void reset(uint32_t flags, void (*enter)(void), void (*exit)(void)) {
    elib_log_cfg_t cfg = {
        .buf = buf,
        .buf_size = BUF_SIZE,
        .output = capture,
        .timestamp = clock_cb,
        .user_data = NULL,
        .level = ELIB_LOG_DEBUG,
        .flags = flags,
    };
    memset(buf, 0, sizeof(buf));
    memset(last, 0, sizeof(last));
    memset(&ctx, 0, sizeof(ctx));
    out_count = 0u;
    hook_enter = 0u;
    hook_exit = 0u;
    balance_at_output = 0u;
    balance_at_clock = 0u;
    assert(elib_log_init(&ctx, &cfg, enter, exit) == ELIB_LOG_OK);
}

void test_hooks_are_stored(void) {
    printf("Test: init stores the hook pair... ");
    reset(ELIB_LOG_FLAG_TIMESTAMP, enter_hook, exit_hook);
    assert(ctx.enter_critical == enter_hook);
    assert(ctx.exit_critical == exit_hook);
    printf("PASSED\n");
}

void test_write_is_balanced(void) {
    printf("Test: a logged line enters and leaves the critical section once... ");
    reset(ELIB_LOG_FLAG_TIMESTAMP, enter_hook, exit_hook);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(hook_enter == 1u);
    assert(hook_exit == 1u);
    assert(hook_enter == hook_exit);
    printf("PASSED\n");
}

void test_hex_is_balanced(void) {
    printf("Test: a hex dump enters and leaves the critical section once... ");
    static const uint8_t data[3] = { 0x01u, 0x02u, 0x03u };
    reset(ELIB_LOG_FLAG_TIMESTAMP, enter_hook, exit_hook);
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, "T", data, 3u) == ELIB_LOG_OK);
    assert(hook_enter == 1u);
    assert(hook_exit == 1u);
    printf("PASSED\n");
}

void test_output_runs_outside_the_lock(void) {
    printf("Test: the transport runs with the critical section released... ");
    reset(ELIB_LOG_FLAG_TIMESTAMP, enter_hook, exit_hook);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(balance_at_output == 0u);
    assert(hook_enter == hook_exit);
    printf("PASSED\n");
}

void test_clock_runs_inside_the_lock(void) {
    printf("Test: the clock runs as part of the fenced assembly... ");
    reset(ELIB_LOG_FLAG_TIMESTAMP, enter_hook, exit_hook);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(balance_at_clock == 1u);
    assert(hook_enter == hook_exit);
    printf("PASSED\n");
}

void test_filtered_line_takes_no_lock(void) {
    printf("Test: a filtered line never touches the hooks... ");
    reset(0u, enter_hook, exit_hook);
    assert(elib_log_set_level(&ctx, ELIB_LOG_ERROR) == ELIB_LOG_OK);
    assert(elib_log_write(&ctx, ELIB_LOG_DEBUG, "T", "noise") == ELIB_LOG_OK);
    assert(hook_enter == 0u);
    assert(hook_exit == 0u);
    printf("PASSED\n");
}

void test_truncated_line_unlocks(void) {
    printf("Test: a truncated line still leaves the lock... ");
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
    hook_enter = 0u;
    hook_exit = 0u;
    assert(elib_log_init(&ctx, &cfg, enter_hook, exit_hook) == ELIB_LOG_OK);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "TAG", "0123456789")
           == ELIB_LOG_ERR_TRUNCATED);
    assert(hook_enter == 1u);
    assert(hook_exit == 1u);
    printf("PASSED\n");
}

void test_null_hooks_are_functional(void) {
    printf("Test: a NULL hook pair leaves the logger working... ");
    reset(ELIB_LOG_FLAG_COLOR | ELIB_LOG_FLAG_TIMESTAMP, NULL, NULL);
    assert(elib_log_write(&ctx, ELIB_LOG_WARNING, "T", "hi") == ELIB_LOG_OK);
    assert(strcmp(last, "\033[33m[9.008][T]:hi\033[0m\r\n") == 0);
    printf("PASSED\n");
}

void test_many_lines_stay_balanced(void) {
    printf("Test: repeated use keeps the enter/exit counts equal... ");
    static const uint8_t data[2] = { 0xAAu, 0xBBu };
    reset(ELIB_LOG_FLAG_TIMESTAMP, enter_hook, exit_hook);
    for (unsigned i = 0u; i < 50u; i++) {
        assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "%u", i) == ELIB_LOG_OK);
        assert(elib_log_hex(&ctx, ELIB_LOG_INFO, "H", data, 2u) == ELIB_LOG_OK);
    }
    assert(hook_enter == 100u);
    assert(hook_exit == 100u);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_critical ===\n");
    test_hooks_are_stored();
    test_write_is_balanced();
    test_hex_is_balanced();
    test_output_runs_outside_the_lock();
    test_clock_runs_inside_the_lock();
    test_filtered_line_takes_no_lock();
    test_truncated_line_unlocks();
    test_null_hooks_are_functional();
    test_many_lines_stay_balanced();
    printf("=== test_critical: all PASSED ===\n");
    return 0;
}
