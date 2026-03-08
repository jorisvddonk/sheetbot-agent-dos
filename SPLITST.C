/*
 * SPLITST.EXE - Split curl -w "%{http_code}" output into body and status
 *
 * curl -w "%{http_code}" appends the 3-digit HTTP status code directly
 * after the response body with no separator. This tool splits them.
 *
 * Usage: SPLITST bodyfile statusfile < rawfile
 *   Writes everything except last 3 bytes to bodyfile.
 *   Writes last 3 bytes (the status code) + newline to statusfile.
 *
 * Example:
 *   curl -s -w "%{http_code}" http://example.com/api > RAW.TXT
 *   SPLITST BODY.JSON STATUS.TXT < RAW.TXT
 *   SET /P STATUS=<STATUS.TXT   -> e.g. 200
 *
 * Compile with Open Watcom:
 *   wcl -l=dos splitst.c -fe=splitst.exe
 * Compile with DJGPP:
 *   gcc -o splitst.exe splitst.c
 *
 * All large buffers are global to avoid stack overflow on 16-bit DOS.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BUFSIZE 16384

static char g_buf[BUFSIZE];

int main(int argc, char *argv[]) {
    FILE *fbody, *fstatus;
    int n, total = 0;

    if (argc < 3) {
        fprintf(stderr, "Usage: SPLITST bodyfile statusfile < rawfile\n");
        return 2;
    }

    /* Read all of stdin */
    while (total < BUFSIZE - 1) {
        n = fread(g_buf + total, 1, BUFSIZE - 1 - total, stdin);
        if (n <= 0) break;
        total += n;
    }

    if (total < 3) {
        fprintf(stderr, "SPLITST: input too short (got %d bytes)\n", total);
        return 1;
    }

    /* Write body (everything except last 3 bytes) */
    fbody = fopen(argv[1], "wb");
    if (!fbody) {
        fprintf(stderr, "SPLITST: cannot open body file: %s\n", argv[1]);
        return 1;
    }
    fwrite(g_buf, 1, total - 3, fbody);
    fclose(fbody);

    /* Write status code (last 3 bytes) + newline */
    fstatus = fopen(argv[2], "w");
    if (!fstatus) {
        fprintf(stderr, "SPLITST: cannot open status file: %s\n", argv[2]);
        return 1;
    }
    g_buf[total] = '\0';
    fprintf(fstatus, "%c%c%c\n", g_buf[total-3], g_buf[total-2], g_buf[total-1]);
    fclose(fstatus);

    return 0;
}
