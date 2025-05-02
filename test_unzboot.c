#include <glib.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Make static functions/variables accessible for testing */
#define static // Remove static keyword for testing purposes
#define main unzboot_original_main // Rename original main before including
#include "unzboot.c"
#undef static // Restore static keyword definition
#undef main // Restore main definition

/*
 * This is a simple gzip compressed string "Hello Gzip Test".
 * The data is generated using the gzip command line tool.
 * Command used:
 *   echo "Hello Gzip Test" | gzip -c | hexdump -v -e '/1 "%02x "'
 * The first two bytes, 0x1f and 0x8b, are the standard "magic number" that
 * identifies a file as being in gzip format.
 */
const guint8 valid_gzip_data[] = {
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0x70, 0xaf, 0xca, 0x2c,
    0x50, 0x08, 0x49, 0x2d, 0x2e, 0xe1, 0x02, 0x00, 0x47, 0x0e,
    0x85, 0x2f, 0x10, 0x00, 0x00, 0x00
};
const char *valid_gzip_uncompressed = "Hello Gzip Test";


/* Test gunzip with valid data */
static void test_gunzip_valid(void) {
    gsize dstlen = 1024;
    gchar *dst = g_malloc(dstlen);
    ssize_t result;

    result = gunzip(dst, dstlen, (uint8_t*)valid_gzip_data, sizeof(valid_gzip_data));

    g_assert_cmpint(result, >, 0); // Expect gunzip to succeed
    g_assert_cmpint(result - 1, ==, strlen(valid_gzip_uncompressed)); // -1 for null terminator, expect same length
    g_assert_cmpmem(dst, result - 1, valid_gzip_uncompressed, strlen(valid_gzip_uncompressed)); // Expect uncompressed data to match

    g_free(dst);
}

/* Test gunzip with insufficient destination buffer */
static void test_gunzip_dst_too_small(void) {
    gsize dstlen = 5; // Too small for "Hello Gzip Test"
    gchar *dst = g_malloc(dstlen);
    ssize_t result;

    result = gunzip(dst, dstlen, (uint8_t*)valid_gzip_data, sizeof(valid_gzip_data));

    /* Expect inflate() to return an error because the buffer is too small */
    g_assert_cmpint(result, ==, -1);

    g_free(dst);
}

/* Test gunzip with invalid header (not gzip) */
static void test_gunzip_invalid_header(void) {
    gsize dstlen = 1024;
    gchar *dst = g_malloc(dstlen);
    ssize_t result;
    const guint8 bad_data[] = { 0x01, 0x02, 0x03, 0x04 }; // Invalid gzip header

    result = gunzip(dst, dstlen, (uint8_t*)bad_data, sizeof(bad_data));

    g_assert_cmpint(result, ==, -1);

    g_free(dst);
}

/* Test gunzip with truncated data (header ok, data missing) */
static void test_gunzip_truncated_data(void) {
    gsize dstlen = 1024;
    gchar *dst = g_malloc(dstlen);
    ssize_t result;

    /* Only provide the first 15 bytes of valid gzip data */
    result = gunzip(dst, dstlen, (uint8_t*)valid_gzip_data, 15);

    /* Expect inflate() to fail because data is incomplete */
    g_assert_cmpint(result, ==, -1);

    g_free(dst);
}

int main(int argc, char **argv) {
    g_test_init(&argc, &argv, NULL);

    /* Add gunzip tests */
    g_test_add_func("/unzboot/gunzip/valid", test_gunzip_valid);
    g_test_add_func("/unzboot/gunzip/dst_too_small", test_gunzip_dst_too_small);
    g_test_add_func("/unzboot/gunzip/invalid_header", test_gunzip_invalid_header);
    g_test_add_func("/unzboot/gunzip/truncated_data", test_gunzip_truncated_data);

    return g_test_run();
}
