#!/bin/bash

dd if=/dev/zero of=A bs=1 count=$((4*1024*1024+1))

printf '\x01' | dd conv=notrunc of=A bs=1 seek=0
printf '\x01' | dd conv=notrunc of=A bs=1 seek=10000
printf '\x01' | dd conv=notrunc of=A bs=1 seek=$((4*1024*1024))