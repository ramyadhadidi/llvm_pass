../clang -O3 test4_local.cpp -emit-llvm -c -o test4_local.bc
../opt -bounds-checking <test4_local.bc >test4_opt.bc
