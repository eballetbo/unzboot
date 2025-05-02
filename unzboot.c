/*
 * Extract a kernel vmlinuz image from a EFI application that carries the actual kernel image in compressed form
 *
 * Copyright (c) 2023 Enric Balletbo i Serra
 *
 * Unpack EFI zboot image functionality in this file is derived from qemu:
 *
 * Copyright (c) 2006 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Gunzip functionality in this file is derived from u-boot:
 *
 * (C) Copyright 2008 Semihalf
 *
 * (C) Copyright 2000-2005
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <zlib.h>
#include <zstd.h>
 
#define ARM64_MAGIC_OFFSET  56

#define ZALLOC_ALIGNMENT	16

#define HEAD_CRC	        2
#define EXTRA_FIELD         4
#define ORIG_NAME           8
#define COMMENT	            0x10
#define RESERVED            0xe0
#define DEFLATED            8
#define LOAD_IMAGE_MAX_GUNZIP_BYTES (256 << 20)

#define le_bswap(v, size) (v)

/* The PE/COFF MS-DOS stub magic number */
#define EFI_PE_MSDOS_MAGIC        "MZ"

/*
 * The Linux header magic number for a EFI PE/COFF
 * image targetting an unspecified architecture.
 */
#define EFI_PE_LINUX_MAGIC        "\xcd\x23\x82\x81"

/*
 * Bootable Linux kernel images may be packaged as EFI zboot images, which are
 * self-decompressing executables when loaded via EFI. The compressed payload
 * can also be extracted from the image and decompressed by a non-EFI loader.
 *
 * The de facto specification for this format is at the following URL:
 *
 * https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/firmware/efi/libstub/zboot-header.S
 *
 * This definition is based on Linux upstream commit 29636a5ce87beba.
 */
struct linux_efi_zboot_header {
    uint8_t     msdos_magic[2];         /* PE/COFF 'MZ' magic number */
    uint8_t     reserved0[2];
    uint8_t     zimg[4];                /* "zimg" for Linux EFI zboot images */
    uint32_t    payload_offset;         /* LE offset to compressed payload */
    uint32_t    payload_size;           /* LE size of the compressed payload */
    uint8_t     reserved1[8];
    char        compression_type[32];   /* Compression type, NUL terminated */
    uint8_t     linux_magic[4];         /* Linux header magic */
    uint32_t    pe_header_offset;       /* LE offset to the PE header */
};

static inline int ldl_he_p(const void *ptr)
{
    int32_t r;
    memcpy(&r, ptr, sizeof(r));
    return r;
}

static inline int ldl_le_p(const void *ptr)
{
    return le_bswap(ldl_he_p(ptr), 32);
}

static void *zalloc(void *x, unsigned items, unsigned size)
{
    void *p;

    size *= items;
    size = (size + ZALLOC_ALIGNMENT - 1) & ~(ZALLOC_ALIGNMENT - 1);

    p = g_malloc(size);

    return (p);
}

static void zfree(void *x, void *addr)
{
    g_free(addr);
}

static ssize_t gunzip(void *dst, size_t dstlen, uint8_t *src, size_t srclen)
{
    z_stream s;
    ssize_t dstbytes;
    int r, flags;
    size_t i;

    /* skip header */
    i = 10;
    if (srclen < 4) {
        goto toosmall;
    }
    flags = src[3];
    if (src[2] != DEFLATED || (flags & RESERVED) != 0) {
        puts ("Error: Bad gzipped data\n");
        return -1;
    }
    if ((flags & EXTRA_FIELD) != 0) {
        if (srclen < 12) {
            goto toosmall;
        }
        i = 12 + src[10] + (src[11] << 8);
    }
    if ((flags & ORIG_NAME) != 0) {
        while (i < srclen && src[i++] != 0) {
            /* do nothing */
        }
    }
    if ((flags & COMMENT) != 0) {
        while (i < srclen && src[i++] != 0) {
            /* do nothing */
        }
    }
    if ((flags & HEAD_CRC) != 0) {
        i += 2;
    }
    if (i >= srclen) {
        goto toosmall;
    }

    s.zalloc = zalloc;
    s.zfree = zfree;

    r = inflateInit2(&s, -MAX_WBITS);
    if (r != Z_OK) {
        printf ("Error: inflateInit2() returned %d\n", r);
        return (-1);
    }
    s.next_in = src + i;
    s.avail_in = srclen - i;
    s.next_out = dst;
    s.avail_out = dstlen;
    r = inflate(&s, Z_FINISH);
    if (r != Z_OK && r != Z_STREAM_END) {
        printf ("Error: inflate() returned %d\n", r);
        return -1;
    }
    dstbytes = s.next_out - (unsigned char *) dst;
    inflateEnd(&s);

    return dstbytes;

toosmall:
    puts("Error: gunzip out of data in header\n");
    return -1;
}

/*
 * Check whether *buffer points to a Linux EFI zboot image in memory.
 *
 * If it does, attempt to decompress it to a new buffer, and free the old one.
 * If any of this fails, return an error to the caller.
 *
 * If the image is not a Linux EFI zboot image, do nothing and return success.
 */
static ssize_t unpack_efi_zboot_image(uint8_t **buffer, size_t *size)
{
    const struct linux_efi_zboot_header *header;
    uint8_t *data = NULL;
    int ploff, plsize;
    ssize_t bytes;

    /* ignore if this is too small to be a EFI zboot image */
    if (*size < sizeof(*header)) {
        fprintf(stderr, "The input file is too small to be a EFI zboot image\n");
        return 0;
    }

    header = (struct linux_efi_zboot_header *)*buffer;

    /* ignore if this is not a Linux EFI zboot image */
    if (memcmp(&header->msdos_magic, EFI_PE_MSDOS_MAGIC, 2) != 0 ||
        memcmp(&header->zimg, "zimg", 4) != 0 ||
        memcmp(&header->linux_magic, EFI_PE_LINUX_MAGIC, 4) != 0) {
        fprintf(stderr, "The input file is not a Linux EFI zboot image\n");
        return 0;
    }

    ploff = ldl_le_p(&header->payload_offset);
    plsize = ldl_le_p(&header->payload_size);

    if (ploff < 0 || plsize < 0 || (size_t)ploff + (size_t)plsize > *size) {
        fprintf(stderr, "unable to handle corrupt EFI zboot image\n");
        return -1;
    }

    data = g_malloc(LOAD_IMAGE_MAX_GUNZIP_BYTES);

    if (!strcmp(header->compression_type, "gzip")) {
        bytes = gunzip(data, LOAD_IMAGE_MAX_GUNZIP_BYTES, *buffer + ploff, plsize);
        if (bytes < 0) {
            fprintf(stderr, "failed to decompress EFI zboot image with GZIP payload.\n");
            g_free(data);
            return -1;
        }
    } else if (!strcmp(header->compression_type, "zstd22") ||
               !strcmp(header->compression_type, "zstd")) {
      bytes = ZSTD_decompress(data, LOAD_IMAGE_MAX_GUNZIP_BYTES,
                              *buffer + ploff, plsize);
      if (bytes < 0) {
        fprintf(stderr,
                "failed to decompress EFI zboot image with ZSTD payload.\n");
        g_free(data);
        return -1;
      }
    } else {
      fprintf(stderr,
              "unable to handle EFI zboot image with \"%.*s\" compression\n",
              (int)sizeof(header->compression_type) - 1,
              header->compression_type);
      g_free(data);
      return -1;
    }

    g_free(*buffer);
    *buffer = g_realloc(data, bytes);
    *size = bytes;
    return bytes;
}

int main(int argc, char *argv[]) {
    uint8_t *buffer;
    gsize len;
    size_t size;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* input_file = argv[1];
    const char* output_file = argv[2];

    /* Load as raw file otherwise */
    if (!g_file_get_contents(input_file, (char **)&buffer, &len, NULL)) {
        fprintf(stderr, "%s: %s: cannot load input file\n", argv[0], input_file);
        exit(EXIT_FAILURE);
    }
    size = len;

    /* Unpack the image if it is a EFI zboot image */
    if (unpack_efi_zboot_image(&buffer, &size) < 0) {
        g_free(buffer);
        fprintf(stderr, "%s: cannot write to unpack zboot image\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* check the arm64 magic header value -- very old kernels may not have it */
    if (size > ARM64_MAGIC_OFFSET + 4 &&
        (memcmp(buffer + ARM64_MAGIC_OFFSET, "ARM\x64", 4) == 0)) {
        fprintf(stdout, "%s: found ARM64 header\n", argv[0]);
        if (!g_file_set_contents(output_file, (char *)buffer, size, NULL)) {
            g_free(buffer);
            fprintf(stderr, "%s: cannot write to output file\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    } else if (size > ARM64_MAGIC_OFFSET + 4 &&
        (memcmp(buffer + ARM64_MAGIC_OFFSET, "RSC\x05", 4) == 0)) {
        fprintf(stdout, "%s: found RISC-V header\n", argv[0]);
        if (!g_file_set_contents(output_file, (char *)buffer, size, NULL)) {
            g_free(buffer);
            fprintf(stderr, "%s: cannot write to output file\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "%s: %s: cannot find ARM64/RISC-V compressed image\n", argv[0], input_file);
        exit(EXIT_FAILURE);
    }

    g_free(buffer);
    exit(EXIT_SUCCESS);
}
