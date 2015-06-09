#include "MemoryInstHelper.h"
#include <llvm/IR/DerivedTypes.h>

using namespace llvm;

unsigned MemoryInstHelper::getTypeSizeInBytes(Value *val)
{
	const Type* val_type = val->getType();

	unsigned type_size_in_bytes = 4; // default to 4 bytes (32 bit)
	
	if(val_type->isIntegerTy()) {
		const IntegerType* ity = cast<IntegerType>(val_type);

		unsigned bitwidth = ity->getBitWidth();
		type_size_in_bytes = bitwidth / 8;

		// We don't expect to have any non whole-byte integers so print a
		// warning if we see one. Also, we round up to the nearest byte to be
		// safe.
		if(bitwidth % 8 != 0) {
			LOG_WARN() << "non-standard integer width: " << bitwidth << "\n";
			type_size_in_bytes++; // round up the nearest byte
		}
	}
	else if(val_type->isFloatTy()) { 
		type_size_in_bytes = 4;
	}
	else if(val_type->isDoubleTy()) { 
		type_size_in_bytes = 8;
	}
	else if(val_type->isPointerTy()) {
		type_size_in_bytes = 8; // FIXME: this assumes 64-bit arch
	}
	else {
		LOG_DEBUG() << "unknown type: " << *val << "\n";
	}

	return type_size_in_bytes;
}
