
import os
import subprocess
import concurrent.futures
from pathlib import Path
from threading import Lock
import tempfile

def cases():
	in_dir = os.path.abspath("test/ctok/input")
	out_dir = os.path.abspath("test/ctok/output")
	for root, _, files in os.walk(in_dir):
		for fname in files:
			in_path = os.path.join(root, fname)
			rel_path = os.path.relpath(in_path, in_dir)
			
			out_path = os.path.join(out_dir, rel_path)
			out_path += ".tokens"

			yield (in_path, out_path)

# ensure clang and ctok output

print('mkdir ...') # BB should flatten out the input so this does not take as long

for in_path, out_path in cases():
	Path(os.path.dirname(out_path)).mkdir(parents=True, exist_ok=True)

def run_clang(clang_path, in_path, out_path):
	with open(out_path, "wb") as out_f:
		subprocess.run(
			f'{clang_path} -cc1 -dump-raw-tokens -std=c11 -x c {in_path}', 
			stderr=out_f)
	print(f'run_clang {in_path}')

with concurrent.futures.ThreadPoolExecutor(max_workers=19) as executor: # clang is a little heavy weight, so we limit to 19 workers
	clang = os.path.abspath("3rd_party/llvm-project/build/Release/bin/clang.exe")
	for in_path, out_path in cases():
		if not os.path.isfile(out_path):
			executor.submit(run_clang, clang, in_path, out_path)

fails = []
fail_lock = Lock()
def run_ctok(ctok_path, in_path, out_path):
	result = subprocess.run(
		f'{ctok_path} -raw {in_path}', 
		capture_output=True)
	ctok_out = result.stdout

	clang_out = b""
	with open(out_path, 'rb') as clang_o:
		clang_out = clang_o.read()

	if clang_out != ctok_out:
		with fail_lock:
			fails.append((out_path, ctok_out))
	print(f'run_ctok {in_path}')

with concurrent.futures.ThreadPoolExecutor() as executor:
	ctok = os.path.abspath("build/exe/ctok.exe")
	for in_path, out_path in cases():
		executor.submit(run_ctok, ctok, in_path, out_path)

# Print failures

print(f'{len(fails)} tests failed')

if fails:
	clang_out_path, ctok_out = fails[0]

	temp_file = tempfile.NamedTemporaryFile(mode='w+b', delete=False)
	temp_file.write(ctok_out)
	ctok_out_path = temp_file.name
	temp_file.close()

	#bcomapre_cmd = f'bcompare {clang_out_path} {ctok_out_path}'
	bcomapre_cmd = f'bcompare /fv="Hex Compare" {clang_out_path} {ctok_out_path}'
	print(bcomapre_cmd)
	subprocess.run(bcomapre_cmd)

exit()



#...
# gen test input steps...
# copy llvm-project to new location before build (also do not include .git)
# delete_non_c_files
# kill udivmodti4_test, it is way too big
# delete files with invalid utf8 
# kill llvm-project\clang\test\C\C2x\n2927.c
# clang\test\CodeGen\string-literal-unicode-conversion.c
# trigrpahs

def delete_non_c_files():
	for foldername, _, filenames in os.walk(os.path.abspath("test/ctok/input/llvm-project")):
		for filename in filenames:
			file_path = os.path.join(foldername, filename)
			_, file_extension = os.path.splitext(filename)
			if file_extension != '.c':
				try:
					os.remove(file_path)
				except Exception as e:
					print(f"Error deleting {file_path}: {e}")
					exit()

def is_valid_utf8(file_path):
	try:
		with open(file_path, 'r', encoding='utf-8') as file:
			file.read()
		return True
	except UnicodeDecodeError:
		return False

def delete_invalid_utf8_files():
	for foldername, _, filenames in os.walk(os.path.abspath("test/ctok/input")):
		for filename in filenames:
			file_path = os.path.join(foldername, filename)
			if not is_valid_utf8(file_path):
				try:
					os.remove(file_path)
				except Exception as e:
					print(f"Error deleting file {file_path}: {e}")
					exit()