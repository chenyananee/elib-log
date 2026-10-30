/* test_hex.c --- hex dump rendering, lengths, bounds and filtering */
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

static void reset(uint32_t flags, elib_log_level_t level) {
    elib_log_cfg_t cfg = {
        .buf = buf,
        .buf_size = BUF_SIZE,
        .output = capture,
        .timestamp = NULL,
        .user_data = NULL,
        .level = level,
        .flags = flags,
    };
    memset(buf, 0, sizeof(buf));
    memset(last, 0, sizeof(last));
    memset(&ctx, 0, sizeof(ctx));
    out_count = 0u;
    assert(elib_log_init(&ctx, &cfg, NULL, NULL) == ELIB_LOG_OK);
}

void test_spec_example(void) {
    printf("Test: the documented dump form \"02 89 A6 FF 5E\"... ");
    static const uint8_t data[5] = { 0x02u, 0x89u, 0xA6u, 0xFFu, 0x5Eu };
    reset(0u, ELIB_LOG_DEBUG);
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, "Sensor", data, 5u) == ELIB_LOG_OK);
    assert(strcmp(last, "[Sensor]:02 89 A6 FF 5E\r\n") == 0);
    printf("PASSED\n");
}

void test_uppercase_digits(void) {
    printf("Test: hex digits are upper case with no 0x prefix... ");
    static const uint8_t data[6] = { 0x00u, 0x0Fu, 0x9Au, 0xABu, 0xCDu, 0xEFu };
    reset(0u, ELIB_LOG_DEBUG);
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, NULL, data, 6u) == ELIB_LOG_OK);
    assert(strcmp(last, "00 0F 9A AB CD EF\r\n") == 0);
    printf("PASSED\n");
}

void test_single_byte(void) {
    printf("Test: a single byte has no separator... ");
    static const uint8_t data[1] = { 0x7Fu };
    reset(0u, ELIB_LOG_DEBUG);
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, "H", data, 1u) == ELIB_LOG_OK);
    assert(strcmp(last, "[H]:7F\r\n") == 0);
    printf("PASSED\n");
}

void test_zero_length(void) {
    printf("Test: a zero-length dump renders the prefix only... ");
    reset(0u, ELIB_LOG_DEBUG);
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, "H", NULL, 0u) == ELIB_LOG_OK);
    assert(strcmp(last, "[H]:\r\n") == 0);

    static const uint8_t data[1] = { 0x01u };
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, "H", data, 0u) == ELIB_LOG_OK);
    assert(strcmp(last, "[H]:\r\n") == 0);

    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, NULL, NULL, 0u) == ELIB_LOG_OK);
    assert(strcmp(last, "\r\n") == 0);
    printf("PASSED\n");
}

void test_null_data_with_length_rejected(void) {
    printf("Test: NULL data with a non-zero length is rejected... ");
    reset(0u, ELIB_LOG_DEBUG);
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, "H", NULL, 1u)
           == ELIB_LOG_ERR_INVALID_PARAM);
    assert(out_count == 0u);
    printf("PASSED\n");
}

void test_null_ctx_rejected(void) {
    printf("Test: a NULL context is rejected... ");
    reset(0u, ELIB_LOG_DEBUG);
    assert(elib_log_hex(NULL, ELIB_LOG_INFO, "H", NULL, 0u)
           == ELIB_LOG_ERR_INVALID_PARAM);
    assert(out_count == 0u);
    printf("PASSED\n");
}

void test_invalid_level_rejected(void) {
    printf("Test: an out-of-range level is rejected... ");
    reset(0u, ELIB_LOG_DEBUG);
    assert(elib_log_hex(&ctx, (elib_log_level_t)4, "H", NULL, 0u)
           == ELIB_LOG_ERR_INVALID_PARAM);
    assert(out_count == 0u);
    printf("PASSED\n");
}

void test_long_dump_fits(void) {
    printf("Test: a dump that fits is emitted in full... ");
    static const uint8_t data[16] = {
        0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
        0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu, 0x0Du, 0x0Eu, 0x0Fu
    };
    reset(0u, ELIB_LOG_DEBUG);
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, NULL, data, 16u) == ELIB_LOG_OK);
    assert(strcmp(last,
                  "00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\r\n") == 0);
    printf("PASSED\n");
}

void test_length_is_uint32(void) {
    printf("Test: the length parameter accepts values above 65535... ");
    static uint8_t data[256];
    for (uint32_t i = 0u; i < 256u; i++) {
        data[i] = (uint8_t)i;
    }
    reset(0u, ELIB_LOG_DEBUG);
    /* 0x10000 would overflow a uint16_t length; the buffer truncates first,
     * which proves the parameter is not narrowed before the loop */
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, NULL, data, 0x10000u)
           == ELIB_LOG_ERR_TRUNCATED);
    assert(out_count == 1u);
    printf("PASSED\n");
}

void test_hex_output_is_one_call(void) {
    printf("Test: the whole dump is handed to the transport once... ");
    static const uint8_t data[8] = {
        0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u, 0x70u, 0x80u
    };
    reset(0u, ELIB_LOG_DEBUG);
    assert(elib_log_hex(&ctx, ELIB_LOG_INFO, "D", data, 8u) == ELIB_LOG_OK);
    assert(out_count == 1u);
    assert(strcmp(last, "[D]:10 20 30 40 50 60 70 80\r\n") == 0);
    printf("PASSED\n");
}

void test_hex_macro(void) {
    printf("Test: the ELIB_LOG_HEX macro forwards the arguments... ");
    static const uint8_t data[2] = { 0x0Au, 0xB0u };
    reset(0u, ELIB_LOG_DEBUG);
    ELIB_LOG_HEX(&ctx, ELIB_LOG_ERROR, "M", data, 2u);
    assert(strcmp(last, "[M]:0A B0\r\n") == 0);
    assert(out_count == 1u);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_hex ===\n");
    test_spec_example();
    test_uppercase_digits();
    test_single_byte();
    test_zero_length();
    test_null_data_with_length_rejected();
    test_null_ctx_rejected();
    test_invalid_level_rejected();
    test_long_dump_fits();
    test_length_is_uint32();
    test_hex_output_is_one_call();
    test_hex_macro();
    printf("=== test_hex: all PASSED ===\n");
    return 0;
}
