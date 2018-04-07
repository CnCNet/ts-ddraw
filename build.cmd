@echo off
REM
REM cnc-patch environment config
REM
set PATH=C:\win-builds-patch-32\bin
gmake clean
gmake release
pause
