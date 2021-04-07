@echo off
@setlocal enabledelayedexpansion

set make=
set var=0
set params=
:nextparam
set /A var=!var! + 1
for /F "tokens=%var% delims= " %%A IN ("%*") do (
    set arg=%%A
    if "!arg:~0,1!"=="-" (
        goto nextparam
    ) 
    if "!arg:~0,1!"=="/" (
        goto nextparam
    ) 
    if "%%~xA"==".dll" (
        set make=y
        set params=%params% /OUT:%%~nA.lib
        goto nextparam
    )
    if "%%~xA"==".o" (
        set params=%params% %%A
        goto nextparam
    )
    if "%%~xA"==".lib" (
        set params=%params% %%A
        goto nextparam
    )
    goto nextparam
)

if defined make (
    lib.exe%params%
    if !errorlevel! neq 0 (
        exit /B !errorlevel!
    )
)

cl.exe %*
if !errorlevel! neq 0 (
    exit /B !errorlevel!
)
