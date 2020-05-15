#!/bin/bash
n_tc=`wc testcases -l | cut -d" " -f1`
i_tc=1
while [ $i_tc -le $n_tc ]
do
	for file in `ls data`
	do
		sed -e "`cat testcases | head -n$i_tc | tail -n1 `" data/$file > expectedoutput
		./a.out -e "`cat testcases | head -n$i_tc | tail -n1 `" -f data/$file > tempout
		echo `cat testcases | head -n$i_tc | tail -n1 `" "$file
		diff tempout expectedoutput -a
	done
	let i_tc++
done
rm tempout expectedoutput
