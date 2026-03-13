@ECHO OFF
REM ============================================================
REM  INSTALL.BAT - Download and set up Sheetbot DOS Agent
REM  Requires: curl.exe, unzip.exe
REM ============================================================

CD C:\

REM ---- Download zip from GitHub ----
ECHO Downloading sheetbot-agent-dos...
curl -sk -L -A "curl/7.0" -o agent.zip https://github.com/jorisvddonk/sheetbot-agent-dos/archive/refs/heads/main.zip
ECHO Download done.

REM ---- Extract, junking paths so everything lands in C:\ ----
ECHO Extracting...
unzip -o -j agent.zip -d C:\
DEL agent.zip

ECHO.
ECHO ============================================================
ECHO  Installation complete! Set your environment variables:
ECHO.
ECHO    SET SHEETBOT_BASEURL=https://yourserver.example.com
ECHO    SET SHEETBOT_AUTH_APIKEY=your_api_key
ECHO         -- or --
ECHO    SET SHEETBOT_AUTH_USER=username
ECHO    SET SHEETBOT_AUTH_PASS=password
ECHO.
ECHO  Then run: AGENT.EXE
ECHO ============================================================