#! /bin/bash

# Example of how to resolve the addresses in a file. Implicitly uses the
# System.map file generated during compilation of the Linux kernel
#
# go run resolve.go "trace.1218" > "trace.1218.res"

# Create a destination folder for the signatures
mkdir -p signatures.mine

echo "Analysing trace.1029.res"
go run analyse.go trace.1029.res
