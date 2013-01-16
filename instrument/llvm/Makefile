.PHONY: build-llvm build-llvm-gcc

LLVM_VERSION=2.8
LLVM_GCC_LANGS=c,c++

INSTALL_DIR=$(CURDIR)/install

LLVM_SRC_DIR=llvm-$(LLVM_VERSION)
LLVM_OBJ_DIR=llvm-$(LLVM_VERSION)-obj
LLVM_GCC_SRC_DIR=llvm-gcc-4.2-$(LLVM_VERSION).source
LLVM_GCC_OBJ_DIR=llvm-gcc-4.2-$(LLVM_VERSION)-obj

# tgz source files for building llvm and llvm-gcc
LLVM_SRC_TAR=llvm-$(LLVM_VERSION).tgz
LLVM_GCC_SRC_TAR=llvm-gcc-4.2-$(LLVM_VERSION).source.tgz

# urls for tgz files
LLVM_SRC_TAR_URL=http://llvm.org/releases/$(LLVM_VERSION)/$(LLVM_SRC_TAR)
LLVM_GCC_SRC_TAR_URL=http://llvm.org/releases/$(LLVM_VERSION)/$(LLVM_GCC_SRC_TAR)

all: build-llvm build-llvm-gcc
build-llvm: llvm-$(LLVM_VERSION).build.success
build-llvm-gcc: llvm-gcc-4.2-$(LLVM_VERSION).build.success

$(LLVM_SRC_TAR):
	wget $(LLVM_SRC_TAR_URL)

$(LLVM_GCC_SRC_TAR):
	wget $(LLVM_GCC_SRC_TAR_URL)

$(LLVM_SRC_DIR): $(LLVM_SRC_TAR)
	tar -xzf $<

$(LLVM_GCC_SRC_DIR): $(LLVM_GCC_SRC_TAR)
	tar -xzf $<

$(LLVM_OBJ_DIR):
	mkdir $@

$(LLVM_GCC_OBJ_DIR):
	mkdir $@

$(LLVM_OBJ_DIR)/Makefile: $(LLVM_OBJ_DIR) $(LLVM_SRC_DIR)
	cd $< && ../$(LLVM_SRC_DIR)/configure --prefix=$(INSTALL_DIR) --enable-optimized --enable-assertions
	@touch $@

$(LLVM_GCC_OBJ_DIR)/Makefile: $(LLVM_GCC_OBJ_DIR) $(LLVM_GCC_SRC_DIR)
	cd $< && ../$(LLVM_GCC_SRC_DIR)/configure --prefix=$(INSTALL_DIR) --enable-languages=$(LLVM_GCC_LANGS) --program-prefix=llvm- --enable-llvm=$(CURDIR)/$(LLVM_OBJ_DIR) --disable-multilib --disable-bootstrap --enable-checking
	@touch $@

llvm-$(LLVM_VERSION).build.success: $(LLVM_OBJ_DIR)/Makefile
	make -C $(LLVM_OBJ_DIR)
	make -C $(LLVM_OBJ_DIR) install
	touch $@

llvm-gcc-4.2-$(LLVM_VERSION).build.success: $(LLVM_GCC_OBJ_DIR)/Makefile llvm-$(LLVM_VERSION).build.success
	make -C $(LLVM_GCC_OBJ_DIR)
	make -C $(LLVM_GCC_OBJ_DIR) install
	touch $@

clean:
	@rm -rf $(LLVM_SRC_TAR) $(LLVM_GCC_SRC_TAR) $(LLVM_SRC_DIR) $(LLVM_GCC_SRC_DIR) $(LLVM_OBJ_DIR) $(LLVM_GCC_OBJ_DIR) llvm-*.build.success install/*
