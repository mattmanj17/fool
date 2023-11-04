
import os
import subprocess
import concurrent.futures
from pathlib import Path
from threading import Lock
import tempfile
import shutil

# build llvm-project/build/Release/bin/clang.exe

# BB (matthewd) should re write this without os.chdir, they are confusing

def build_clang():
	if not os.path.exists('.3rd_party'):
		os.mkdir('.3rd_party')
	os.chdir('.3rd_party')
	
	if os.path.exists('llvm-project'):
		os.chdir('..')
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

	os.chdir('..')

build_clang()

# generate .test/input

if not os.path.exists('.test'):
	os.mkdir('.test')

if not os.path.exists('.test/input'):
	os.mkdir('.test/input')

	skip_dirs = [
		"build",
		".git",
	]

	skip_files = [

		# this file is stupid big and takes a second for clang to even spit out the tokens.
		# skip it to avoid an annoying wait when we run_clang.

		os.path.abspath(".3rd_party/llvm-project/compiler-rt/test/builtins/Unit/udivmodti4_test.c"),

		# these files have Named UCNs in them (\N{some-code-point}), which we do not support

		os.path.abspath(".3rd_party/llvm-project/clang/test/FixIt/fixit-unicode-named-escape-sequences.c"),
		os.path.abspath(".3rd_party/llvm-project/clang/test/Lexer/unicode.c"),
		os.path.abspath(".3rd_party/llvm-project/clang/test/Preprocessor/ucn-pp-identifier.c"),
		os.path.abspath(".3rd_party/llvm-project/clang/test/Sema/ucn-identifiers.c"),
	]

	src_dir = os.path.abspath('.3rd_party/')
	dest_dir = os.path.abspath('.test/input/')

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

# copy directory structure of .test/input to .test/output

def cases():
	in_dir = os.path.abspath(".test/input")
	out_dir = os.path.abspath(".test/output")
	for root, _, files in os.walk(in_dir):
		for fname in files:
			in_path = os.path.join(root, fname)
			rel_path = os.path.relpath(in_path, in_dir)
			
			out_path = os.path.join(out_dir, rel_path)
			out_path += ".tokens"

			yield (in_path, out_path)

print('mkdir ...')

for in_path, out_path in cases():
	Path(os.path.dirname(out_path)).mkdir(parents=True, exist_ok=True)

# generate .test/output

def run_clang(clang_path, in_path, out_path):
	with open(out_path, "wb") as out_f:
		subprocess.run(
			f'{clang_path} -cc1 -dump-raw-tokens -std=c11 -x c {in_path}', 
			stderr=out_f)
	print(f'run_clang {in_path}')

with concurrent.futures.ThreadPoolExecutor(max_workers=19) as executor: # clang is a little heavy weight, so we limit to 19 workers
	clang = os.path.abspath(".3rd_party/llvm-project/build/Release/bin/clang.exe")
	for in_path, out_path in cases():
		if not os.path.isfile(out_path):
			executor.submit(run_clang, clang, in_path, out_path)

# compare ctok output to .test/output

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
	ctok = os.path.abspath(".build/exe/ctok.exe")
	for in_path, out_path in cases():
		executor.submit(run_ctok, ctok, in_path, out_path)

# Print failures (and open beyond compare)

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
