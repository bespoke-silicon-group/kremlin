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

	const llvm::Type* voidTy();

	const llvm::IntegerType* i64();
	const llvm::IntegerType* i32();
	const llvm::IntegerType* i16();
	const llvm::IntegerType* i8();
	const llvm::IntegerType* i1();

	const llvm::PointerType* pi64();
	const llvm::PointerType* pi32();
	const llvm::PointerType* pi16();
	const llvm::PointerType* pi8();
	const llvm::PointerType* pi1();
};

#endif // LLVM_TYPES_H
