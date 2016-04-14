../clang -O0 loop.cpp -emit-llvm -c -o loop.bc
../opt -loop-simplify <loop.bc >loop_simplify.bc
../opt -bounds-checking <loop_simplify.bc >loop_opt.bc
