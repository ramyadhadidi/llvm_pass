../clang -O0 local.cpp -emit-llvm -c -o local.bc
../opt -bounds-checking <local.bc >opt.bc
