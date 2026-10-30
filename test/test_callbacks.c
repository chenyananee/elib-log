/* test_callbacks.c --- user_data plumbing and callback ordering */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_log.h"

#define BUF_SIZE 128u

typedef struct {
    char last[BUF_SIZE];
    unsigned output_calls;
    unsigned clock_calls;
    unsigned order;              /* 1 = clock seen, 2 = output seen */
    void *output_ud;
    void *clock_ud;
    const char *seen_str;
    elib_log_timestamp_t stamp;
} sink_t;

static elib_log_ctx_t ctx;
static char buf[BUF_SIZE];
static sink_t sink;

static void sink_output(const char *s, void *ud) {
    size_t i = 0u;
    sink_t *k = (sink_t *)ud;

    while (s[i] != '\0' && i + 1u < sizeof(k->last)) {
        k->last[i] = s[i];
        i++;
    }
    k->last[i] = '\0';
    k->output_calls++;
    k->output_ud = ud;
    k->seen_str = s;
    if (k->order == 0u) {
        k->order = 2u;
    }
}

static elib_log_timestamp_t sink_clock(void *ud) {
    sink_t *k = (sink_t *)ud;

    k->clock_calls++;
    k->clock_ud = ud;
    if (k->order == 0u) {
        k->order = 1u;
    }
    return k->stamp;
}

static void reset(uint32_t flags) {
    elib_log_cfg_t cfg = {
        .buf = buf,
        .buf_size = BUF_SIZE,
        .output = sink_output,
        .timestamp = sink_clock,
        .user_data = &sink,
        .level = ELIB_LOG_DEBUG,
        .flags = flags,
    };
    memset(buf, 0, sizeof(buf));
    memset(&sink, 0, sizeof(sink));
    memset(&ctx, 0, sizeof(ctx));
    sink.stamp.s = 3u;
    sink.stamp.ms = 14u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
}

void test_output_gets_user_data(void) {
    printf("Test: the output callback receives user_data... ");
    reset(0u);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(sink.output_calls == 1u);
    assert(sink.output_ud == &sink);
    assert(strcmp(sink.last, "[T]:hi\r\n") == 0);
    printf("PASSED\n");
}

void test_clock_gets_user_data(void) {
    printf("Test: the timestamp callback receives the same user_data... ");
    reset(ELIB_LOG_FLAG_TIMESTAMP);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(sink.clock_calls == 1u);
    assert(sink.clock_ud == &sink);
    assert(strcmp(sink.last, "[3.014][T]:hi\r\n") == 0);
    printf("PASSED\n");
}

void test_clock_runs_before_output(void) {
    printf("Test: the clock is consulted before the transport... ");
    reset(ELIB_LOG_FLAG_TIMESTAMP);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(sink.order == 1u);        /* the clock ran first */
    assert(sink.clock_calls == 1u);
    printf("PASSED\n");
}

void test_clock_once_per_line(void) {
    printf("Test: the clock is consulted once per emitted line... ");
    reset(ELIB_LOG_FLAG_TIMESTAMP);
    for (unsigned i = 0u; i < 4u; i++) {
        assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "line %u", i)
               == ELIB_LOG_OK);
    }
    assert(sink.clock_calls == 4u);
    assert(sink.output_calls == 4u);
    printf("PASSED\n");
}

void test_clock_skipped_when_filtered(void) {
    printf("Test: a filtered line never touches the clock... ");
    reset(ELIB_LOG_FLAG_TIMESTAMP);
    assert(elib_log_set_level(&ctx, ELIB_LOG_ERROR) == ELIB_LOG_OK);
    assert(elib_log_write(&ctx, ELIB_LOG_DEBUG, "T", "noise") == ELIB_LOG_OK);
    assert(sink.clock_calls == 0u);
    assert(sink.output_calls == 0u);
    printf("PASSED\n");
}

void test_output_sees_the_context_buffer(void) {
    printf("Test: the transport receives the caller's buffer... ");
    reset(0u);
    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "T", "hi") == ELIB_LOG_OK);
    assert(sink.seen_str == buf);
    printf("PASSED\n");
}

void test_two_contexts_have_separate_sinks(void) {
    printf("Test: two loggers keep their own user_data and buffer... ");
    static char buf_b[BUF_SIZE];
    static sink_t sink_b;
    elib_log_ctx_t ctx_b;

    reset(0u);
    memset(&sink_b, 0, sizeof(sink_b));
    elib_log_cfg_t cfg = {
        .buf = buf_b,
        .buf_size = BUF_SIZE,
        .output = sink_output,
        .timestamp = NULL,
        .user_data = &sink_b,
        .level = ELIB_LOG_DEBUG,
        .flags = ELIB_LOG_FLAG_EOL_LF,
    };
    assert(elib_log_init(&ctx_b, &cfg, NULL, NULL) == ELIB_LOG_OK);

    assert(elib_log_write(&ctx, ELIB_LOG_INFO, "A", "one") == ELIB_LOG_OK);
    assert(elib_log_write(&ctx_b, ELIB_LOG_INFO, "B", "two") == ELIB_LOG_OK);

    assert(strcmp(sink.last, "[A]:one\r\n") == 0);
    assert(strcmp(sink_b.last, "[B]:two\n") == 0);
    assert(sink.output_ud == &sink);
    assert(sink_b.output_ud == &sink_b);
    printf("PASSED\n");
}

void test_level_is_per_context(void) {
    printf("Test: the runtime level is per context... ");
    static char buf_b[BUF_SIZE];
    static sink_t sink_b;
    elib_log_ctx_t ctx_b;

    reset(0u);
    memset(&sink_b, 0, sizeof(sink_b));
    elib_log_cfg_t cfg = {
        .buf = buf_b,
        .buf_size = BUF_SIZE,
        .output = sink_output,
        .timestamp = NULL,
        .user_data = &sink_b,
        .level = ELIB_LOG_ERROR,
        .flags = 0u,
    };
    assert(elib_log_init(&ctx_b, &cfg, NULL, NULL) == ELIB_LOG_OK);

    assert(elib_log_write(&ctx, ELIB_LOG_DEBUG, "A", "d") == ELIB_LOG_OK);
    assert(elib_log_write(&ctx_b, ELIB_LOG_DEBUG, "B", "d") == ELIB_LOG_OK);

    assert(sink.output_calls == 1u);
    assert(sink_b.output_calls == 0u);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_callbacks ===\n");
    test_output_gets_user_data();
    test_clock_gets_user_data();
    test_clock_runs_before_output();
    test_clock_once_per_line();
    test_clock_skipped_when_filtered();
    test_output_sees_the_context_buffer();
    test_two_contexts_have_separate_sinks();
    test_level_is_per_context();
    printf("=== test_callbacks: all PASSED ===\n");
    return 0;
}
