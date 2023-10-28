@echo off
setlocal

if not exist 3rd_party mkdir 3rd_party
cd 3rd_party

if exist llvm-project goto havellvm

git clone https://github.com/llvm/llvm-project.git
cd llvm-project
git fetch --all --tags
git checkout tags/llvmorg-17.0.3 -b 17.0.3-branch
mkdir build 
cd build
cmake -DLLVM_ENABLE_PROJECTS=clang -A x64 -Thost=x64 ..\llvm

cd ..
cd ..

:havellvm

cd llvm-project
msbuild -m:4 build/tools/clang/tools/driver/clang.vcxproj /property:Configuration=Release

endlocal