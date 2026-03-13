/*
 * JSONTXT.EXE - Wrap stdin as a JSON string value
 *
 * Reads one line from stdin, JSON-escapes it, and outputs it
 * wrapped in a complete {"data":{"key":"value"}} structure.
 *
 * Usage: JSONTXT keyname < input.txt > output.json
 *   e.g. JSONTXT date < datestr.txt > done.json
 *        -> {"data":{"date":"Current date is Fri 03-13-2026"}}
 *
 * Compile with Open Watcom:
 *   wcl -l=dos jsontxt.c -fe=jsontxt.exe
 * Compile with DJGPP:
 *   gcc -o jsontxt.exe jsontxt.c
 *
 * All large buffers are global to avoid stack overflow on 16-bit DOS.
 */

#include <stdio.h>
#include <string.h>

#define BUFSIZE 1024

static char g_buf[BUFSIZE];

int main(int argc, char *argv[]) {
    int total = 0, n, i;
    const char *key = "value";

    if (argc >= 2) key = argv[1];

    /* Read stdin */
    while (total < BUFSIZE - 1) {
        n = fread(g_buf + total, 1, BUFSIZE - 1 - total, stdin);
        if (n <= 0) break;
        total += n;
    }
    g_buf[total] = '\0';

    /* Trim trailing whitespace/newlines */
    while (total > 0 &&
           (g_buf[total-1] == '\n' || g_buf[total-1] == '\r' ||
            g_buf[total-1] == ' '  || g_buf[total-1] == '\t')) {
        g_buf[--total] = '\0';
    }

    /* Output JSON, escaping special chars in value */
    printf("{\"data\":{\"%s\":\"", key);
    for (i = 0; i < total; i++) {
        switch (g_buf[i]) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\n': printf("\\n");  break;
            case '\r': printf("\\r");  break;
            case '\t': printf("\\t");  break;
            default:   fputc(g_buf[i], stdout); break;
        }
    }
    printf("\"}}\n");
    return 0;
}