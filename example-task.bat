@ECHO OFF
REM ============================================================
REM  Sheetbot FreeDOS Task Script - Date Test
REM ============================================================

REM ---- Get date, build JSON payload in one shot ----
DATE /T > today.txt
jsontxt date < today.txt > done.json

ECHO Payload:
TYPE done.json

REM ---- Write curl config ----
ECHO -s> curl.cfg
ECHO -k>> curl.cfg
ECHO -X POST>> curl.cfg
ECHO --data-binary @->> curl.cfg
ECHO -H "Content-Type: application/json">> curl.cfg
ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> curl.cfg
ECHO url = "%SHEETBOT_TASK_COMPLETEURL%">> curl.cfg

REM ---- Report completion ----
curl -K curl.cfg < done.json
ECHO.
ECHO Task complete!

REM ---- Cleanup ----
DEL done.json curl.cfg today.txt