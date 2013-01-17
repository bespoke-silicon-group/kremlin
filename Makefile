.PHONY: build clean

LLVM_BASE_DIR=instrument/llvm
PLANNER_DIR=planner

build:
	make -C $(LLVM_BASE_DIR)
	make -C $(PLANNER_DIR)

clean:
	make -C $(LLVM_BASE_DIR) clean
	make -C $(PLANNER_DIR) clean
