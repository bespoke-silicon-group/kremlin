#ifndef LLVM_TYPES_H
#define LLVM_TYPES_H

#include <llvm/LLVMContext.h>
#include <llvm/Type.h>

/**
 * Holds references to llvm types.
 */
struct LLVMTypes
{
	llvm::LLVMContext& context;

	LLVMTypes(llvm::LLVMContext& context);

	llvm::Type* voidTy();

	llvm::IntegerType* i64();
	llvm::IntegerType* i32();
	llvm::IntegerType* i16();
	llvm::IntegerType* i8();
	llvm::IntegerType* i1();

	llvm::PointerType* pi64();
	llvm::PointerType* pi32();
	llvm::PointerType* pi16();
	llvm::PointerType* pi8();
	llvm::PointerType* pi1();
};

#endif // LLVM_TYPES_H
