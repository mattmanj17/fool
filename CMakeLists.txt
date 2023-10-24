
cmake_minimum_required(VERSION 3.0)

project(ctok VERSION 0.1)

set(SOURCES 
    main.c 
    lcp.c 
    lex.c 
    print_tokens.c 
    unicode.c
)

set(COMMON_COMPILE_OPTIONS
    /std:c11
    /std:c++14

    /permissive- 

    /D"_CRT_SECURE_NO_WARNINGS"
    /D"_UNICODE"
    /D"UNICODE"

    /Wall
    /WX
    /wd"5045" 
    /wd"4711" 
    /wd"4668" 
    /wd"5039" 
    /wd"4710" 
    /wd"4995" 
    /wd"4061" 
    /wd"4062" 
    /wd"5105"
)

add_executable(ctok ${SOURCES})
target_compile_options(ctok PRIVATE ${COMMON_COMPILE_OPTIONS} /TC)

# cpp build, for better errors/warnings

add_executable(ctok_cpp ${SOURCES})
target_compile_options(ctok PRIVATE ${COMMON_COMPILE_OPTIONS} /TP)