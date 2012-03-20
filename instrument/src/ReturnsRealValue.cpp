#include "ReturnsRealValue.h"

using namespace llvm;

/**
 * @param ret_type The return type of the operation.
 * @return true if the type is a real value.
 */
bool ReturnsRealValue::operator()(const llvm::Type& ret_type) 
{
    if(ret_type.getTypeID() != Type::VoidTyID && ret_type.getTypeID() != Type::PointerTyID)
        return true;
    else
        return false;
}

/**
 * @param ci The call to analyze.
 * @return true if the type is a real value.
 */
bool ReturnsRealValue::operator()(llvm::CallInst& ci) 
{
    return (*this)(*ci.getType());
}

/**
 * @param func The function to analyze.
 * @return true if the type is a real value.
 */
bool ReturnsRealValue::operator()(llvm::Function& func) 
{
    return (*this)(*func.getReturnType());
}

/**
 * @param ii The call to analyze.
 * @return true if the type is a real value.
 */
bool ReturnsRealValue::operator()(llvm::InvokeInst& ii) 
{
    return (*this)(*ii.getType());
}

