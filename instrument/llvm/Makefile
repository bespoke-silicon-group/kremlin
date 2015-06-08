.PHONY: build-llvm build-llvm-gcc clean

LLVM_VERSION=3.6.1

UNAME := $(shell uname)

INSTALL_DIR=$(CURDIR)/install

LLVM_CONFIG_OPTS = --prefix=$(INSTALL_DIR) --enable-assertions
LLVM_MAKE_OPTS = REQUIRES_RTTI=1 REQUIRES_EH=1

# Bug with LLVM 2.8 on mac requires debug (i.e. unoptimized) llvm build
#ifeq ($(UNAME),Darwin)
#LLVM_CONFIG_OPTS += --disable-optimized
#else
LLVM_CONFIG_OPTS += --enable-optimized
#endif

LLVM_SRC_DIR=llvm-$(LLVM_VERSION).src
LLVM_OBJ_DIR=llvm-$(LLVM_VERSION)-obj
CLANG_SRC_DIR=$(LLVM_SRC_DIR)/tools/clang

# tar.xz source files for building llvm and llvm-gcc
LLVM_SRC_TAR=llvm-$(LLVM_VERSION).src.tar.xz
CLANG_SRC_TAR=cfe-$(LLVM_VERSION).src.tar.xz

# urls for tgz files
LLVM_SRC_TAR_URL=http://llvm.org/releases/$(LLVM_VERSION)/$(LLVM_SRC_TAR)
CLANG_SRC_TAR_URL=http://llvm.org/releases/$(LLVM_VERSION)/$(CLANG_SRC_TAR)

all: build-llvm
build-llvm: llvm-$(LLVM_VERSION).build.success
build-llvm-gcc: llvm-gcc-4.2-$(LLVM_VERSION).build.success

$(LLVM_SRC_TAR):
	wget $(LLVM_SRC_TAR_URL)

$(CLANG_SRC_TAR):
	wget $(CLANG_SRC_TAR_URL)

$(LLVM_SRC_DIR): $(LLVM_SRC_TAR)
	tar -xJf $<
	@touch $@

$(CLANG_SRC_DIR): $(CLANG_SRC_TAR)
	tar -xJf $<
	mv cfe-$(LLVM_VERSION).src $@
	@touch $@

$(LLVM_OBJ_DIR):
	mkdir $@

$(LLVM_OBJ_DIR)/Makefile: $(LLVM_OBJ_DIR) $(LLVM_SRC_DIR) $(CLANG_SRC_DIR)
	cd $< && ../$(LLVM_SRC_DIR)/configure $(LLVM_CONFIG_OPTS)
	@touch $@

llvm-$(LLVM_VERSION).build.success: $(LLVM_OBJ_DIR)/Makefile
	make -C $(LLVM_OBJ_DIR) $(LLVM_MAKE_OPTS)
	make -C $(LLVM_OBJ_DIR) install
	@touch $<
	@date > $@

clean:
	@rm -rf $(LLVM_SRC_TAR) $(LLVM_SRC_DIR) $(LLVM_OBJ_DIR) llvm-*.build.success install/*
