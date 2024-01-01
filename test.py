
import os
import subprocess
import shutil
import itertools
from pathlib import Path
import concurrent.futures
from threading import Lock
import tempfile

def main():
	if not os.path.exists('untracked'):
		os.mkdir('untracked')
	
	build_clang()
	setup_tests()
	run_tests()

def build_clang():
	# build llvm-project/build/Release/bin/clang.exe

	if not os.path.exists('untracked/3rd_party'):
		os.mkdir('untracked/3rd_party')
	
	if os.path.exists('untracked/3rd_party/llvm-project'):
		return
	
	subprocess.run([
		'git', 'clone',
		'https://github.com/mattmanj17/llvm-project.git',
		'untracked/3rd_party/llvm-project'])
	
	subprocess.run([
		'git', 
		'-C', 'untracked/3rd_party/llvm-project', 
		'switch', 
		'17.0.3-branch'])
	
	os.mkdir('untracked/3rd_party/llvm-project/build')
	
	subprocess.run([
		'cmake', 
		'-DLLVM_ENABLE_PROJECTS=clang', 
		'-A', 'x64', 
		'-Thost=x64', 
		'-S'
		'untracked/3rd_party/llvm-project/llvm',
		'-B',
		'untracked/3rd_party/llvm-project/build'])
	
	subprocess.run([
		'msbuild',
		'-m:4',
		'untracked/3rd_party/llvm-project/build/tools/clang/tools/driver/clang.vcxproj', 
		'/property:Configuration=Release'])

def setup_tests():
	# generate test/input

	if not os.path.exists('untracked/test'):
		os.mkdir('untracked/test')

	if not os.path.exists('untracked/fool_corpus'):
		subprocess.run([
			'git', 'clone', 
			'https://github.com/mattmanj17/fool_corpus.git',
			'untracked/fool_corpus'])

	if not os.path.exists('untracked/test/input'):
		setup_test_input()

	ensure_test_output()

def setup_test_input():
	os.mkdir('untracked/test/input')
	copy_raw_corpus()
	scrub_test_input()

def copy_raw_corpus():
	print("copy_corpus")
	shutil.copytree("untracked/fool_corpus/ctok_raw/","untracked/test/input/fool_corpus/ctok_raw")

def scrub_test_input():
	print("scrub")

	# clean up some patterns where we diverge from clang
	# we still support correctly lexing files that contain these patterns, 
	#  we just slightly differ in the exact ways we split up tokens compared to clang.
	#  These are all edge cases involving whitespace, so slight divergance is fine.

	dest_dir = os.path.abspath('untracked/test/input/')

	with concurrent.futures.ThreadPoolExecutor() as executor:
		for root, _, fnames in os.walk(dest_dir):
			for fname in fnames:
				src_file = os.path.join(root, fname)
				executor.submit(scrub_ws_in_file, src_file)

def scrub_ws_in_file(src_file):
	scrub_exe = os.path.abspath("untracked/build/exe/scrub_ws.exe")
	subprocess.run(f'{scrub_exe} "{src_file}" "{src_file}"')
	print(f'scrub_ws {src_file}')

def ensure_test_output():
	# copy directory structure of test/input to test/output

	if os.path.isdir('untracked/test/output'):
		return

	print('mkdir ...')
	in_dir = os.path.abspath("untracked/test/input")
	out_dir = os.path.abspath("untracked/test/output")
	for root, _, _ in os.walk(in_dir):
		rel_path = os.path.relpath(root, in_dir)
		out_path = os.path.join(out_dir, rel_path)
		Path(out_path).mkdir(parents=True, exist_ok=True)
	
	# generate test/output
	# NOTE(matthewd) clang is a little heavy weight, so we limit to 19 workers

	with concurrent.futures.ThreadPoolExecutor(max_workers=19) as executor: 
		clang = os.path.abspath("untracked/3rd_party/llvm-project/build/Release/bin/clang.exe")
		for in_path, out_path in test_cases():
			if not os.path.isfile(out_path):
				executor.submit(run_clang, clang, in_path, out_path)

def test_cases():
	in_dir = os.path.abspath("untracked/test/input")
	out_dir = os.path.abspath("untracked/test/output")
	for root, _, files in os.walk(in_dir):
		for fname in files:
			in_path = os.path.join(root, fname)
			rel_path = os.path.relpath(in_path, in_dir)
			
			out_path = os.path.join(out_dir, rel_path)
			out_path += ".tokens"

			yield (in_path, out_path)

def run_clang(clang_path, in_path, out_path):
	with open(out_path, "wb") as out_f:
		subprocess.run(
			f'{clang_path} -cc1 -dump-raw-tokens -std=c11 -x c "{in_path}"', 
			stderr=out_f)
	print(f'run_clang {in_path}')

def run_tests():
	fails = []
	fail_lock = Lock()

	skips = [

		# misc files we don't even care about

		os.path.abspath('untracked/test/input/fool_corpus/ctok_raw/cpython-3.12.0/Lib/test/xmltestdata/c14n-20/README'),
		os.path.abspath('untracked/test/input/fool_corpus/ctok_raw/cpython-3.12.0/Misc/NEWS.d/next/C API/README.rst'),
		os.path.abspath('untracked/test/input/fool_corpus/ctok_raw/cpython-3.12.0/Misc/NEWS.d/next/Core and Builtins/README.rst'),
		os.path.abspath('untracked/test/input/fool_corpus/ctok_raw/obs-studio-30.0.0/plugins/obs-qsv11/QSV11-License-Clarification-Email.txt'),
		os.path.abspath('untracked/test/input/fool_corpus/ctok_raw/raylib-5.0/examples/README.md'),

		# utf8 BOM. we generate the same tokens, but have different col numbers on the first line (on purpose)

		os.path.abspath('untracked/test/input/fool_corpus/ctok_raw/raylib-5.0/projects/VS2019-Android/raylib_android/raylib_android.NativeActivity/android_native_app_glue.h'),
		os.path.abspath('untracked/test/input/fool_corpus/ctok_raw/raylib-5.0/projects/VS2019-Android/raylib_android/raylib_android.NativeActivity/android_native_app_glue.c'),
		os.path.abspath('untracked/test/input/fool_corpus/ctok_raw/raylib-5.0/projects/VS2019-Android/raylib_android/raylib_android.NativeActivity/main.c'),
	]

	with concurrent.futures.ThreadPoolExecutor() as executor:
		ctok = os.path.abspath("untracked/build/exe/ctok.exe")
		for in_path, out_path in test_cases():
			if in_path in skips:
				continue
			executor.submit(
						run_ctok, 
						ctok, 
						in_path, 
						out_path,
						fails,
						fail_lock)
	
	print(f'{len(fails)} tests failed')
	if fails:
		clang_out_path, ctok_out = fails[0]

		temp_file = tempfile.NamedTemporaryFile(mode='w+b', delete=False)
		temp_file.write(ctok_out)
		ctok_out_path = temp_file.name
		temp_file.close()

		#bcomapre_cmd = f'bcompare /fv="Hex Compare" "{clang_out_path}" "{ctok_out_path}"'
		bcomapre_cmd = f'bcompare "{clang_out_path}" "{ctok_out_path}"'
		print(bcomapre_cmd)
		subprocess.run(bcomapre_cmd)

def run_ctok(ctok_path, in_path, out_path, fails, fail_lock):
	result = subprocess.run(
		f'{ctok_path} "{in_path}"', 
		capture_output=True)
	ctok_out = result.stdout

	clang_out = b""
	with open(out_path, 'rb') as clang_o:
		clang_out = clang_o.read()

	if clang_out != ctok_out:
		with fail_lock:
			fails.append((out_path, ctok_out))
	print(f'run_ctok {in_path}')

main()