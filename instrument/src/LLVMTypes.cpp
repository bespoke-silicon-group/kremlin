#include "LLVMTypes.h"

using namespace llvm;

LLVMTypes::LLVMTypes(LLVMContext& context) :
	context(context)
{
}

/**
 * @return The void type.
 */
const Type* LLVMTypes::voidTy()
{
	return Type::getVoidTy(context);
}

const IntegerType* LLVMTypes::i64()
{
	return (Type::getInt64Ty(context));
}

const IntegerType* LLVMTypes::i32()
{
	return (Type::getInt32Ty(context));
}

const IntegerType* LLVMTypes::i16()
{
	return (Type::getInt16Ty(context));
}

const IntegerType* LLVMTypes::i8()
{
	return (Type::getInt8Ty(context));
}

const IntegerType* LLVMTypes::i1()
{
	return (Type::getInt1Ty(context));
}


const PointerType* LLVMTypes::pi64()
{
	return (Type::getInt64PtrTy(context));
}

const PointerType* LLVMTypes::pi32()
{
	return (Type::getInt32PtrTy(context));
}

const PointerType* LLVMTypes::pi16()
{
	return (Type::getInt16PtrTy(context));
}

const PointerType* LLVMTypes::pi8()
{
	return (Type::getInt8PtrTy(context));
}

const PointerType* LLVMTypes::pi1()
{
	return (Type::getInt1PtrTy(context));
}
