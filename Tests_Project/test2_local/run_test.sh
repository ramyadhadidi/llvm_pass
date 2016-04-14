../clang test2_local.cpp -emit-llvm -c -o test2_local.bc
../opt -bounds-checking <test2_local.bc >test2_opt.bc
