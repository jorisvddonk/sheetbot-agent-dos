/*
 * AGENT.EXE - Sheetbot DOS Agent Runtime
 *
 * Implements the full agent lifecycle against the Sheetbot API:
 *   1. Authenticate (API key or username/password)
 *   2. Load and merge capabilities
 *   3. Poll for a task
 *   4. Accept task
 *   5. Fetch and execute task script (TASK.BAT)
 *   6. Report failure if script exits non-zero
 *      (script itself calls /complete or /failed on success)
 *
 * External dependencies (must be in PATH or same directory):
 *   curl.exe, jget.exe, jset.exe
 *
 * Environment variables (set before running):
 *   SHEETBOT_BASEURL        e.g. https://myserver.example.com
 *   SHEETBOT_AUTH_APIKEY    API key (preferred)
 *   SHEETBOT_AUTH_USER      Username (alternative)
 *   SHEETBOT_AUTH_PASS      Password (alternative)
 *
 * Compile with Open Watcom:
 *   wcl -l=dos agent.c -fe=agent.exe
 * Compile with DJGPP:
 *   gcc -o agent.exe agent.c
 *
 * All large buffers are global/static to avoid stack overflow on
 * 16-bit DOS (default stack ~4KB).
 *
 * curl usage notes:
 *   - All POST bodies are passed via stdin with @- to avoid DOS
 *     filename issues with @filename syntax.
 *   - No -w "%{http_code}" / splitst - we check for expected output
 *     values instead of HTTP status codes.
 *   - All curl commands written to run.bat to bypass DOS 128-char
 *     command line limit.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Buffer sizes                                                         */
/* ------------------------------------------------------------------ */

#define URL_MAX     512
#define TOKEN_MAX   512
#define VAL_MAX     512
#define ENV_MAX    1100
#define CMD_MAX    2232

/* ------------------------------------------------------------------ */
/* Global buffers - kept off the stack                                 */
/* ------------------------------------------------------------------ */

static char g_baseurl[URL_MAX];
static char g_token[TOKEN_MAX];
static char g_auth_header[TOKEN_MAX + 32];
static char g_task_id[VAL_MAX];
static char g_script_url[URL_MAX];
static char g_cmd[CMD_MAX];
static char g_logbuf[512];

/* persistent putenv buffers */
static char g_env_task_id[ENV_MAX];
static char g_env_auth_hdr[ENV_MAX];
static char g_env_baseurl[ENV_MAX];
static char g_env_task_baseurl[ENV_MAX];
static char g_env_task_accepturl[ENV_MAX];
static char g_env_task_completeurl[ENV_MAX];
static char g_env_task_failedurl[ENV_MAX];
static char g_env_task_dataurl[ENV_MAX];
static char g_env_task_artefacturl[ENV_MAX];

static char g_auth_apikey[VAL_MAX];
static char g_auth_user[VAL_MAX];
static char g_auth_pass[VAL_MAX];
static char g_sys_hostname[VAL_MAX];
static char g_task_url[ENV_MAX];

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/*
 * Write cmd to run.bat and execute it.
 * Bypasses the 128-char DOS system() command line limit.
 */
static int run(const char *cmd) {
    FILE *f;
    int rc;
    f = fopen("run.bat", "w");
    if (!f) { printf("Error: cannot write run.bat\n"); return -1; }
    fprintf(f, "@echo off\n%s\n", cmd);
    fclose(f);
    rc = system("run.bat");
    return rc;
}

/*
 * Run a curl POST command, passing body from infile via stdin.
 * Writes curl config to curlcfg.txt to keep run.bat line short,
 * bypassing both the DOS 128-char command line limit and the
 * DOS batch file line length limit.
 *
 * curlcfg.txt contains:  --data-binary @-
 * run.bat contains:      curl -sk -K curlcfg.txt [url] > outfile < infile
 *
 * Additional curl options (method, headers) are passed via extracfg.
 */
static int curl_post(const char *url, const char *infile,
                     const char *outfile, const char *extra_headers) {
    FILE *f;
    int rc;

    /* Write curl config file - one option per line, no length issues */
    f = fopen("curlcfg.txt", "w");
    if (!f) { printf("Error: cannot write curlcfg.txt\n"); return -1; }
    fprintf(f, "-s\n");
    fprintf(f, "-k\n");
    fprintf(f, "-X POST\n");
    fprintf(f, "--data-binary @-\n");
    fprintf(f, "-H \"Content-Type: application/json\"\n");
    if (extra_headers && extra_headers[0] != '\0') {
        fprintf(f, "-H \"%s\"\n", extra_headers);
    }
    fprintf(f, "url = \"%s\"\n", url);
    fclose(f);

    /* run.bat just invokes curl with the config file and redirects */
    f = fopen("run.bat", "w");
    if (!f) { printf("Error: cannot write run.bat\n"); return -1; }
    fprintf(f, "@echo off\n");
    fprintf(f, "curl -K curlcfg.txt > %s < %s\n", outfile, infile);
    fclose(f);

    rc = system("run.bat");
    return rc;
}



/* Read the first whitespace-delimited token from a file into dst. */
static int read_token(const char *filename, char *dst, int dstlen) {
    FILE *f = fopen(filename, "r");
    char fmt[16];
    if (!f) { dst[0] = '\0'; return 0; }
    sprintf(fmt, "%%%ds", dstlen - 1);
    if (fscanf(f, fmt, dst) != 1) { dst[0] = '\0'; fclose(f); return 0; }
    fclose(f);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Capability loading                                                   */
/* ------------------------------------------------------------------ */

static void load_capabilities(void) {
    FILE *f;

    run("jset > caps.json");

    f = fopen(".capabilities.json", "r");
    if (f) { fclose(f); printf("Loading .capabilities.json\n"); run("copy .capabilities.json caps.json >NUL"); }

    f = fopen(".capabilities.dynamic.bat", "r");
    if (f) { fclose(f); printf("Running .capabilities.dynamic.bat\n"); run("call .capabilities.dynamic.bat caps.json"); }

    f = fopen(".capabilities.override.json", "r");
    if (f) { fclose(f); printf("Loading .capabilities.override.json\n"); run("copy .capabilities.override.json caps.json >NUL"); }
}

/* ------------------------------------------------------------------ */
/* Cleanup temp files                                                   */
/* ------------------------------------------------------------------ */

static void cleanup(void) {
    remove("loginreq.json"); remove("loginresp.json");
    remove("caps.json");     remove("syscaps.json");
    remove("pollreq.json");  remove("taskresp.json");
    remove("scripturl.txt"); remove("taskid.txt");
    remove("token.txt");     remove("task.bat");
    remove("run.bat");       remove("hname.txt");
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void) {
    char *env;
    int rc;

    printf("Sheetbot DOS Agent starting\n");

    /* ---- Read required environment ---- */
    env = getenv("SHEETBOT_BASEURL");
    if (!env || env[0] == '\0') { printf("Error: SHEETBOT_BASEURL is not set\n"); return 1; }
    strncpy(g_baseurl, env, URL_MAX - 1);
    printf("Using server: %s\n", g_baseurl);

    g_auth_apikey[0] = g_auth_user[0] = g_auth_pass[0] = '\0';
    env = getenv("SHEETBOT_AUTH_APIKEY"); if (env) strncpy(g_auth_apikey, env, VAL_MAX - 1);
    env = getenv("SHEETBOT_AUTH_USER");   if (env) strncpy(g_auth_user,   env, VAL_MAX - 1);
    env = getenv("SHEETBOT_AUTH_PASS");   if (env) strncpy(g_auth_pass,   env, VAL_MAX - 1);

    /* Scrub credentials from environment immediately after reading */
    putenv("SHEETBOT_AUTH_APIKEY=");
    putenv("SHEETBOT_AUTH_USER=");
    putenv("SHEETBOT_AUTH_PASS=");

    /* ---- Authentication ---- */
    g_token[0] = g_auth_header[0] = '\0';

    if (g_auth_apikey[0] != '\0') {
        printf("Attempting authentication with API key\n");

        /* Build login payload, POST via stdin pipe */
        snprintf(g_cmd, CMD_MAX, "jset apiKey %s > loginreq.json", g_auth_apikey);
        run(g_cmd);

        snprintf(g_task_url, ENV_MAX, "%s/login", g_baseurl);
        rc = curl_post(g_task_url, "loginreq.json", "loginresp.json", "");
        printf("Login curl exit: %d\n", rc);
        if (rc != 0) { printf("Error: curl failed during login\n"); cleanup(); return 1; }

    } else if (g_auth_user[0] != '\0') {
        printf("Attempting authentication with user: %s\n", g_auth_user);

        snprintf(g_cmd, CMD_MAX,
                 "jset username %s password %s > loginreq.json", g_auth_user, g_auth_pass);
        run(g_cmd);

        snprintf(g_task_url, ENV_MAX, "%s/login", g_baseurl);
        rc = curl_post(g_task_url, "loginreq.json", "loginresp.json", "");
        printf("Login curl exit: %d\n", rc);
        if (rc != 0) { printf("Error: curl failed during login\n"); cleanup(); return 1; }

    } else {
        printf("No authentication credentials provided\n");
        goto auth_done;
    }

    /* Print response for debugging */
    printf("Login response: ");
    {
        FILE *f = fopen("loginresp.json", "r");
        if (f) {
            while (fgets(g_logbuf, sizeof(g_logbuf), f)) printf("%s", g_logbuf);
            fclose(f);
        }
        printf("\n");
    }

    run("jget token < loginresp.json > token.txt");
    read_token("token.txt", g_token, TOKEN_MAX);
    if (g_token[0] == '\0') { printf("Error: no token in auth response\n"); cleanup(); return 1; }
    printf("Token obtained\n");
    snprintf(g_auth_header, sizeof(g_auth_header), "Authorization: Bearer %s", g_token);

auth_done:
    /* Scrub auth variables from memory */
    memset(g_auth_apikey, 0, VAL_MAX);
    memset(g_auth_user,   0, VAL_MAX);
    memset(g_auth_pass,   0, VAL_MAX);

    /* ---- Load capabilities ---- */
    load_capabilities();

    /* Get hostname */
    /* FreeDOS doesn't have a hostname command.
     * Try COMPUTERNAME env var (set by some FreeDOS configs), else use "freedos". */
    g_sys_hostname[0] = '\0';
    env = getenv("COMPUTERNAME");
    if (env && env[0] != '\0') strncpy(g_sys_hostname, env, VAL_MAX - 1);
    if (g_sys_hostname[0] == '\0') strcpy(g_sys_hostname, "freedos");
    printf("Hostname: %s\n", g_sys_hostname);

    /* Build pollreq.json directly - avoid chained @file refs in jset
     * which produce truncated JSON when intermediate files are missing. */
    snprintf(g_cmd, CMD_MAX,
             "jset type freedos-bat capabilities {} os freedos arch x86 hostname %s > pollreq.json",
             g_sys_hostname);
    run(g_cmd);
    printf("Capabilities built\n");

    /* ---- Poll for tasks ---- */
    printf("Polling for tasks at: %s/tasks/get\n", g_baseurl);

    snprintf(g_task_url, ENV_MAX, "%s/tasks/get", g_baseurl);
    rc = curl_post(g_task_url, "pollreq.json", "taskresp.json", g_auth_header);
    printf("Poll curl exit: %d\n", rc);
    if (rc != 0) { printf("Error: curl failed during task poll\n"); cleanup(); return 1; }

    /* ---- Check if a task is available ---- */
    rc = run("jget script < taskresp.json > scripturl.txt");
    if (rc != 0) { printf("No task available\n"); cleanup(); return 0; }
    read_token("scripturl.txt", g_script_url, URL_MAX);
    if (g_script_url[0] == '\0') { printf("No task available\n"); cleanup(); return 0; }

    run("jget id < taskresp.json > taskid.txt");
    read_token("taskid.txt", g_task_id, VAL_MAX);
    printf("Task received: %s\n", g_task_id);

    /* ---- Set task environment variables ---- */
    snprintf(g_env_task_id,          ENV_MAX, "SHEETBOT_TASK_ID=%s",                           g_task_id);
    snprintf(g_env_auth_hdr,         ENV_MAX, "SHEETBOT_AUTHORIZATION_HEADER=Bearer %s",        g_token);
    snprintf(g_env_baseurl,          ENV_MAX, "SHEETBOT_BASEURL=%s",                            g_baseurl);
    snprintf(g_env_task_baseurl,     ENV_MAX, "SHEETBOT_TASK_BASEURL=%s/tasks/%s",              g_baseurl, g_task_id);
    snprintf(g_env_task_accepturl,   ENV_MAX, "SHEETBOT_TASK_ACCEPTURL=%s/tasks/%s/accept",     g_baseurl, g_task_id);
    snprintf(g_env_task_completeurl, ENV_MAX, "SHEETBOT_TASK_COMPLETEURL=%s/tasks/%s/complete", g_baseurl, g_task_id);
    snprintf(g_env_task_failedurl,   ENV_MAX, "SHEETBOT_TASK_FAILEDURL=%s/tasks/%s/failed",     g_baseurl, g_task_id);
    snprintf(g_env_task_dataurl,     ENV_MAX, "SHEETBOT_TASK_DATAURL=%s/tasks/%s/data",         g_baseurl, g_task_id);
    snprintf(g_env_task_artefacturl, ENV_MAX, "SHEETBOT_TASK_ARTEFACTURL=%s/tasks/%s/artefacts",g_baseurl, g_task_id);
    putenv(g_env_task_id);
    putenv(g_env_auth_hdr);
    putenv(g_env_baseurl);
    putenv(g_env_task_baseurl);
    putenv(g_env_task_accepturl);
    putenv(g_env_task_completeurl);
    putenv(g_env_task_failedurl);
    putenv(g_env_task_dataurl);
    putenv(g_env_task_artefacturl);

    /* ---- Accept task ---- */
    printf("Accepting task\n");
    snprintf(g_task_url, ENV_MAX, "%s/tasks/%s/accept", g_baseurl, g_task_id);
    /* Write empty JSON body for accept */
    {
        FILE *ef = fopen("empty.json", "w");
        if (ef) { fprintf(ef, "{}\n"); fclose(ef); }
    }
    curl_post(g_task_url, "empty.json", "NUL", g_auth_header);
    printf("Task accepted\n");

    /* ---- Fetch script ---- */
    printf("Fetching script from: %s\n", g_script_url);
    if (g_auth_header[0] != '\0') {
        snprintf(g_cmd, CMD_MAX,
                 "curl -sk -H \"%s\" \"%s\" > task.bat",
                 g_auth_header, g_script_url);
    } else {
        snprintf(g_cmd, CMD_MAX, "curl -sk \"%s\" > task.bat", g_script_url);
    }
    run(g_cmd);

    /* ---- Execute script ---- */
    /* Script calls /complete or /failed itself via SHEETBOT_TASK_* env vars.
     * We only call /failed here if the script returns a non-zero exit code. */
    printf("Executing task script\n");
    rc = run("task.bat");

    if (rc != 0) {
        printf("Task script failed (exit %d), reporting failure\n", rc);
        snprintf(g_task_url, ENV_MAX, "%s/tasks/%s/failed", g_baseurl, g_task_id);
        curl_post(g_task_url, "empty.json", "NUL", g_auth_header);
        cleanup();
        return 1;
    }

    printf("Task script finished successfully\n");
    cleanup();
    return 0;
}