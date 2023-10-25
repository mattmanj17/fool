
import os
import sys

sources = [
    'ctok.c', 
    'lcp.c', 
    'lex.c', 
    'print_tokens.c', 
    'unicode.c',
]

common_options = [
    '/nologo',

    '/permissive-',

    '/D"_CRT_SECURE_NO_WARNINGS"',
    '/D"_UNICODE"',
    '/D"UNICODE"',

    '/Wall',
    '/WX',
    '/wd"5045"',
    '/wd"4711"',
    '/wd"4668"',
    '/wd"5039"',
    '/wd"4710"',
    '/wd"4995"',
    '/wd"4061"',
    '/wd"4062"',
    '/wd"5105"',
]

sources_and_options = ' '.join(sources) + ' ' + ' '.join(common_options)

def yeild_strings(arg):
    if isinstance(arg, list):
        for thing in arg:
            yield from yeild_strings(thing)
    else:
        yield str(arg)

def space_join(args):
    strings = []
    for arg in args:
        for thing in yeild_strings(arg):
            strings.append(thing)
    return ' '.join(strings)

def echo_sys(*argv):
    cmd = space_join(argv)
    print('')
    print(cmd)
    print('')
    ret = os.system(cmd)
    if ret != 0:
        sys.exit(ret)

# build as cpp

os.system('cls')
echo_sys('cl', sources, common_options, '/std:c++14', '/TP')

# build as c

os.system('cls')
echo_sys('cl', sources, common_options, '/std:c11', '/TC')
