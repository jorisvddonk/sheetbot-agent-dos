/*
 * B64ENC.EXE - Base64 encode stdin to stdout
 *
 * Usage: B64ENC < input.exe > output.b64
 *
 * Compile with Open Watcom:
 *   wcl -l=dos b64enc.c -fe=b64enc.exe
 * Compile with DJGPP:
 *   gcc -o b64enc.exe b64enc.c
 *
 * All large buffers are global to avoid stack overflow on 16-bit DOS.
 */

#include <stdio.h>

#define BUFSIZE 16384

static unsigned char g_buf[BUFSIZE];

static const char b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int main(void) {
    int n, i;
    unsigned char a, b, c;
    int col = 0;

    while ((n = fread(g_buf, 1, BUFSIZE, stdin)) > 0) {
        i = 0;
        while (i < n) {
            a = g_buf[i++];
            b = (i < n) ? g_buf[i++] : 0;
            c = (i < n) ? g_buf[i++] : 0;

            putchar(b64[a >> 2]);
            putchar(b64[((a & 3) << 4) | (b >> 4)]);
            putchar((i - 1 < n || n % 3 == 0) ? b64[((b & 0xf) << 2) | (c >> 6)] : '=');
            putchar((i < n || n % 3 == 1) ? b64[c & 0x3f] : '=');

            col += 4;
            if (col >= 76) { putchar('\n'); col = 0; }
        }
    }
    if (col > 0) putchar('\n');
    return 0;
}