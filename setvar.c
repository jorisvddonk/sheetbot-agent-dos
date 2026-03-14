/*
 * SETVAR.EXE - Read a value from stdin, write a SET statement to a .BAT file
 *
 * Usage: SETVAR VARNAME < value.txt
 *   Writes "SET VARNAME=<value>" to SETVAR.BAT
 *   Caller then does: CALL SETVAR.BAT
 *
 * Example:
 *   jget data.x < taskdata.json | SETVAR COORD_X
 *   CALL SETVAR.BAT
 *   -> COORD_X is now set to the value of data.x
 *
 * Compile with Open Watcom:
 *   wcl -l=dos setvar.c -fe=setvar.exe
 * Compile with DJGPP:
 *   gcc -o setvar.exe setvar.c
 *
 * All large buffers are global to avoid stack overflow on 16-bit DOS.
 */

#include <stdio.h>
#include <string.h>

#define BUFSIZE 512

static char g_val[BUFSIZE];

int main(int argc, char *argv[]) {
    FILE *f;
    int n, total = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: SETVAR VARNAME < value.txt\n");
        return 2;
    }

    /* Read value from stdin */
    while (total < BUFSIZE - 1) {
        n = fread(g_val + total, 1, BUFSIZE - 1 - total, stdin);
        if (n <= 0) break;
        total += n;
    }
    g_val[total] = '\0';

    /* Trim trailing whitespace/newlines */
    while (total > 0 &&
           (g_val[total-1] == '\n' || g_val[total-1] == '\r' ||
            g_val[total-1] == ' '  || g_val[total-1] == '\t')) {
        g_val[--total] = '\0';
    }

    /* Write SET statement to SETVAR.BAT */
    f = fopen("setvar.bat", "w");
    if (!f) {
        fprintf(stderr, "SETVAR: cannot write setvar.bat\n");
        return 1;
    }
    fprintf(f, "SET %s=%s\n", argv[1], g_val);
    fclose(f);

    return 0;
}