@echo off
setlocal

set obj_dir="build/obj/"
set exe_dir="build/exe/"
set pdb_dir="build/pdb/"

set common_options=^
 /nologo /permissive-^
 /D"_CRT_SECURE_NO_WARNINGS" /D"_UNICODE" /D"UNICODE"^
 /Wall /WX^
 /wd"5045" /wd"4711" /wd"4668" /wd"5039" /wd"4710" /wd"4995"^
 /wd"4061" /wd"4062" /wd"5105"^
 /Zi /Gm-^
 /Fo%obj_dir% /Fe%exe_dir% /Fd%pdb_dir%

set link_options=/link /INCREMENTAL:NO

if not exist %obj_dir% mkdir %obj_dir%
if not exist %exe_dir% mkdir %exe_dir%
if not exist %pdb_dir% mkdir %pdb_dir%

echo build as cpp
cl ctok.c %common_options% /std:c++14 /TP %link_options%

echo build as c
cl ctok.c %common_options% /std:c11 /TC %link_options%

endlocal