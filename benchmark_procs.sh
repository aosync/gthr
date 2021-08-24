#!/bin/sh

procs=8
if type nproc; then
	procs=$(nproc)
fi

i=1

while [ $i -lt $procs ]; do
	time ./bld/gthrmain $i
	i=$((i+1))
done
