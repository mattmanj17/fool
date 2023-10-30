
import os
import shutil

def copy_c_files(src_dir, dest_dir):
    for root, _, fnames in os.walk(src_dir):
        for fname in fnames:
            if fname.endswith('.c'):
                src_file = os.path.join(root, fname)
                rel_path = os.path.relpath(src_file, src_dir)
                destination_file = os.path.join(dest_dir, rel_path)
                os.makedirs(os.path.dirname(destination_file), exist_ok=True)
                shutil.copy2(src_file, destination_file)

source_directory = '3rd_party/llvm-project/'
destination_directory = 'test/ctok/input/'

copy_c_files(source_directory, destination_directory)