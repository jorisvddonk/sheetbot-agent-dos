#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BUFSIZE 16384

static char g_json[BUFSIZE];
static char g_out[BUFSIZE];
static char g_subbuf[BUFSIZE];
static char g_needle[256];
static char g_segment[128];

static int read_stdin(void) {
    int total = 0, n;
    while (total < BUFSIZE - 1) {
        n = fread(g_json + total, 1, BUFSIZE - 1 - total, stdin);
        if (n <= 0) break;
        total += n;
    }
    g_json[total] = '\0';
    return total;
}

static int json_get_string(const char *key, const char *json, char *out, int outlen) {
    char needle[256];
    const char *p;
    int i;
    if (strlen(key) + 3 >= sizeof(needle)) return 0;
    needle[0] = '"';
    strcpy(needle + 1, key);
    strcat(needle, "\"");
    p = json;
    while (*p) {
        p = strstr(p, needle);
        if (!p) return 0;
        p += strlen(needle);
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p != ':') continue;
        p++;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p != '"') continue;
        p++;
        i = 0;
        while (*p && i < outlen - 1) {
            if (*p == '\\' && *(p + 1)) {
                p++;
                switch (*p) {
                    case '"':  out[i++] = '"';  break;
                    case '\\': out[i++] = '\\'; break;
                    case '/':  out[i++] = '/';  break;
                    case 'n':  out[i++] = '\n'; break;
                    case 'r':  out[i++] = '\r'; break;
                    case 't':  out[i++] = '\t'; break;
                    default:   out[i++] = *p;   break;
                }
                p++;
            } else if (*p == '"') {
                break;
            } else {
                out[i++] = *p++;
            }
        }
        out[i] = '\0';
        return 1;
    }
    return 0;
}

static int json_get_path(const char *path) {
    const char *p = path;
    const char *dot;
    const char *current_json = g_json;
    const char *sp;
    int depth, i, seglen;
    while (1) {
        dot = strchr(p, '.');
        if (!dot) {
            return json_get_string(p, current_json, g_out, BUFSIZE);
        }
        seglen = (int)(dot - p);
        if (seglen >= (int)sizeof(g_segment)) return 0;
        strncpy(g_segment, p, seglen);
        g_segment[seglen] = '\0';
        p = dot + 1;
        g_needle[0] = '"';
        strcpy(g_needle + 1, g_segment);
        strcat(g_needle, "\"");
        sp = strstr(current_json, g_needle);
        if (!sp) return 0;
        sp += strlen(g_needle);
        while (*sp == ' ' || *sp == '\t' || *sp == '\r' || *sp == '\n') sp++;
        if (*sp != ':') return 0;
        sp++;
        while (*sp == ' ' || *sp == '\t' || *sp == '\r' || *sp == '\n') sp++;
        if (*sp != '{') return 0;
        sp++;
        depth = 1;
        i = 0;
        g_subbuf[i++] = '{';
        while (*sp && depth > 0 && i < BUFSIZE - 2) {
            if (*sp == '"') {
                g_subbuf[i++] = *sp++;
                while (*sp && *sp != '"') {
                    if (*sp == '\\' && *(sp+1)) { g_subbuf[i++] = *sp++; }
                    g_subbuf[i++] = *sp++;
                }
                if (*sp == '"') g_subbuf[i++] = *sp++;
                continue;
            }
            if (*sp == '{') depth++;
            else if (*sp == '}') { depth--; if (depth == 0) break; }
            g_subbuf[i++] = *sp++;
        }
        g_subbuf[i++] = '}';
        g_subbuf[i] = '\0';
        current_json = g_subbuf;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: JGET keyname < input.json\n");
        fprintf(stderr, "  Dot notation: JGET os.os < input.json\n");
        return 2;
    }
    if (read_stdin() <= 0) return 1;
    if (json_get_path(argv[1])) {
        printf("%s\n", g_out);
        return 0;
    }
    return 1;
}
