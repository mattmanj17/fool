
import os
import subprocess
import shutil
import concurrent.futures
from threading import Lock
import tempfile

ignored_by_git_dir = "ignored_by_git"

llvm_git_url = "https://github.com/mattmanj17/llvm-project.git"
llvm_branch_name = '17.0.3-branch'

llvm_root_dir = f"{ignored_by_git_dir}/llvm-project"
llvm_build_dir = f"{llvm_root_dir}/build"
llvm_cmake_root = f'{llvm_root_dir}/llvm'
llvm_clang_vcxproj = f'{llvm_build_dir}/tools/clang/tools/driver/clang.vcxproj'
llvm_clang_exe = f"{llvm_build_dir}/Release/bin/clang.exe"

raw_test_files_in = "test_files/raw"
raw_test_files_scrubbed = f'{ignored_by_git_dir}/scrubbed_raw_test_files'
raw_test_files_out = f"{ignored_by_git_dir}/test_output/raw"

scrub_ws_path = f"{ignored_by_git_dir}/build/exe/scrub_ws.exe"

no_hashtag_test_files_in = "test_files/no_hashtag"
no_hashtag_test_files_out = f"{ignored_by_git_dir}/test_output/no_hashtag"

ctok_exe = f"{ignored_by_git_dir}/build/exe/ctok.exe"

def main():
	if not os.path.exists(ignored_by_git_dir):
		os.mkdir(ignored_by_git_dir)
	
	build_clang()
	setup_tests()
	run_tests()

def build_clang():
	# build llvm-project/build/Release/bin/clang.exe
	
	if os.path.exists(llvm_root_dir):
		return
	
	subprocess.run(['git', 'clone', llvm_git_url, llvm_root_dir])
	subprocess.run(['git', '-C', llvm_root_dir, 'switch', llvm_branch_name])
	os.mkdir(llvm_build_dir)
	
	subprocess.run([
		'cmake', 
		'-DLLVM_ENABLE_PROJECTS=clang', 
		'-A', 'x64', 
		'-Thost=x64', 
		'-S',
		llvm_cmake_root,
		'-B',
		llvm_build_dir])
	
	subprocess.run([
		'msbuild',
		'-m:4',
		llvm_clang_vcxproj, 
		'/property:Configuration=Release'])

def setup_tests():
	# generate test/input

	if not os.path.exists(raw_test_files_scrubbed):
		#os.mkdir(raw_test_files_scrubbed)
		scrub_raw_input()

	ensure_test_output()

def scrub_raw_input():
	print("scrub_raw_input")

	# make a copy of the raw input

	shutil.copytree(raw_test_files_in, raw_test_files_scrubbed)

	# clean up some patterns where we diverge from clang
	# we still support correctly lexing files that contain these patterns, 
	#  we just slightly differ in the exact ways we split up tokens compared to clang.
	#  These are all edge cases involving whitespace, so slight divergance is fine.

	dest_dir = os.path.abspath(raw_test_files_scrubbed)

	with concurrent.futures.ThreadPoolExecutor() as executor:
		for root, _, fnames in os.walk(dest_dir):
			for fname in fnames:
				src_file = os.path.join(root, fname)
				executor.submit(scrub_ws_in_file, src_file)

def scrub_ws_in_file(src_file):
	scrub_exe = os.path.abspath(scrub_ws_path)
	subprocess.run(f'{scrub_exe} "{src_file}" "{src_file}"')
	print(f'scrub_ws {src_file}')

def ensure_test_output():
	# generate test/output
	# NOTE(matthewd) clang is a little heavy weight, so we limit to 19 workers

	with concurrent.futures.ThreadPoolExecutor(max_workers=19) as executor: 
		clang = os.path.abspath(llvm_clang_exe)
		for in_path, out_path in raw_test_cases():
			if not os.path.isfile(out_path):
				directory = os.path.dirname(out_path)
				if not os.path.exists(directory):
					os.makedirs(directory)
				executor.submit(run_clang, clang, True, in_path, out_path)
		for in_path, out_path in no_hashtag_test_cases():
			if not os.path.isfile(out_path):
				directory = os.path.dirname(out_path)
				if not os.path.exists(directory):
					os.makedirs(directory)
				executor.submit(run_clang, clang, False, in_path, out_path)

def raw_test_cases():
	in_dir = os.path.abspath(raw_test_files_scrubbed)
	out_dir = os.path.abspath(raw_test_files_out)
	for root, _, files in os.walk(in_dir):
		for fname in files:
			in_path = os.path.join(root, fname)
			rel_path = os.path.relpath(in_path, in_dir)
			
			out_path = os.path.join(out_dir, rel_path)
			out_path += ".tokens"

			yield (in_path, out_path)

def no_hashtag_test_cases():
	in_dir = os.path.abspath(no_hashtag_test_files_in)
	out_dir = os.path.abspath(no_hashtag_test_files_out)
	for root, _, files in os.walk(in_dir):
		for fname in files:
			in_path = os.path.join(root, fname)
			rel_path = os.path.relpath(in_path, in_dir)
			
			out_path = os.path.join(out_dir, rel_path)
			out_path += ".tokens"

			yield (in_path, out_path)

def run_clang(clang_path, raw, in_path, out_path):
	with open(out_path, "wb") as out_f:
		cmd = '-dump-raw-tokens' if raw else '-dump-tokens'
		subprocess.run(
			f'{clang_path} -cc1 {cmd} -std=c11 -x c "{in_path}"', 
			stderr=out_f)
	print(f'run_clang {in_path}')

def run_tests():
	fails = []
	fail_lock = Lock()

	with concurrent.futures.ThreadPoolExecutor() as executor:
		ctok = os.path.abspath(ctok_exe)
		for in_path, out_path in raw_test_cases():
			executor.submit(
						run_ctok, 
						ctok, 
						in_path, 
						out_path,
						fails,
						fail_lock)
			
		""" # todo run lex tests
		for in_path, out_path in lex_test_cases():
			if in_path in skips:
				continue
			executor.submit(
						run_ctok, 
						ctok, 
						False,
						in_path, 
						out_path,
						fails,
						fail_lock)
		"""
	
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