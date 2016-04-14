../clang test3_local.cpp -emit-llvm -c -o test3_local.bc
../opt -bounds-checking <test3_local.bc >test3_opt.bc
