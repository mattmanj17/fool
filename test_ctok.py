
import os
import subprocess
import concurrent.futures
import filecmp

from pathlib import Path

def clang_path():
	return "3rd_party\\llvm-project\\build\\Release\\bin\\clang.exe"

def ctok_path():
	return "build\\exe\\ctok.exe"

def to_out_path(in_path, ext):
	out_path = os.path.join("test/ctok/" + ext, in_path)
	out_path += "." + ext
	return out_path

def bake_path(pth):
	return os.path.normpath(os.path.join(os.getcwd(), pth))

def input_paths():
	for root, dirs, files in os.walk("3rd_party/llvm-project", topdown=True):
		if root == "3rd_party/llvm-project":
			dirs.remove('build')
			dirs.remove('.git')
		for fname in files:
			in_path = os.path.join(root, fname)
			yield in_path

def run_clang(input_path):
	out_path = to_out_path(input_path, "clang_tok")

	input_path = bake_path(input_path)
	out_path = bake_path(out_path)

	cmd = "-dump-raw-tokens"
	warns = "-Wno-unicode-homoglyph"
	invoke_clang = f"{clang_path()} -cc1 {warns} {cmd} -std=c11 -x c" # specifically targeting c11
	Path(os.path.dirname(out_path)).mkdir(parents=True, exist_ok=True)
	with open(out_path, "wb") as out_f:
		subprocess.run(
			invoke_clang + " \"" + input_path + "\"", 
			stderr=out_f)
	print(out_path)
		
def gen_clang_tok():
	with concurrent.futures.ThreadPoolExecutor(max_workers=19) as executor:
		for input_path in input_paths():
			executor.submit(run_clang, input_path)

def run_ctok(input_path):
	out_path = to_out_path(input_path, "ctok_tok")

	input_path = bake_path(input_path)
	out_path = bake_path(out_path)

	Path(os.path.dirname(out_path)).mkdir(parents=True, exist_ok=True)
	with open(out_path, "wb") as out_f:
		subprocess.run(
			ctok_path() + " -raw " + " \"" + input_path + "\"", 
			stdout=out_f)
	print(out_path)

def gen_ctok_tok():	
	with concurrent.futures.ThreadPoolExecutor() as executor:
		for input_path in input_paths():
			executor.submit(run_ctok, input_path)

def compare():
	for input_path in input_paths():
		out_path_ctok = to_out_path(input_path, "ctok_tok")
		out_path_clang = to_out_path(input_path, "clang_tok")

		out_path_ctok = bake_path(out_path_ctok)
		out_path_clang = bake_path(out_path_clang)

		print(input_path)
		if not filecmp.cmp(out_path_clang, out_path_ctok):
			print('failed!')
			print('opening diff in beyond comapre (clang on left, ctok on right)')
			
			# ' /fv="Hex Compare" '
			bcomapre_cmd = f'bcompare "{out_path_clang}" "{out_path_ctok}"'
			print(bcomapre_cmd)
			subprocess.run(bcomapre_cmd)
			break

if __name__ == "__main__":
	compare()