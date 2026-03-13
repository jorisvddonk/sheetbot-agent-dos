@ECHO OFF
REM Upload all EXEs as artefacts

ECHO -s> c.cfg
ECHO -k>> c.cfg
ECHO -F file=@agent.exe>> c.cfg
ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> c.cfg
ECHO url = "%SHEETBOT_TASK_ARTEFACTURL%">> c.cfg
curl -K c.cfg

ECHO -s> c.cfg
ECHO -k>> c.cfg
ECHO -F file=@jget.exe>> c.cfg
ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> c.cfg
ECHO url = "%SHEETBOT_TASK_ARTEFACTURL%">> c.cfg
curl -K c.cfg

ECHO -s> c.cfg
ECHO -k>> c.cfg
ECHO -F file=@jset.exe>> c.cfg
ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> c.cfg
ECHO url = "%SHEETBOT_TASK_ARTEFACTURL%">> c.cfg
curl -K c.cfg

ECHO -s> c.cfg
ECHO -k>> c.cfg
ECHO -F file=@jsontxt.exe>> c.cfg
ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> c.cfg
ECHO url = "%SHEETBOT_TASK_ARTEFACTURL%">> c.cfg
curl -K c.cfg

ECHO -s> c.cfg
ECHO -k>> c.cfg
ECHO -F file=@stripnl.exe>> c.cfg
ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> c.cfg
ECHO url = "%SHEETBOT_TASK_ARTEFACTURL%">> c.cfg
curl -K c.cfg

ECHO -s> c.cfg
ECHO -k>> c.cfg
ECHO -F file=@b64enc.exe>> c.cfg
ECHO -H "Authorization: %SHEETBOT_AUTHORIZATION_HEADER%">> c.cfg
ECHO url = "%SHEETBOT_TASK_ARTEFACTURL%">> c.cfg
curl -K c.cfg

DEL c.cfg

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