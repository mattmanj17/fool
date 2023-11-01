
import os
import shutil

skip_dirs = [
	"build",
	".git",
]

skip_files = [

	# this file is stupid big and takes a second for clang to even spit out the tokens.
	# skip it to avoid an annoying wait when we run_clang.

	os.path.abspath("3rd_party/llvm-project/compiler-rt/test/builtins/Unit/udivmodti4_test.c"),

	# these files have Named UCNs in them (\N{some-code-point}), which we do not support

	os.path.abspath("3rd_party/llvm-project/clang/test/FixIt/fixit-unicode-named-escape-sequences.c"),
	os.path.abspath("3rd_party/llvm-project/clang/test/Lexer/unicode.c"),
	os.path.abspath("3rd_party/llvm-project/clang/test/Preprocessor/ucn-pp-identifier.c"),
	os.path.abspath("3rd_party/llvm-project/clang/test/Sema/ucn-identifiers.c"),
]

def copy_c_files(src_dir, dest_dir):
	for root, dirs, fnames in os.walk(src_dir, topdown=True):
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

source_directory = os.path.abspath('3rd_party/')
destination_directory = os.path.abspath('test/ctok/input/')

copy_c_files(source_directory, destination_directory)