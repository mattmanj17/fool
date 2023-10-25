
import subprocess
import sys
from pathlib import Path

sources = [
    'ctok.c', 
    'lcp.c', 
    'lex.c', 
    'print_tokens.c', 
    'unicode.c',
]

obj_dir = '../ctok_build/obj/'
exe_dir = '../ctok_build/exe/'

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

    '/Fo' + obj_dir,
    '/Fe' + exe_dir,
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
    res = subprocess.run(cmd)
    if res.returncode != 0:
        sys.exit(res.returncode)

Path(obj_dir).mkdir(parents=True, exist_ok=True)
Path(exe_dir).mkdir(parents=True, exist_ok=True)

# build as cpp

echo_sys('cl', sources, common_options, '/std:c++14', '/TP')

# build as c

echo_sys('cl', sources, common_options, '/std:c11', '/TC')
