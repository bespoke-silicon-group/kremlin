.PHONY: build

LLVM_BASE_DIR=instrument/llvm

build:
	make -C $(LLVM_BASE_DIR)
