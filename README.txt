================================================================================
  SHEETBOT FREEDOS AGENT RUNTIME
  A Sheetbot agent runtime for FreeDOS
  github.com/jorisvddonk/sheetbot
================================================================================

INSTALLATION
------------
In FreeDOS 1.14:

curl -sk -L -o i.bat https://raw.githubusercontent.com/jorisvddonk/sheetbot-agent-dos/refs/heads/main/INSTALL.bat
CALL i.bat

OVERVIEW
--------
This package implements a Sheetbot agent runtime for FreeDOS. It polls a
Sheetbot server for tasks of type "freedos-bat", executes them as .BAT
files, and reports results back to the server.

Tested on: FreeDOS 1.4 running in QEMU
Requires:  Open Watcom 2.0 (to compile), curl.exe (to run)


FILES
-----
  agent.c          Main agent runtime
  jget.c           JSON field extractor
  jset.c           JSON object builder
  jsontxt.c        Wraps stdin text as a JSON payload
  stripnl.c        Strips newlines from stdin
  b64enc.c         Base64 encodes stdin to stdout
  compile.bat      Compiles everything with Open Watcom
  example-task.bat Example task script that reports the current date
  splitst.c        Deprecated - not used anymore


COMPILING
---------
1. Copy all .C files and BUILD.BAT to C:\

2. Run:
     BUILD.BAT

   Assumes Open Watcom 2.0 is installed in C:\WATCOM.
   Produces: JGET.EXE  JSET.EXE  JSONTXT.EXE  STRIPNL.EXE
             B64ENC.EXE  AGENT.EXE


RUNNING
-------
1. Set environment variables:

     SET SHEETBOT_BASEURL=https://yourserver.example.com

   Then set ONE of the following for authentication:

     SET SHEETBOT_AUTH_APIKEY=your_api_key

       -- or --

     SET SHEETBOT_AUTH_USER=username
     SET SHEETBOT_AUTH_PASS=password

   To make these permanent, add the SET lines to C:\AUTOEXEC.BAT.

2. Run the agent:

     AGENT.EXE

   The agent will:
     - Authenticate with the server
     - Poll for a task of type "freedos-bat"
     - Accept, fetch, and execute the task script as TASK.BAT
     - Report failure if the script exits with ERRORLEVEL 1
     - Clean up all temp files and exit

3. The agent exits after one poll cycle. To poll continuously, call
   it from a loop in AUTOEXEC.BAT or a wrapper script:

     :LOOP
     AGENT.EXE
     GOTO LOOP


WRITING TASK SCRIPTS
--------------------
Task scripts are plain .BAT files served by the Sheetbot server.
Register tasks with type "freedos-bat" to target this runtime.

The agent sets these environment variables before running the script:

  SHEETBOT_BASEURL               Base URL of the server
  SHEETBOT_TASK_ID               Task UUID
  SHEETBOT_AUTHORIZATION_HEADER  Auth token value (Bearer eyJ...)
  SHEETBOT_TASK_BASEURL          https://server/tasks/<id>
  SHEETBOT_TASK_ACCEPTURL        https://server/tasks/<id>/accept
  SHEETBOT_TASK_COMPLETEURL      https://server/tasks/<id>/complete
  SHEETBOT_TASK_FAILEDURL        https://server/tasks/<id>/failed
  SHEETBOT_TASK_DATAURL          https://server/tasks/<id>/data
  SHEETBOT_TASK_ARTEFACTURL      https://server/tasks/<id>/artefacts

The script must call /complete or /failed itself. If it exits with
ERRORLEVEL 1, the agent will call /failed automatically.

MINIMAL TASK SCRIPT TEMPLATE:
  -----------------------------------------------------------------------
  @ECHO OFF
  jset data {} > done.json

  ECHO -s> curl.cfg
  ECHO -k>> curl.cfg
  ECHO -X POST>> curl.cfg
  ECHO --data-binary @->> curl.cfg
  ECHO -H "Content-Type: application/json">> curl.cfg
  ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> curl.cfg
  ECHO url = "%SHEETBOT_TASK_COMPLETEURL%">> curl.cfg

  curl -K curl.cfg < done.json
  DEL done.json curl.cfg
  -----------------------------------------------------------------------

UPLOADING ARTEFACTS FROM A TASK SCRIPT:
  -----------------------------------------------------------------------
  ECHO -s> c.cfg
  ECHO -k>> c.cfg
  ECHO -F file=@myfile.txt>> c.cfg
  ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> c.cfg
  ECHO url = "%SHEETBOT_TASK_ARTEFACTURL%">> c.cfg
  curl -K c.cfg
  DEL c.cfg
  -----------------------------------------------------------------------


IMPORTANT NOTES FOR TASK SCRIPTS
---------------------------------
  1. Use "ECHO text> file" not "ECHO text > file" - the space before >
     adds a trailing space which breaks curl.cfg parsing.

  2. The auth header env var is "Bearer <token>" - prepend "Authorization:"
       -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%"

  3. Use jsontxt.exe to safely wrap text output as a JSON payload:
       DATE /T > today.txt
       jsontxt date < today.txt > done.json
     Handles newline stripping and JSON escaping automatically.

  4. All curl commands must use -K curl.cfg to avoid the DOS 128-char
     command line limit. Never put long commands directly on one line.

  5. All temp filenames must be 8.3 format (max 8 chars dot max 3 chars).

  6. Do not use jset with @file references for intermediate files -
     they produce truncated JSON if the file is empty or missing.
     Build JSON payloads in a single jset call where possible.

  7. FOR /F with "tokens=" is not supported in FreeDOS COMMAND.COM.
     Use jsontxt.exe or stripnl.exe to process command output instead.


UTILITIES REFERENCE
-------------------
JGET keyname < input.json
  Extracts a string field from JSON on stdin.
  Exit code 0 if found, 1 if not found.
  Supports one level of dot notation: JGET os.name
  Example:
    ECHO {"id":"abc","type":"bat"} | JGET type   ->  bat

JSET key1 val1 [key2 val2 ...] > output.json
  Builds a flat JSON object from key/value pairs.
  Special unquoted values: {}  []  true  false  null
  Bare numbers: prefix with =  (e.g. JSET count =42)
  Example:
    JSET type freedos-bat os freedos  ->  {"type":"freedos-bat","os":"freedos"}

JSONTXT keyname < input.txt > output.json
  Wraps stdin as {"data":{"keyname":"..."}}.
  Trims trailing whitespace/newlines. Escapes special chars.
  Example:
    DATE /T | JSONTXT date  ->  {"data":{"date":"Fri 03-13-2026"}}

STRIPNL < input.txt > output.txt
  Strips all newlines and carriage returns from stdin.
  Trims leading and trailing whitespace.

B64ENC < input.exe > output.b64
  Base64 encodes stdin. Useful for binary file transfer over
  connections that mangle raw binary data.
  Decode on Mac/Linux: base64 -d output.b64 > input.exe


GETTING FILES OUT OF FREEDOS
-----------------------------
If you need to transfer compiled .EXE files out of FreeDOS and your
ISP does deep packet inspection that mangles binary uploads:

  1. Base64 encode the file:
       b64enc < agent.exe > ag.b64

  2. Upload via stdin pipe (avoids @filename DPI triggering):
       type ag.b64 | curl -sk -X POST http://host:port/ag.b64 --data-binary @-

  3. Decode on Mac/Linux:
       base64 -d ag.b64 > agent.exe

  Or better - use Sheetbot's own artefacts API to upload files directly
  to the server, then download them from the Sheetbot UI. See the
  artefact upload template above or `upload_exe_files.freedos-bat.bat`


KNOWN LIMITATIONS
-----------------
  - Polls once per invocation (no built-in loop)
  - No HTTPS certificate verification (-k flag used throughout)
  - Capabilities reporting is minimal (type, os, arch, hostname)
  - jset does not support nested objects as output values
  - FOR /F tokens= not supported in FreeDOS - use jsontxt/stripnl
  - All C code uses static global buffers (16-bit DOS stack is ~4KB)
  - Command lines and batch file lines limited to ~125 characters
  - Always use curl -K config.cfg for any non-trivial curl invocation


================================================================================