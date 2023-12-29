
import os
import subprocess
import shutil
import itertools
from pathlib import Path
import concurrent.futures
from threading import Lock
import tempfile

def main():
	build_clang()
	setup_tests()
	run_tests()

def build_clang():
	# build llvm-project/build/Release/bin/clang.exe
	# BB (matthewd) should re write this without os.chdir, they are confusing

	cwd = os.getcwd()

	if not os.path.exists('untracked'):
		os.mkdir('untracked')

	if not os.path.exists('untracked/3rd_party'):
		os.mkdir('untracked/3rd_party')
	
	os.chdir('untracked/3rd_party')
	
	if os.path.exists('llvm-project'):
		os.chdir(cwd)
		return
	
	print('downloading and building clang')
	
	print('git clone')
	subprocess.run([
		'git', 'clone', 
		'https://github.com/mattmanj17/llvm-project.git'])
	
	os.chdir('llvm-project')
	
	print('git switch')
	subprocess.run(['git', 'switch', '17.0.3-branch'])
	
	os.mkdir('build')
	os.chdir('build')
	
	print('cmake')
	subprocess.run([
		'cmake', 
		'-DLLVM_ENABLE_PROJECTS=clang', 
		'-A', 'x64', 
		'-Thost=x64', 
		'..\\llvm'])
	
	os.chdir('..')
	
	print('msbuild')
	subprocess.run([
		'msbuild', 
		'-m:4', 
		'build/tools/clang/tools/driver/clang.vcxproj', 
		'/property:Configuration=Release'])

	os.chdir(cwd)

def setup_tests():
	# generate test/input

	if not os.path.exists('untracked/test'):
		os.mkdir('untracked/test')

	if not os.path.exists('untracked/test/input'):
		setup_test_input()

	ensure_test_output()

def setup_test_input():
	os.mkdir('untracked/test/input')
	copy_3rd_party_c_files()
	copy_test_files()
	generate_test_input()
	scrub_test_input()

def copy_3rd_party_c_files():
	skip_dirs = [
		"build",
		".git",
	]

	skip_files = [

		# these files have Named UCNs in them (\N{some-code-point}), which we do not support

		os.path.abspath("untracked/3rd_party/llvm-project/clang/test/FixIt/fixit-unicode-named-escape-sequences.c"),
		os.path.abspath("untracked/3rd_party/llvm-project/clang/test/Lexer/unicode.c"),
		os.path.abspath("untracked/3rd_party/llvm-project/clang/test/Preprocessor/ucn-pp-identifier.c"),
		os.path.abspath("untracked/3rd_party/llvm-project/clang/test/Sema/ucn-identifiers.c"),

		# these files have garbage unicode extending ids/nums, which we handle differently from clang

		os.path.abspath("untracked/3rd_party/llvm-project/clang/test/FixIt/fixit-unicode.c"),
		os.path.abspath("untracked/3rd_party/llvm-project/clang/test/Preprocessor/ucn-allowed-chars.c"),
		os.path.abspath("untracked/3rd_party/llvm-project/clang/test/Preprocessor/utf8-allowed-chars.c"),
	]

	src_dir = os.path.abspath('untracked/3rd_party/')
	dest_dir = os.path.abspath('untracked/test/input/')

	for root, dirs, fnames in os.walk(src_dir, topdown=True):
		print(f"copy c files in {root}")
		dirs[:] = [dir for dir in dirs if dir not in skip_dirs] 
		for fname in fnames:
			if fname.endswith('.c'):
				src_file = os.path.join(root, fname)
				if src_file in skip_files:
					continue
				rel_path = os.path.relpath(src_file, src_dir)
				destination_file = os.path.join(dest_dir, rel_path)
				os.makedirs(os.path.dirname(destination_file), exist_ok=True)
				shutil.copy2(src_file, destination_file)

def copy_test_files():
	shutil.copytree("test","untracked/test/input/test")

def generate_test_input():
	# 'chars' is a carefully chosen subset of characters, 
	#  designed to exersise interesting code paths in the lexer,
	#  especially hitting an EOF in different states.

	# bug! this will not hit the full UCN code paths...
	#  need input at least 6 chars long for that

	chars = b"\"\\\n\x20'8uULE0_.-*/"
	out_dir = "untracked/test/input/spew"

	if not os.path.exists(out_dir):
		os.makedirs(out_dir, exist_ok=True)

	findex = 0
	for result in itertools.product(chars, repeat=4):
		file_name = f"{out_dir}/spew_{findex}.txt"
		
		if (findex % 1000) == 0:
			print(f'gen spew {file_name}')
		
		with open(file_name, "wb") as binary_file:
			binary_file.write(bytes(result))
		findex += 1

def scrub_test_input():
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
	subprocess.run(f'{scrub_exe} {src_file} {src_file}')
	print(f'scrub_ws {src_file}')

def ensure_test_output():
	# copy directory structure of test/input to test/output

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
			f'{clang_path} -cc1 -dump-raw-tokens -std=c11 -x c {in_path}', 
			stderr=out_f)
	print(f'run_clang {in_path}')

def run_tests():
	fails = []
	fail_lock = Lock()

	with concurrent.futures.ThreadPoolExecutor() as executor:
		ctok = os.path.abspath("untracked/build/exe/ctok.exe")
		for in_path, out_path in test_cases():
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

		#bcomapre_cmd = f'bcompare /fv="Hex Compare" {clang_out_path} {ctok_out_path}'
		bcomapre_cmd = f'bcompare {clang_out_path} {ctok_out_path}'
		print(bcomapre_cmd)
		subprocess.run(bcomapre_cmd)

def run_ctok(ctok_path, in_path, out_path, fails, fail_lock):
	result = subprocess.run(
		f'{ctok_path} {in_path}', 
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