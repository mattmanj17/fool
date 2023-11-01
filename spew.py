import os
import itertools

# '/' not included to avoid generating comments

chars = b"@\"\\\n\x20'8uULE0_[](){}.:%><#,=|^.;?;*+-&!~"

out_dir = "test/ctok/input/spew"

if not os.path.exists(out_dir):
	os.makedirs(out_dir, exist_ok=True)

findex = 0
for prefix in itertools.product(chars, repeat=2):
	prefix_bytes = bytes(prefix)
	suffixes = itertools.product(chars, repeat=3) 
	combo_bytes = [(prefix_bytes + bytes(suffix)) for suffix in suffixes]
	result = b'\n'.join(combo_bytes)
	file_name = f"{out_dir}/spew_{findex}.txt"
	with open(file_name, "wb") as binary_file:
		binary_file.write(result)
	findex += 1
