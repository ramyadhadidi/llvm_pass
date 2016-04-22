#./clang -S -g -O0 -emit-llvm array.c -c -o array.ll
#./clang -g -O0 -emit-llvm array.c -c -o array.bc
./clang -S -O0 -emit-llvm checkbounds.c -c -o checkbounds.ll
./clang -O0 -emit-llvm checkbounds.c -c -o checkbounds.bc
./clang -S -O0 -emit-llvm loop.c -c -o loop.ll
./clang -O0 -emit-llvm loop.c -c -o loop.bc
./clang -S -O0 -emit-llvm loop1.c -c -o loop1.ll
./clang -O0 -emit-llvm loop1.c -c -o loop1.bc
./clang -S -O0 -emit-llvm loop2.c -c -o loop2.ll
./clang -O0 -emit-llvm loop2.c -c -o loop2.bc
./clang -S -O0 -emit-llvm loop3.c -c -o loop3.ll
./clang -O0 -emit-llvm loop3.c -c -o loop3.bc
./clang -S -O0 -emit-llvm loop4.c -c -o loop4.ll
./clang -O0 -emit-llvm loop4.c -c -o loop4.bc
./clang -S -O0 -emit-llvm loop5.c -c -o loop5.ll
./clang -O0 -emit-llvm loop5.c -c -o loop5.bc
./clang -S -O0 -emit-llvm loop6.c -c -o loop6.ll
./clang -O0 -emit-llvm loop6.c -c -o loop6.bc

#./clang -S -O0 -emit-llvm example.c -c -o example.ll
#./clang -O0 -emit-llvm example.c -c -o example.bc

#./clang -S -O0 -emit-llvm stream.c -c -o stream.ll
#./clang -O0 -emit-llvm stream.c -c -o stream.bc

#./opt -S -load ../llvmprojects/build/lib/project.so -project -stats --debug-pass=Structure < array.bc > array.o 2>array.out
./opt -S -load ../llvmprojects/build/lib/project.so -project -stats --debug-pass=Structure < loop.bc > loop.o 2>loop.out
./opt -S -load ../llvmprojects/build/lib/project.so -project -stats --debug-pass=Structure < loop1.bc > loop1.o 2>loop1.out
./opt -S -load ../llvmprojects/build/lib/project.so -project -stats --debug-pass=Structure < loop2.bc > loop2.o 2>loop2.out
./opt -S -load ../llvmprojects/build/lib/project.so -project -stats --debug-pass=Structure < loop3.bc > loop3.o 2>loop3.out
./opt -S -load ../llvmprojects/build/lib/project.so -project -stats --debug-pass=Structure < loop4.bc > loop4.o 2>loop4.out
./opt -S -load ../llvmprojects/build/lib/project.so -project -stats --debug-pass=Structure < loop5.bc > loop5.o 2>loop5.out
./opt -S -load ../llvmprojects/build/lib/project.so -project -stats --debug-pass=Structure < loop6.bc > loop6.o 2>loop6.out

#./opt -S -load ../llvmprojects/build/lib/project.so -project -stats --debug-pass=Structure < stream.bc > stream.o 2>stream.out
#./opt -S -gvn -constprop -constmerge -consthoist -load ../llvmprojects/build/lib/project.so -project -stats --debug-pass=Structure < loop2.bc > loop2.o 2>loop2.out


#./llvm-link -S checkbounds.o loop2.o -o linked
#./llvm/bin/lli linked

