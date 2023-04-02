#!/bin/bash

make build

./create_A.sh

./sparse A B 

gzip -k A
gzip -k B

gzip -cd B.gz | ./sparse C

./sparse -b 100 A D

stat A >> result.txt
stat A.gz >> result.txt
stat B >> result.txt
stat B.gz >> result.txt
stat C >> result.txt
stat D >> result.txt
