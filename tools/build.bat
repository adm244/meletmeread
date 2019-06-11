@ECHO OFF
REM This file is a part of cdev project
REM https://github.com/adm244/cdev

SETLOCAL
REM [customize those variables]
SET libs=kernel32.lib user32.lib Psapi.lib
SET libshook=kernel32.lib shell32.lib
SET files=..\%source%\main.cpp
SET fileshook=%source%\loader\main.cpp
SET deffile=%source%\loader\exports.def
SET commoninclude=%source%\common
SET hookname=dinput8
SET projname=letmeread
SET modsdir=NativeMods

SET debug=/Od /Zi /nologo /DDEBUG /LDd
SET release=/O2 /WX /nologo /LD
SET args=/I..\%commoninclude% /Fe%projname% %files% %release% /link %libs%
SET argshook=/I%commoninclude% /Fe%hookname% %fileshook% %release% /link /DEF:%deffile% %libshook%

SET compiler=CL
REM ###########################

SET edit=edit
SET setprjname=setname

IF [%1]==[%setprjname%] GOTO SetProjectName
IF [%1]==[%edit%] GOTO EditBuildFile
IF [%1]==[] GOTO Build
GOTO Error

:Build
ECHO: Build started...

IF NOT EXIST "%bin%" MKDIR "%bin%"
IF NOT EXIST "%bin%\%modsdir%" MKDIR "%bin%\%modsdir%"

ECHO: Compiling hook...
PUSHD "%bin%"
"%compiler%" %argshook%
POPD

ECHO: Compiling mod...
PUSHD "%bin%\%modsdir%"
"%compiler%" %args%
POPD

ECHO: Build finished.
GOTO:EOF

:SetProjectName
IF [%2]==[] ECHO: ERROR: Name for a project was NOT specified! && GOTO:EOF

ECHO: Changing project name to %2...
ENDLOCAL
SET project=%2
ECHO: Done!
GOTO:EOF

:EditBuildFile
"%editor%" "%tools%\%~n0.bat"
GOTO:EOF

:Error
ECHO: ERROR: wrong arguments passed!
GOTO:EOF
