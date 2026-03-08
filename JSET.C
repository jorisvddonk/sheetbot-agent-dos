/*
 * JSET.EXE - Simple JSON object builder for DOS
 * Usage: JSET key1 value1 [key2 value2 ...]
 *        Prints a flat JSON object to stdout.
 *
 * Special value tokens (not quoted in output):
 *   {}     - empty object literal
 *   []     - empty array literal
 *   true   - boolean true
 *   false  - boolean false
 *   null   - null
 *   @file  - read raw value from file (for pre-built JSON fragments)
 *
 * Examples:
 *   JSET type bat                         -> {"type":"bat"}
 *   JSET apiKey abc123                    -> {"apiKey":"abc123"}
 *   JSET type bat capabilities {}         -> {"type":"bat","capabilities":{}}
 *   JSET data @result.json                -> {"data":{...file contents...}}
 *   JSET result true count 42             -> {"result":true,"count":"42"}
 *
 * Note: numeric values are quoted as strings unless you need them bare.
 * For bare numbers, prefix with = : JSET count =42 -> {"count":42}
 *
 * Compile with Open Watcom:
 *   wcl -l=dos jset.c -o jset.exe
 * Compile with DJGPP:
 *   gcc -o jset.exe jset.c
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXPAIRS  64
#define VALBUFSIZE 8192

/* Tokens that should NOT be quoted as strings */
static int is_literal(const char *val) {
    if (strcmp(val, "{}") == 0)    return 1;
    if (strcmp(val, "[]") == 0)    return 1;
    if (strcmp(val, "true") == 0)  return 1;
    if (strcmp(val, "false") == 0) return 1;
    if (strcmp(val, "null") == 0)  return 1;
    return 0;
}

/* Read file contents into buf. Returns bytes read or -1. */
static int read_file(const char *filename, char *buf, int maxlen) {
    FILE *f;
    int n, total = 0;

    f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "JSET: cannot open file: %s\n", filename);
        return -1;
    }
    while (total < maxlen - 1) {
        n = fread(buf + total, 1, maxlen - 1 - total, f);
        if (n <= 0) break;
        total += n;
    }
    buf[total] = '\0';
    fclose(f);

    /* Trim trailing whitespace/newlines */
    while (total > 0 && (buf[total-1] == '\n' || buf[total-1] == '\r' ||
                          buf[total-1] == ' '  || buf[total-1] == '\t')) {
        buf[--total] = '\0';
    }
    return total;
}

/* Write a JSON-escaped string value (without surrounding quotes) to stdout */
static void write_escaped(const char *s) {
    while (*s) {
        switch (*s) {
            case '"':  fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\n': fputs("\\n",  stdout); break;
            case '\r': fputs("\\r",  stdout); break;
            case '\t': fputs("\\t",  stdout); break;
            default:
                /* Control characters */
                if ((unsigned char)*s < 0x20) {
                    printf("\\u%04x", (unsigned char)*s);
                } else {
                    fputc(*s, stdout);
                }
                break;
        }
        s++;
    }
}

int main(int argc, char *argv[]) {
    static char filebuf[VALBUFSIZE];
    int i;
    int first = 1;

    if (argc < 2) {
        /* No arguments - emit empty object */
        printf("{}\n");
        return 0;
    }

    if ((argc - 1) % 2 != 0) {
        fprintf(stderr, "JSET: arguments must be key-value pairs\n");
        fprintf(stderr, "Usage: JSET key1 value1 [key2 value2 ...]\n");
        return 2;
    }

    fputc('{', stdout);

    for (i = 1; i < argc; i += 2) {
        const char *key = argv[i];
        const char *val = argv[i + 1];

        if (!first) fputc(',', stdout);
        first = 0;

        /* Write key */
        fputc('"', stdout);
        write_escaped(key);
        fputc('"', stdout);
        fputc(':', stdout);

        /* Write value */
        if (val[0] == '@') {
            /* File reference - read raw JSON from file */
            if (read_file(val + 1, filebuf, VALBUFSIZE) < 0) {
                return 1;
            }
            fputs(filebuf, stdout);
        } else if (val[0] == '=' && val[1] != '\0') {
            /* Bare/numeric value - emit without quotes */
            fputs(val + 1, stdout);
        } else if (is_literal(val)) {
            /* Literal token - emit without quotes */
            fputs(val, stdout);
        } else {
            /* Regular string value - quote and escape */
            fputc('"', stdout);
            write_escaped(val);
            fputc('"', stdout);
        }
    }

    fputc('}', stdout);
    fputc('\n', stdout);

    return 0;
}
