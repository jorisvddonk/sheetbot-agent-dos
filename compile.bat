@ECHO OFF
REM ============================================================
REM  BUILD.BAT - Compile Sheetbot DOS Agent with Open Watcom 2
REM  Assumes all .c files are in C:\
REM  Assumes Open Watcom is installed in C:\WATCOM
REM ============================================================

REM ---- Set up Open Watcom environment ----
SET WATCOM=C:\WATCOM
SET PATH=%WATCOM%\BINW;%PATH%
SET INCLUDE=%WATCOM%\H
SET LIB=%WATCOM%\LIB286\DOS;%WATCOM%\LIB286

ECHO Open Watcom environment set
ECHO WATCOM=%WATCOM%

CD C:\

REM ---- Compile each file ----
ECHO.
ECHO Compiling JGET.C...
WCL -L=DOS jget.c -FE=jget.exe
IF ERRORLEVEL 1 GOTO FAIL_JGET

ECHO.
ECHO Compiling JSET.C...
WCL -L=DOS jset.c -FE=jset.exe
IF ERRORLEVEL 1 GOTO FAIL_JSET

ECHO.
ECHO Compiling STRIPNL.C...
WCL -L=DOS stripnl.c -FE=stripnl.exe
IF ERRORLEVEL 1 GOTO FAIL_STRIPNL

ECHO.
ECHO Compiling JSONTXT.C...
WCL -L=DOS jsontxt.c -FE=jsontxt.exe
IF ERRORLEVEL 1 GOTO FAIL_JSONTXT

ECHO.
ECHO Compiling B64ENC.C...
WCL -L=DOS b64enc.c -FE=b64enc.exe
IF ERRORLEVEL 1 GOTO FAIL_B64ENC

ECHO.
ECHO Compiling SETVAR.C...
WCL -L=DOS setvar.c -FE=setvar.exe
IF ERRORLEVEL 1 GOTO FAIL_SETVAR

ECHO.
ECHO Compiling AGENT.C...
WCL -L=DOS agent.c -FE=agent.exe
IF ERRORLEVEL 1 GOTO FAIL_AGENT

ECHO.
ECHO ============================================================
ECHO  All files compiled successfully!
ECHO  Binaries: JGET.EXE JSET.EXE STRIPNL.EXE JSONTXT.EXE AGENT.EXE SETVAR.EXE B64ENC.EXE
ECHO ============================================================
GOTO END

:FAIL_JGET
ECHO ERROR: Failed to compile JGET.C
GOTO END

:FAIL_JSET
ECHO ERROR: Failed to compile JSET.C
GOTO END

:FAIL_STRIPNL
ECHO ERROR: Failed to compile STRIPNL.C
GOTO END

:FAIL_SETVAR
ECHO ERROR: Failed to compile SETVAR.C
GOTO END

:FAIL_JSONTXT
ECHO ERROR: Failed to compile JSONTXT.C
GOTO END

:FAIL_AGENT
ECHO ERROR: Failed to compile AGENT.C
GOTO END

:FAIL_B64ENC
ECHO ERROR: Failed to compile B64ENC.C
GOTO END

:END