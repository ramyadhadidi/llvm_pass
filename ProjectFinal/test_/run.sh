#../llvm/bin/clang -S -g -O0 -emit-llvm array.c -c -o array.bc
../llvm/bin/clang -S -O0 -emit-llvm checkbounds.c -c -o checkbounds.o
../llvm/bin/clang -S -g -O0 -emit-llvm example.c -c -o array.bc
../llvm/bin/opt -S  -gvn -load ../build/lib/project.so -project -stats  < array.bc > out.o 2>tmp.out

../llvm/bin/llvm-link -S checkbounds.o out.o -o linked
../llvm/bin/lli linked

