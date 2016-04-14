../clang -O0 global.cpp -emit-llvm -c -o global.bc
../opt -bounds-checking <global.bc >global_opt.bc
