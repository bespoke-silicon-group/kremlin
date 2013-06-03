.PHONY: build-llvm build-llvm-gcc clean

LLVM_VERSION=3.0
LLVM_GCC_LANGS=c,c++

UNAME := $(shell uname)

INSTALL_DIR=$(CURDIR)/install

LLVM_CONFIG_OPTS = --prefix=$(INSTALL_DIR) --enable-assertions
LLVM_GCC_CONFIG_OPTS = --prefix=$(INSTALL_DIR) --enable-languages=$(LLVM_GCC_LANGS) --program-prefix=llvm- --enable-llvm=$(CURDIR)/$(LLVM_OBJ_DIR) --disable-multilib --disable-bootstrap --enable-checking

# Bug with LLVM 2.8 on mac requires debug (i.e. unoptimized) llvm build
ifeq ($(UNAME),Darwin)
LLVM_CONFIG_OPTS += --disable-optimized
else
LLVM_CONFIG_OPTS += --enable-optimized
endif

LLVM_SRC_DIR=llvm-$(LLVM_VERSION).src
LLVM_OBJ_DIR=llvm-$(LLVM_VERSION)-obj
LLVM_GCC_SRC_DIR=llvm-gcc-4.2-$(LLVM_VERSION).source
LLVM_GCC_OBJ_DIR=llvm-gcc-4.2-$(LLVM_VERSION)-obj
CLANG_SRC_DIR=$(LLVM_SRC_DIR)/tools/clang

# tar.gz source files for building llvm and llvm-gcc
LLVM_SRC_TAR=llvm-$(LLVM_VERSION).tar.gz
LLVM_GCC_SRC_TAR=llvm-gcc-4.2-$(LLVM_VERSION).source.tgz
CLANG_SRC_TAR=clang-$(LLVM_VERSION).tar.gz

# urls for tgz files
LLVM_SRC_TAR_URL=http://llvm.org/releases/$(LLVM_VERSION)/$(LLVM_SRC_TAR)
LLVM_GCC_SRC_TAR_URL=http://llvm.org/releases/$(LLVM_VERSION)/$(LLVM_GCC_SRC_TAR)
CLANG_SRC_TAR_URL=http://llvm.org/releases/$(LLVM_VERSION)/$(CLANG_SRC_TAR)

# patch of LLVM src to support kremlin
LLVM_SRC_PATCH=kremlin-llvm-$(LLVM_VERSION).patch

all: build-llvm
build-llvm: llvm-$(LLVM_VERSION).build.success
build-llvm-gcc: llvm-gcc-4.2-$(LLVM_VERSION).build.success

$(LLVM_SRC_TAR):
	wget $(LLVM_SRC_TAR_URL)

$(LLVM_GCC_SRC_TAR):
	wget $(LLVM_GCC_SRC_TAR_URL)

$(CLANG_SRC_TAR):
	wget $(CLANG_SRC_TAR_URL)

$(LLVM_SRC_DIR): $(LLVM_SRC_TAR)
	tar -xzf $<
	patch -p0 < $(LLVM_SRC_PATCH)
	@touch $@

$(LLVM_GCC_SRC_DIR): $(LLVM_GCC_SRC_TAR)
	tar -xzf $<
	@touch $@

$(CLANG_SRC_DIR): $(CLANG_SRC_TAR)
	tar -xzf $<
	mv clang-$(LLVM_VERSION).src $@
	@touch $@

$(LLVM_OBJ_DIR):
	mkdir $@

$(LLVM_GCC_OBJ_DIR):
	mkdir $@

$(LLVM_OBJ_DIR)/Makefile: $(LLVM_OBJ_DIR) $(LLVM_SRC_DIR) $(CLANG_SRC_DIR)
	cd $< && ../$(LLVM_SRC_DIR)/configure $(LLVM_CONFIG_OPTS)
	@touch $@

$(LLVM_GCC_OBJ_DIR)/Makefile: $(LLVM_GCC_OBJ_DIR) $(LLVM_GCC_SRC_DIR)
	cd $< && ../$(LLVM_GCC_SRC_DIR)/configure $(LLVM_GCC_CONFIG_OPTS)
	@touch $@

llvm-$(LLVM_VERSION).build.success: $(LLVM_OBJ_DIR)/Makefile
	make -C $(LLVM_OBJ_DIR)
	make -C $(LLVM_OBJ_DIR) install
	@touch $<
	@date > $@

llvm-gcc-4.2-$(LLVM_VERSION).build.success: $(LLVM_GCC_OBJ_DIR)/Makefile llvm-$(LLVM_VERSION).build.success
	make -C $(LLVM_GCC_OBJ_DIR)
	make -C $(LLVM_GCC_OBJ_DIR) install
	@touch $<
	@date > $@

clean:
	@rm -rf $(LLVM_SRC_TAR) $(LLVM_GCC_SRC_TAR) $(LLVM_SRC_DIR) $(LLVM_GCC_SRC_DIR) $(LLVM_OBJ_DIR) $(LLVM_GCC_OBJ_DIR) llvm-*.build.success install/*
