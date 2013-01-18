.PHONY: build clean

LLVM_BASE_DIR=instrument/llvm
PLANNER_DIR=planner

build:
	make -C $(LLVM_BASE_DIR)
	make -C $(PLANNER_DIR)
	@echo "***IMPORTANT*** kremlin requires the following directory as part of your PATH: $(CURDIR)/bin"

clean:
	make -C $(LLVM_BASE_DIR) clean
	make -C $(PLANNER_DIR) clean
