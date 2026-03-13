@ECHO OFF
SET PATH=C:\;%PATH%
REM ============================================================
REM  Sheetbot FreeDOS Task Script - Noctis IV Planet Dump
REM  Runs Noctis IV Plus to render a planet and uploads the BMP
REM  as a task artefact.
REM ============================================================

REM ---- CD into Noctis directory ----
CD c:\nivplus\Noctis-IV-Plus-planetdump\modules

REM ---- Run Noctis planet dump ----
ECHO Running Noctis IV planet dump...
noctis.exe -dump -18928 29680 -67336 3
ECHO Noctis done.

REM ---- Upload PLANET.BMP as artefact ----
ECHO Uploading PLANET.BMP...
ECHO -s> c.cfg
ECHO -k>> c.cfg
ECHO -F file=@PLANET.BMP>> c.cfg
ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> c.cfg
ECHO url = "%SHEETBOT_TASK_ARTEFACTURL%">> c.cfg
curl -K c.cfg
DEL c.cfg

REM ---- Report completion ----
jset data {} > done.json
ECHO -s> curl.cfg
ECHO -k>> curl.cfg
ECHO -X POST>> curl.cfg
ECHO --data-binary @->> curl.cfg
ECHO -H "Content-Type: application/json">> curl.cfg
ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> curl.cfg
ECHO url = "%SHEETBOT_TASK_COMPLETEURL%">> curl.cfg
curl -K curl.cfg < done.json
ECHO.
ECHO Done!

REM ---- Cleanup ----
DEL done.json curl.cfg