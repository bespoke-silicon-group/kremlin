#!/bin/bash

# take 2... please ignore

# The script's directory
ROOT_DIR=`dirname $0`

# Install llvm 2.8
cd $ROOT_DIR/instrument/llvm/
./llvm-2.8-install.sh
