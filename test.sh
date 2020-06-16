#!/bin/bash

nvcc main.cu kernels.cu nfa.cu

export TIMEFORMAT="%E"
n_tc=`wc testcases -l | cut -d" " -f1`
i_tc=1
while [ $i_tc -le $n_tc ]
do
	for file in `ls data`
	do
		echo -n $i_tc","$file"," >> res
		(time sed -e "`cat testcases | head -n$i_tc | tail -n1 `" data/$file > expectedoutput) 2>>time_gnused
		(time ./a.out -e "`cat testcases | head -n$i_tc | tail -n1 `" -f data/$file -c > tempout) 2>>time_cpused
		diff tempout expectedoutput | wc -l | tr '\n' ',' >> res
		(time ./a.out -e "`cat testcases | head -n$i_tc | tail -n1 `" -f data/$file -b 1 > tempout) 2>>time_cudased1
		echo -n `diff tempout expectedoutput | wc -l`"," >> res
		(time ./a.out -e "`cat testcases | head -n$i_tc | tail -n1 `" -f data/$file -b 5 > tempout) 2>>time_cudased5
		echo -n `diff tempout expectedoutput | wc -l`"," >> res
		(time ./a.out -e "`cat testcases | head -n$i_tc | tail -n1 `" -f data/$file -b 10 > tempout) 2>>time_cudased10
		echo `diff tempout expectedoutput | wc -l` >> res
	done
	let i_tc++
done
paste res time_gnused time_cpused time_cudased1 time_cudased5 time_cudased10 | sed 's/\t/,/g' > results
rm tempout expectedoutput time_gnused time_cpused time_cudased* res
