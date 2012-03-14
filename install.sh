#!/bin/bash

# The script's directory
ROOT_DIR=`dirname $0`

# Install llvm 2.8
cd $ROOT_DIR/instrument/llvm/
./llvm-2.8-install.sh
