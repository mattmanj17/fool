
import os
import subprocess
import concurrent.futures
from pathlib import Path
import filecmp

def run_clang(in_path, out_path):
	clang_path = os.path.abspath("3rd_party/llvm-project/build/Release/bin/clang.exe")
	Path(os.path.dirname(out_path)).mkdir(parents=True, exist_ok=True)
	with open(out_path, "wb") as out_f:
		subprocess.run(
			f'{clang_path} -cc1 -dump-raw-tokens -std=c11 -x c {in_path}', 
			stderr=out_f)
	print(out_path)
		
def run_ctok(in_path, out_path):
	ctok_path = os.path.abspath("build/exe/ctok.exe")
	Path(os.path.dirname(out_path)).mkdir(parents=True, exist_ok=True)
	with open(out_path, "wb") as out_f:
		subprocess.run(
			f'{ctok_path} -raw {in_path}', 
			stdout=out_f)
	print(out_path)

# list cases (input path, clang output path, ctok output path)

input_dir = os.path.abspath("test/ctok/input/llvm-project")
clang_out_dir = os.path.abspath("test/ctok/output/clang")
ctok_out_dir = os.path.abspath("test/ctok/output/ctok")

cases = []
for root, _, files in os.walk(input_dir):
	for fname in files:
		in_path = os.path.join(root, fname)
		relpath = os.path.relpath(in_path, input_dir)
		
		clang_out_path = os.path.join(clang_out_dir, relpath)
		clang_out_path += ".tokens"
		
		ctok_out_path = os.path.join(ctok_out_dir, relpath)
		ctok_out_path += ".tokens"

		cases.append((in_path, clang_out_path, ctok_out_path))

# ensure clang and ctok output

with concurrent.futures.ThreadPoolExecutor(max_workers=19) as executor:
	for in_path, clang_out_path, ctok_out_path in cases:
		if not os.path.isfile(clang_out_path):
			executor.submit(run_clang, in_path, clang_out_path)
		if not os.path.isfile(ctok_out_path):
			executor.submit(run_ctok, in_path, ctok_out_path)

# count failures

num_files = 0
num_failed = 0
for in_path, clang_out_path, ctok_out_path in cases:
	print(in_path)
	num_files += 1
	if not filecmp.cmp(clang_out_path, ctok_out_path):
		num_failed += 1

print(f'{num_failed} of {num_files} failed')
