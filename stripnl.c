/*
 * STRIPNL.EXE - Strip newlines and carriage returns from stdin
 * Writes result to stdout with all \r and \n replaced by spaces,
 * then trims leading/trailing whitespace.
 *
 * Usage: STRIPNL < input.txt > output.txt
 *   or:  STRIPNL < input.txt
 *
 * Compile with Open Watcom:
 *   wcl -l=dos stripnl.c -fe=stripnl.exe
 * Compile with DJGPP:
 *   gcc -o stripnl.exe stripnl.c
 *
 * All large buffers are global to avoid stack overflow on 16-bit DOS.
 */

#include <stdio.h>
#include <string.h>

#define BUFSIZE 1024

static char g_buf[BUFSIZE];
static char g_out[BUFSIZE];

int main(void) {
    int total = 0, n, i, start, end;

    /* Read stdin */
    while (total < BUFSIZE - 1) {
        n = fread(g_buf + total, 1, BUFSIZE - 1 - total, stdin);
        if (n <= 0) break;
        total += n;
    }
    g_buf[total] = '\0';

    /* Replace \r and \n with spaces */
    for (i = 0; i < total; i++) {
        if (g_buf[i] == '\r' || g_buf[i] == '\n')
            g_buf[i] = ' ';
    }

    /* Copy to out, trimming leading and trailing spaces */
    start = 0;
    while (start < total && g_buf[start] == ' ') start++;
    end = total - 1;
    while (end > start && g_buf[end] == ' ') end--;

    n = 0;
    for (i = start; i <= end; i++)
        g_out[n++] = g_buf[i];
    g_out[n] = '\0';

    printf("%s\n", g_out);
    return 0;
}