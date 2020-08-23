# Pre Process
Pre-process extracts interface related information based on AST and converts the information to suitable formats.

We can extract the interface related information as follows.

## Generate cc1 Command
Just run `python gen_all_related_cc1_cmd.py`. 

## Download Android LLVM 
We use the Clang plugin to extract information based on AST. However, the Clang used to compile AOSP is a particularly customized version. As a result, we need to download the corresponding version.

From the prefix of AOSP compilation command,
```bash
$ prebuilts/clang/host/linux-x86/clang-4691093/bin/clang++.real -v
Android (4691093 based on r316199) clang version 6.0.2 (https://android.googlesource.com/toolchain/clang 183abd29fc496f55536e
7d904e0abae47888fc7f) (https://android.googlesource.com/toolchain/llvm 34361f192e41ed6e4e8f9aca80a4ea7e9856f327) (based on LLVM 6.0.2svn)
```
we can know the target llvm version. We can then download the llvm projects.

```bash
git clone https://android.googlesource.com/toolchain/llvm llvm-android
cd llvm-android
git checkout llvm-r316199
cd tools
rm clang
git clone https://android.googlesource.com/toolchain/clang 
cd clang 
git checkout llvm-r316199
rm lld
git clone https://android.googlesource.com/toolchain/lld
cd lld 
git checkout llvm-r316199
```

## Compile Clang Plugin BinderIface
First, we should create a symbolic link of `BinderIface` at the `tools/clang/examples/` folder of the downloaded LLVM.
```bash
ln -s /path/to/fans/interface-model-extractor/pre-process/BinderIface /path/to/llvm-android/tools/clang/examples/
```
Second, add `add_subdirectory(BinderIface)` at the end of file `llvm-android/tools/clang/examples/CMakeLists.txt`. After that, the `CMakeLists.txt` should be

```bash
$ cat CMakeLists.txt
if(NOT CLANG_BUILD_EXAMPLES)
  set_property(DIRECTORY PROPERTY EXCLUDE_FROM_ALL ON)
  set(EXCLUDE_FROM_ALL ON)
endif()

if(CLANG_ENABLE_STATIC_ANALYZER)
add_subdirectory(analyzer-plugin)
endif()
add_subdirectory(clang-interpreter)
add_subdirectory(PrintFunctionNames)
add_subdirectory(AnnotateFunctions)
add_subdirectory(BinderIface)
```

Finally, compile the BinderIface plugin.

```bash
cd /path/to/llvm-android
mkdir build
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE="Release" -DLLVM_TARGETS_TO_BUILD="X86" ..
# replace the N_PROCS with the number you want, e.g., make -j15
make -j [N_PROCS]  
make BinderIface
```

> You might need to build Z3 from source code (https://github.com/Z3Prover/z3) before compiling the BinderIface plugin.

Besides, you can use `cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=YES ..` to generate the compilation commands of llvm to configure your editor (e.g., vscode) to enable auto-complete functionality.


## Deal with "Corner" Cases 

Currently, we find that a few special cases are not easy to deal with, e.g., `case` statements in the `switch` statement are not wrapped by `{` and `}`. We need to write lots of code to deal with them, so here we modify them manually without affecting the semantics. 

Besides, it stands to reason that the types of variables in `readFromParcel` and `writeToParcel` functions of one parcelable structure should match. However, we find variable types may be imprecise due to the non-standard nature of the code. As a result, we try to make the variable type precise through these two functions utilizing the nature that the order they operate on variables should be the same. For example, we can improve the variable types in `readFromParcel` of `class aaudio::AAudioStreamConfiguration`(frameworks/av/media/libaaudio/src/binding/AAudioStreamConfiguration.cpp) through the variable types in `writeToParcel` function. 

For details, please see [Misc](misc.md).

## Extract Interface Related Information Based on AST

After solving the above "corner" cases, we could start extracting the rough interface model. 

Firstly, we should create some symbolic links.
```bash
cd /path/to/aosp
ln -s /path/to/fans/workdir/service-related-file/misc_parcel_related_function.txt .
ln -s /path/to/fans/workdir/service-related-file/special_parcelable_function.txt .
```

Then, we can use `extract_from_ast.py` to extract related information. This script provides several functionalities.
```bash
$ python extract_from_ast.py --help
usage: extract_from_ast.py [-h] [-f FILE]

Extract interface information from ast

optional arguments:
  -h, --help            show this help message and exit
  -f FILE, --file FILE  process the target file
```

When no argument is given, 

```bash
$ python extract_from_ast.py       
Do you want to remove all of the files and extract again?y/n
```

This script will extract interface related information according to your demand.

Generally, when you extract the interface related information for the first time, you should choose `y`. Later, when you need to process new files (e.g., some new misc functions are discovered in some new files), you'd better choose `n` to save your time.

The extracted information will be stored in `/path/to/aosp/data` folder as decribed by the `rough_interface_related_data_dir` in the `fans.cfg`.