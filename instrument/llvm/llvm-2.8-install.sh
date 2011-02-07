#!/bin/bash
#
# Downloads llvm 2.8 into ./2.8-src, then makes into ./2.8-obj and
# installs into ./2.8-install.
# 
# Where llvm-gcc will be installed to
PREFIX=$PWD/install

LLVM_SRC=2.8-src
LLVM_OBJ=2.8-obj
LLVM_GCC_SRC=llvm-gcc-4.2-2.8-src
LLVM_GCC_OBJ=llvm-gcc-4.2-2.8-obj

# Make the prefix directory
mkdir $PREFIX

# Get the files
wget http://llvm.org/releases/2.8/llvm-2.8.tgz
wget http://llvm.org/releases/2.8/llvm-gcc-4.2-2.8.source.tgz

# Unzip the files
tar xzf llvm-2.8.tgz
tar xzf llvm-gcc-4.2-2.8.source.tgz

# Rename the folders to shorter names
mv llvm-2.8 $LLVM_SRC
mv llvm-gcc-4.2-2.8.source $LLVM_GCC_SRC

# Make the object directories
mkdir $LLVM_OBJ
mkdir $LLVM_GCC_OBJ

# Set compilers
export CC=gcc34
export CXX=g++34

# Configure, make and install LLVM
cd $LLVM_OBJ
../$LLVM_SRC/configure --prefix=$PREFIX --enable-optimized --enable-assertions
make
make install

# Configure and make llvm-gcc
cd ../$LLVM_GCC_OBJ
../$LLVM_GCC_SRC/configure --prefix=$PREFIX \
--enable-languages=c,c++,fortran --program-prefix=llvm- \
--enable-llvm=$PWD/../$LLVM_OBJ --disable-multilib \
--disable-bootstrap --enable-checking

make
make install
