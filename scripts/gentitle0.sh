#! /bin/bash

id=`echo "$1" | sed "s/[^0-9]//g"`
let lineno=$id+1

head -$lineno titles | tail -1
