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
 *   curl.exe, jget.exe, jset.exe, splitst.exe
 *
 * Environment variables (set before running):
 *   SHEETBOT_BASEURL        e.g. http://myserver.example.com
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
static char g_auth_header[TOKEN_MAX + 32]; /* "Authorization: Bearer " + token */
static char g_task_id[VAL_MAX];
static char g_script_url[URL_MAX];
static char g_status[8];
static char g_cmd[CMD_MAX];

/*
 * NOTE on putenv() and g_envbuf:
 * putenv() on DOS runtimes typically stores a pointer to the string you pass,
 * not a copy. Each call to putenv() therefore needs its own persistent buffer.
 * We declare separate static buffers for each env var we set.
 */
static char g_env_task_id[ENV_MAX];
static char g_env_auth_hdr[ENV_MAX];
static char g_env_baseurl[ENV_MAX];
static char g_env_task_baseurl[ENV_MAX];
static char g_env_task_accepturl[ENV_MAX];
static char g_env_task_completeurl[ENV_MAX];
static char g_env_task_failedurl[ENV_MAX];
static char g_env_task_dataurl[ENV_MAX];
static char g_auth_apikey[VAL_MAX];
static char g_auth_user[VAL_MAX];
static char g_auth_pass[VAL_MAX];
static char g_sys_hostname[VAL_MAX];
static char g_curl_args[CMD_MAX];
static char g_task_url[ENV_MAX];
static char g_env_task_artefacturl[ENV_MAX];

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static int run(const char *cmd) {
    return system(cmd);
}

/* Read the first whitespace-delimited token from a file into dst. */
static int read_token_from_file(const char *filename, char *dst, int dstlen) {
    FILE *f = fopen(filename, "r");
    char fmt[16];
    if (!f) { dst[0] = '\0'; return 0; }
    /* Build format string with correct width to avoid scanf overflow */
    sprintf(fmt, "%%%ds", dstlen - 1);
    if (fscanf(f, fmt, dst) != 1) { dst[0] = '\0'; fclose(f); return 0; }
    fclose(f);
    return 1;
}

/* Print message and return non-zero for HTTP error statuses. */
static int check_status(const char *status, const char *context) {
    if (strcmp(status, "200") == 0) return 0;
    if (strcmp(status, "201") == 0) return 0;
    if (strcmp(status, "204") == 0) return 0;
    if (strcmp(status, "401") == 0) { printf("Error [%s]: Unauthenticated (401)\n",      context); return 1; }
    if (strcmp(status, "403") == 0) { printf("Error [%s]: Unauthorized (403)\n",         context); return 1; }
    if (strcmp(status, "404") == 0) { printf("Error [%s]: Not found (404)\n",            context); return 1; }
    if (strcmp(status, "500") == 0) { printf("Error [%s]: Internal server error (500)\n",context); return 1; }
    printf("Error [%s]: HTTP %s\n", context, status);
    return 1;
}

/*
 * Run curl with -w "%{http_code}", pipe through splitst.
 * g_curl_args: everything after "curl -sk -w \"%{http_code}\" "
 * Reads status into g_status.
 */
static int curl_split(const char *g_curl_args,
                      const char *body_file,
                      const char *status_file) {
    int rc;
    snprintf(g_cmd, CMD_MAX,
             "curl -sk -w \"%%{http_code}\" %s > RAW.TXT", g_curl_args);
    rc = run(g_cmd);
    if (rc != 0) return rc;
    snprintf(g_cmd, CMD_MAX, "splitst %s %s < RAW.TXT", body_file, status_file);
    rc = run(g_cmd);
    if (rc != 0) return rc;
    read_token_from_file(status_file, g_status, sizeof(g_status));
    return 0;
}

/* Build a curl args string for an authenticated or unauthenticated request. */
static void curl_args_post(char *buf, int buflen,
                           const char *url, const char *data) {
    if (g_auth_header[0] != '\0') {
        snprintf(buf, buflen,
                 "-X POST \"%s\" -H \"Content-Type: application/json\" -H \"%s\" -d %s",
                 url, g_auth_header, data);
    } else {
        snprintf(buf, buflen,
                 "-X POST \"%s\" -H \"Content-Type: application/json\" -d %s",
                 url, data);
    }
}

/* ------------------------------------------------------------------ */
/* Capability loading                                                   */
/* ------------------------------------------------------------------ */

static void load_capabilities(void) {
    FILE *f;

    run("jset > CAPS.JSON");

    f = fopen(".capabilities.json", "r");
    if (f) {
        fclose(f);
        printf("Loading .capabilities.json\n");
        run("copy .capabilities.json CAPS.JSON >NUL");
    }

    f = fopen(".capabilities.dynamic.bat", "r");
    if (f) {
        fclose(f);
        printf("Running .capabilities.dynamic.bat\n");
        run("call .capabilities.dynamic.bat CAPS.JSON");
    }

    f = fopen(".capabilities.override.json", "r");
    if (f) {
        fclose(f);
        printf("Loading .capabilities.override.json\n");
        run("copy .capabilities.override.json CAPS.JSON >NUL");
    }
}

/* ------------------------------------------------------------------ */
/* Cleanup temp files                                                   */
/* ------------------------------------------------------------------ */

static void cleanup(void) {
    remove("LOGINREQ.JSON");  remove("RAW.TXT");
    remove("LOGINBODY.JSON"); remove("LOGINSTATUS.TXT");
    remove("TOKEN.TXT");      remove("CAPS.JSON");
    remove("SYSCAPS.JSON");   remove("POLLREQ.JSON");
    remove("TASKBODY.JSON");  remove("TASKSTATUS.TXT");
    remove("SCRIPTURL.TXT");  remove("TASKID.TXT");
    remove("TASK.BAT");       remove("HNAME.TXT");
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void) {
    char *env;
    int  rc;

    printf("Sheetbot DOS Agent starting\n");

    /* ---- Read required environment ---- */
    env = getenv("SHEETBOT_BASEURL");
    if (!env || env[0] == '\0') {
        printf("Error: SHEETBOT_BASEURL is not set\n");
        return 1;
    }
    strncpy(g_baseurl, env, URL_MAX - 1);
    g_baseurl[URL_MAX - 1] = '\0';
    printf("Using server: %s\n", g_baseurl);

    g_auth_apikey[0] = g_auth_user[0] = g_auth_pass[0] = '\0';
    env = getenv("SHEETBOT_AUTH_APIKEY"); if (env) { strncpy(g_auth_apikey, env, VAL_MAX-1); }
    env = getenv("SHEETBOT_AUTH_USER");   if (env) { strncpy(g_auth_user,   env, VAL_MAX-1); }
    env = getenv("SHEETBOT_AUTH_PASS");   if (env) { strncpy(g_auth_pass,   env, VAL_MAX-1); }

    /* Scrub credentials from environment immediately after reading */
    putenv("SHEETBOT_AUTH_APIKEY=");
    putenv("SHEETBOT_AUTH_USER=");
    putenv("SHEETBOT_AUTH_PASS=");

    /* ---- Authentication ---- */
    g_token[0] = g_auth_header[0] = '\0';

    if (g_auth_apikey[0] != '\0') {
        printf("Attempting authentication with API key\n");
        snprintf(g_cmd, CMD_MAX, "jset apiKey %s > LOGINREQ.JSON", g_auth_apikey);
        run(g_cmd);
        snprintf(g_curl_args, CMD_MAX,
                 "-X POST \"%s/login\" -H \"Content-Type: application/json\" -d @LOGINREQ.JSON",
                 g_baseurl);
        if (curl_split(g_curl_args, "LOGINBODY.JSON", "LOGINSTATUS.TXT") != 0) {
            printf("Error: curl failed during login\n"); cleanup(); return 1;
        }
        printf("Auth status: %s\n", g_status);
        if (check_status(g_status, "login") != 0) { cleanup(); return 1; }
        run("jget token < LOGINBODY.JSON > TOKEN.TXT");
        read_token_from_file("TOKEN.TXT", g_token, TOKEN_MAX);
        if (g_token[0] == '\0') { printf("Error: no token in auth response\n"); cleanup(); return 1; }
        printf("Token obtained\n");
        snprintf(g_auth_header, sizeof(g_auth_header), "Authorization: Bearer %s", g_token);

    } else if (g_auth_user[0] != '\0') {
        printf("Attempting authentication with user: %s\n", g_auth_user);
        snprintf(g_cmd, CMD_MAX,
                 "jset username %s password %s > LOGINREQ.JSON", g_auth_user, g_auth_pass);
        run(g_cmd);
        snprintf(g_curl_args, CMD_MAX,
                 "-X POST \"%s/login\" -H \"Content-Type: application/json\" -d @LOGINREQ.JSON",
                 g_baseurl);
        if (curl_split(g_curl_args, "LOGINBODY.JSON", "LOGINSTATUS.TXT") != 0) {
            printf("Error: curl failed during login\n"); cleanup(); return 1;
        }
        printf("Auth status: %s\n", g_status);
        if (check_status(g_status, "login") != 0) { cleanup(); return 1; }
        run("jget token < LOGINBODY.JSON > TOKEN.TXT");
        read_token_from_file("TOKEN.TXT", g_token, TOKEN_MAX);
        if (g_token[0] == '\0') { printf("Error: no token in auth response\n"); cleanup(); return 1; }
        printf("Token obtained\n");
        snprintf(g_auth_header, sizeof(g_auth_header), "Authorization: Bearer %s", g_token);

    } else {
        printf("No authentication credentials provided\n");
    }

    /* ---- Load capabilities ---- */
    load_capabilities();

    /* Get hostname */
    g_sys_hostname[0] = '\0';
    env = getenv("HOSTNAME");
    if (env) strncpy(g_sys_hostname, env, VAL_MAX - 1);
    if (g_sys_hostname[0] == '\0') {
        run("hostname > HNAME.TXT 2>NUL");
        read_token_from_file("HNAME.TXT", g_sys_hostname, VAL_MAX);
        remove("HNAME.TXT");
    }
    if (g_sys_hostname[0] == '\0') strcpy(g_sys_hostname, "freedos");

    snprintf(g_cmd, CMD_MAX,
             "jset os freedos arch x86 hostname %s local @CAPS.JSON > SYSCAPS.JSON",
             g_sys_hostname);
    run(g_cmd);
    run("jset type bat capabilities @SYSCAPS.JSON > POLLREQ.JSON");
    printf("Capabilities built\n");

    /* ---- Poll for tasks ---- */
    printf("Polling for tasks at: %s/tasks/get\n", g_baseurl);
    snprintf(g_task_url, sizeof(g_task_url), "%s/tasks/get", g_baseurl);
    curl_args_post(g_curl_args, CMD_MAX, g_task_url, "@POLLREQ.JSON");
    if (curl_split(g_curl_args, "TASKBODY.JSON", "TASKSTATUS.TXT") != 0) {
        printf("Error: curl failed during task poll\n"); cleanup(); return 1;
    }
    printf("Task poll status: %s\n", g_status);
    if (check_status(g_status, "tasks/get") != 0) { cleanup(); return 1; }

    /* ---- Check if a task is available ---- */
    rc = run("jget script < TASKBODY.JSON > SCRIPTURL.TXT");
    if (rc != 0) { printf("No task available\n"); cleanup(); return 0; }
    read_token_from_file("SCRIPTURL.TXT", g_script_url, URL_MAX);
    if (g_script_url[0] == '\0') { printf("No task available\n"); cleanup(); return 0; }

    run("jget id < TASKBODY.JSON > TASKID.TXT");
    read_token_from_file("TASKID.TXT", g_task_id, VAL_MAX);
    printf("Task received: %s\n", g_task_id);

    /* ---- Set task environment variables ---- */
    snprintf(g_env_task_id,         ENV_MAX, "SHEETBOT_TASK_ID=%s",                          g_task_id);
    snprintf(g_env_auth_hdr,        ENV_MAX, "SHEETBOT_AUTHORIZATION_HEADER=Bearer %s",       g_token);
    snprintf(g_env_baseurl,         ENV_MAX, "SHEETBOT_BASEURL=%s",                           g_baseurl);
    snprintf(g_env_task_baseurl,    ENV_MAX, "SHEETBOT_TASK_BASEURL=%s/tasks/%s",             g_baseurl, g_task_id);
    snprintf(g_env_task_accepturl,  ENV_MAX, "SHEETBOT_TASK_ACCEPTURL=%s/tasks/%s/accept",    g_baseurl, g_task_id);
    snprintf(g_env_task_completeurl,ENV_MAX, "SHEETBOT_TASK_COMPLETEURL=%s/tasks/%s/complete",g_baseurl, g_task_id);
    snprintf(g_env_task_failedurl,  ENV_MAX, "SHEETBOT_TASK_FAILEDURL=%s/tasks/%s/failed",    g_baseurl, g_task_id);
    snprintf(g_env_task_dataurl,    ENV_MAX, "SHEETBOT_TASK_DATAURL=%s/tasks/%s/data",        g_baseurl, g_task_id);
    snprintf(g_env_task_artefacturl,ENV_MAX, "SHEETBOT_TASK_ARTEFACTURL=%s/tasks/%s/artefacts",g_baseurl, g_task_id);
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
    if (g_auth_header[0] != '\0') {
        snprintf(g_cmd, CMD_MAX, "curl -sk -X POST \"%s\" -H \"Content-Type: application/json\" -H \"%s\" -d {} >NUL", g_task_url, g_auth_header);
    } else {
        snprintf(g_cmd, CMD_MAX, "curl -sk -X POST \"%s\" -H \"Content-Type: application/json\" -d {} >NUL", g_task_url);
    }
    run(g_cmd);
    printf("Task accepted\n");

    /* ---- Fetch script ---- */
    printf("Fetching script from: %s\n", g_script_url);
    if (g_auth_header[0] != '\0') {
        snprintf(g_cmd, CMD_MAX,
                 "curl -sk -H \"%s\" \"%s\" > TASK.BAT",
                 g_auth_header, g_script_url);
    } else {
        snprintf(g_cmd, CMD_MAX, "curl -sk \"%s\" > TASK.BAT", g_script_url);
    }
    run(g_cmd);

    /* ---- Execute script ---- */
    /* Script calls /complete or /failed itself via SHEETBOT_TASK_* env vars.
     * We only call /failed here if the script returns a non-zero exit code. */
    printf("Executing task script\n");
    rc = run("TASK.BAT");

    if (rc != 0) {
        printf("Task script failed (exit %d), reporting failure\n", rc);
        snprintf(g_task_url, ENV_MAX, "%s/tasks/%s/failed", g_baseurl, g_task_id);
        if (g_auth_header[0] != '\0') {
            snprintf(g_cmd, CMD_MAX, "curl -sk -X POST \"%s\" -H \"Content-Type: application/json\" -H \"%s\" -d {} >NUL", g_task_url, g_auth_header);
        } else {
            snprintf(g_cmd, CMD_MAX, "curl -sk -X POST \"%s\" -H \"Content-Type: application/json\" -d {} >NUL", g_task_url);
        }
        run(g_cmd);
        cleanup();
        return 1;
    }

    printf("Task script finished successfully\n");
    cleanup();
    return 0;
}
