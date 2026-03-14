@ECHO OFF
REM ============================================================
REM  Sheetbot FreeDOS Task Script - Noctis IV Planet Dump
REM  Reads coordinates from task data, renders planet, uploads BMPs.
REM
REM  Task data format:
REM  {
REM    "x": -18928,
REM    "y": 29680,
REM    "z": -67336,
REM    "planet": 3
REM  }
REM ============================================================

SET PATH=C:\;%PATH%

REM ---- Fetch task data ----
ECHO Fetching task data...
ECHO -s> c.cfg
ECHO -k>> c.cfg
ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> c.cfg
ECHO url = "%SHEETBOT_TASK_BASEURL%">> c.cfg
curl -K c.cfg > taskdata.json
DEL c.cfg

ECHO Task data:
TYPE taskdata.json

REM ---- Extract coordinates using setvar ----
jget data.x < taskdata.json | setvar COORD_X
CALL setvar.bat
jget data.y < taskdata.json | setvar COORD_Y
CALL setvar.bat
jget data.z < taskdata.json | setvar COORD_Z
CALL setvar.bat
jget data.planet < taskdata.json | setvar COORD_P
CALL setvar.bat

DEL taskdata.json setvar.bat

ECHO Coordinates: %COORD_X% %COORD_Y% %COORD_Z% planet %COORD_P%

REM ---- CD into Noctis directory ----
CD c:\nivplus\Noctis-IV-Plus-planetdump\modules

REM ---- Run Noctis planet dump ----
ECHO Running Noctis IV planet dump...
noctis.exe -dump %COORD_X% %COORD_Y% %COORD_Z% %COORD_P%
ECHO Noctis done.

REM ---- Upload PLANET.BMP ----
ECHO Uploading PLANET.BMP...
ECHO -s> c.cfg
ECHO -k>> c.cfg
ECHO -F file=@PLANET.BMP>> c.cfg
ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> c.cfg
ECHO url = "%SHEETBOT_TASK_ARTEFACTURL%">> c.cfg
curl -K c.cfg
DEL c.cfg

REM ---- Upload P_SURFAC.BMP ----
ECHO Uploading P_SURFAC.BMP...
ECHO -s> c.cfg
ECHO -k>> c.cfg
ECHO -F file=@P_SURFAC.BMP>> c.cfg
ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> c.cfg
ECHO url = "%SHEETBOT_TASK_ARTEFACTURL%">> c.cfg
curl -K c.cfg
DEL c.cfg

REM ---- Upload P_WCLOUD.BMP ----
ECHO Uploading P_WCLOUD.BMP...
ECHO -s> c.cfg
ECHO -k>> c.cfg
ECHO -F file=@P_WCLOUD.BMP>> c.cfg
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