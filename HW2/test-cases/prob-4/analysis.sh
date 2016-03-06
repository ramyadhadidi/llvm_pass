TIME_PASS=""
#TIME_PASS="-time-passes"

#echo ""
#echo "Decompress"
#opt -load ~/scratch/llvm/build/lib/LLVM-HW2-P4.so -Assignment_4 decompress.bc> /dev/null

#echo ""
#echo "Compress"
#opt -load ~/scratch/llvm/build/lib/LLVM-HW2-P4.so -Assignment_4 compress.bc> /dev/null


opt -load ~/scratch/llvm/build/lib/LLVM-HW2-P4.so -Assignment_4 case-1.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW2-P4.so -Assignment_4 case-2.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW2-P4.so -Assignment_4 case-3.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW2-P4.so -Assignment_4 case-4.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW2-P4.so -Assignment_4 case-5.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW2-P4.so -Assignment_4 case-6.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW2-P4.so -Assignment_4 case-7.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW2-P4.so -Assignment_4 case-8.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW2-P4.so -Assignment_4 case-9.bc> /dev/null
opt -load ~/scratch/llvm/build/lib/LLVM-HW2-P4.so -Assignment_4 case-10.bc> /dev/null
