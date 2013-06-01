#include "LLVMTypes.h"

using namespace llvm;

LLVMTypes::LLVMTypes(LLVMContext& context) :
	context(context)
{
}

/**
 * @return The void type.
 */
Type* LLVMTypes::voidTy()
{
	return Type::getVoidTy(context);
}

IntegerType* LLVMTypes::i64()
{
	return (Type::getInt64Ty(context));
}

IntegerType* LLVMTypes::i32()
{
	return (Type::getInt32Ty(context));
}

IntegerType* LLVMTypes::i16()
{
	return (Type::getInt16Ty(context));
}

IntegerType* LLVMTypes::i8()
{
	return (Type::getInt8Ty(context));
}

IntegerType* LLVMTypes::i1()
{
	return (Type::getInt1Ty(context));
}


PointerType* LLVMTypes::pi64()
{
	return (Type::getInt64PtrTy(context));
}

PointerType* LLVMTypes::pi32()
{
	return (Type::getInt32PtrTy(context));
}

PointerType* LLVMTypes::pi16()
{
	return (Type::getInt16PtrTy(context));
}

PointerType* LLVMTypes::pi8()
{
	return (Type::getInt8PtrTy(context));
}

PointerType* LLVMTypes::pi1()
{
	return (Type::getInt1PtrTy(context));
}
