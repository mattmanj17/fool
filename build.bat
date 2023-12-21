@echo off
setlocal

set obj_dir="untracked/build/obj/"
set exe_dir="untracked/build/exe/"
set pdb_dir="untracked/build/pdb/"

set common_options=^
 /nologo /permissive-^
 /D"_CRT_SECURE_NO_WARNINGS" /D"_UNICODE" /D"UNICODE"^
 /Wall /WX^
 /wd"5045" /wd"4711" /wd"4668" /wd"5039" /wd"4710" /wd"4995"^
 /wd"4061" /wd"4062" /wd"5105" /wd"4820"^
 /Zi /Gm-^
 /Fo%obj_dir% /Fe%exe_dir% /Fd%pdb_dir%

set link_options=/link /INCREMENTAL:NO

if not exist %obj_dir% mkdir %obj_dir%
if not exist %exe_dir% mkdir %exe_dir%
if not exist %pdb_dir% mkdir %pdb_dir%

call :build ctok.c
call :build scrub_ws.c

exit /B 0

:build_cpp
echo cpp
cl %~1 %common_options% /std:c++14 /TP %link_options%
exit /B 0

:build_c
echo c
cl %~1 %common_options% /std:c11 /TC %link_options%
exit /B 0

:build
call :build_cpp %~1
::call :build_c %~1
exit /B 0

endlocal