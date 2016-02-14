TIME_PASS=""
#TIME_PASS="-time-passes"

echo ""
echo "Decompress"
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_1 $TIME_PASS decompress.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_2 $TIME_PASS decompress.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_3 $TIME_PASS decompress.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_4 $TIME_PASS decompress.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_5 $TIME_PASS decompress.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P3.so -Assignment_3_1 $TIME_PASS  decompress.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P3.so -Assignment_3_2 $TIME_PASS decompress.bc> /dev/null
#opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P3.so -Assignment_3_3 $TIME_PASS decompress.bc> /dev/null
#opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P3.so -Assignment_3_4 $TIME_PASS decompress.bc> /dev/null


echo ""
echo "Compress"
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_1 $TIME_PASS compress.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_2 $TIME_PASS compress.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_3 $TIME_PASS compress.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_4 $TIME_PASS compress.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_5 $TIME_PASS compress.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P3.so -Assignment_3_1 $TIME_PASS  compress.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P3.so -Assignment_3_2 $TIME_PASS compress.bc> /dev/null
#opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P3.so -Assignment_3_3 $TIME_PASS compress.bc> /dev/null
#opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P3.so -Assignment_3_4 $TIME_PASS compress.bc> /dev/null


#echo ""
#echo "Hello"
#opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_1 $TIME_PASS hello.bc> /dev/null
#opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_2 $TIME_PASS hello.bc> /dev/null
#opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_3 $TIME_PASS hello.bc> /dev/null
#opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_4 $TIME_PASS hello.bc> /dev/null
#opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P2.so -Assignment_2_5 $TIME_PASS hello.bc> /dev/null
#opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P3.so -Assignment_3_1 $TIME_PASS  hello.bc> /dev/null
#opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P3.so -Assignment_3_2 $TIME_PASS hello.bc> /dev/null
#opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P3.so -Assignment_3_3 $TIME_PASS hello.bc> /dev/null
#opt -load ~/scratch/llvm/build/lib/LLVM-HW1-P3.so -Assignment_3_4 $TIME_PASS hello.bc> /dev/null

