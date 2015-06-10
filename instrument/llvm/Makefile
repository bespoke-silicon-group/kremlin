.PHONY: build-llvm clean

LLVM_VERSION=3.6.1

INSTALL_DIR=$(CURDIR)/install

LLVM_CMAKE_OPTS = -DLLVM_ENABLE_EH=ON -DLLVM_ENABLE_RTTI=ON -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_TARGETS_TO_BUILD="X86"
LLVM_CMAKE_OPTS += -DCMAKE_INSTALL_PREFIX=$(INSTALL_DIR) -DCMAKE_BUILD_TYPE="Release"

LLVM_SRC_DIR=llvm-$(LLVM_VERSION).src
LLVM_OBJ_DIR=llvm-$(LLVM_VERSION)-obj
CLANG_SRC_DIR=$(LLVM_SRC_DIR)/tools/clang

# tar.xz source files for building llvm/clang
LLVM_SRC_TAR=llvm-$(LLVM_VERSION).src.tar.xz
CLANG_SRC_TAR=cfe-$(LLVM_VERSION).src.tar.xz

# urls for tgz files
LLVM_SRC_TAR_URL=http://llvm.org/releases/$(LLVM_VERSION)/$(LLVM_SRC_TAR)
CLANG_SRC_TAR_URL=http://llvm.org/releases/$(LLVM_VERSION)/$(CLANG_SRC_TAR)

all: build-llvm
build-llvm: llvm-$(LLVM_VERSION).build.success

$(LLVM_SRC_TAR):
	wget $(LLVM_SRC_TAR_URL)

$(CLANG_SRC_TAR):
	wget $(CLANG_SRC_TAR_URL)

$(LLVM_SRC_DIR): $(LLVM_SRC_TAR)
	tar -xJf $<
	ln -s $(CURDIR)/../src $@/lib/Transforms/KremlinInstrument
	echo "add_subdirectory(KremlinInstrument)" >> $@/lib/Transforms/CMakeLists.txt
	@touch $@

$(CLANG_SRC_DIR): $(CLANG_SRC_TAR)
	tar -xJf $<
	mv cfe-$(LLVM_VERSION).src $@
	@touch $@

$(LLVM_OBJ_DIR):
	mkdir $@

$(LLVM_OBJ_DIR)/Makefile: $(LLVM_OBJ_DIR) $(LLVM_SRC_DIR) $(CLANG_SRC_DIR)
	cd $< && cmake $(LLVM_CMAKE_OPTS) ../$(LLVM_SRC_DIR) 
	@touch $@

llvm-$(LLVM_VERSION).build.success: $(LLVM_OBJ_DIR)/Makefile
	make -C $(LLVM_OBJ_DIR)
	make -C $(LLVM_OBJ_DIR) install
	@touch $<
	@date > $@

clean:
	@rm -rf $(LLVM_SRC_TAR) $(CLANG_SRC_TAR) $(LLVM_SRC_DIR) $(LLVM_OBJ_DIR) llvm-*.build.success $(INSTALL_DIR)
