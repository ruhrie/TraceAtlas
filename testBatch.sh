#!/bin/bash
# epoch=200
# print_freq=50
# model=vgg11
app=gemm
trc=~/Dash-cor-dev/Dash-Corpus/GSL/GSL_projects_L/blas/build/raw.trc
btc=~/Dash-cor-dev/Dash-Corpus/GSL/GSL_projects_L/blas/build/example_1.tra
koi=0

mkdir ./out
rm -r ./out/$app
mkdir ./out/$app
mkdir ./out/$app/wset
mkdir ./out/$app/res


./build/bin/KernelSerial -koi $koi -n 0 -e 0 -en 0 -t $trc  \
       -k JRout.json -b $btc  -o ./out/$app/wset/res_$n-$e-$en.json -nb >> ./out/$app/res/res_$n-$e-$en.txt &



for n in 5 10 15 25
do
  for e in 0.1 0.25 0.5 0.75
  do
    #   echo "python3.7 main.py  --arch=$model --print-freq=${print_freq} \
    #   --epochs=$epoch --workersize=$GPU_num --save-dir=save_598_GPU_${GPU_num}_${model}_floating_${grad_quant_bits} \
    #   --grad_quant --grad_quant_bits=$grad_quant_bits"
    #   python3.7 main.py  --arch=$model --print-freq=${print_freq} \
    #   --epochs=$epoch --workersize=$GPU_num --save-dir=save_598_GPU_${GPU_num}_${model}_floating_${grad_quant_bits} \
    #   --grad_quant=0 --grad_quant_bits=$grad_quant_bits

     ./build/bin/KernelSerial -koi $koi -n $n -e $e -en 1 -t $trc  \
       -k JRout.json -b $btc  -o ./out/$app/wset/res_$n-$e-$en.json -nb >> ./out/$app/res/res_$n-$e-$en.txt &

  done
done 
