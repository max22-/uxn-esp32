#!/bin/bash

echo "Formatting.."
clang-format -i tables.c

echo "Cleaning.."
rm -f ../../bin/tables

echo "Building.."
mkdir -p ../../bin
cc  -lm tables.c -o ../../bin/tables

echo "Assembling.."
../../bin/tables 

echo "Done."
