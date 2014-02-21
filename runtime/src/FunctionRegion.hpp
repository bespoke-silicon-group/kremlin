#ifndef FUNCTION_REGION_HPP
#define FUNCTION_REGION_HPP

#include "MemMapAllocator.h"
#include "Table.h" // for delete of table in destructor

class FunctionRegion {
private:
	static const int DUMMY_RETURN_REG = -1;
	static const UInt32 ERROR_CHECK_CODE = 0xDEADBEEF; // XXX: debug only?
	Reg return_register;
	CID call_site_id;
	UInt32 error_checking_code;

public:
	Table* table; // TODO: make this private

	void setReturnRegister(Reg r) { 
		// TODO: error checking?
		this->return_register = r; 
	}

	FunctionRegion(CID callsite_id) { 
		this->table = NULL;
		this->return_register = FunctionRegion::DUMMY_RETURN_REG;
		this->error_checking_code = FunctionRegion::ERROR_CHECK_CODE;
		this->call_site_id = callsite_id;
	}

	~FunctionRegion() {
		assert(this->table != NULL);
		delete this->table;
		this->table = NULL;
	}

	CID getCallSiteID() { return this->call_site_id; }
	Reg getReturnRegister() { return this->return_register; }
	Table* getTable() { return this->table; }

	void sanityCheck() {
		assert(error_checking_code == FunctionRegion::ERROR_CHECK_CODE);
	}

	static void* operator new(size_t size) {
		return (FunctionRegion*)MemPoolAllocSmall(sizeof(FunctionRegion));
	}

	static void operator delete(void* ptr) {
		MemPoolFreeSmall(ptr, sizeof(FunctionRegion));
	}
};

#endif // FUNCTION_REGION_HPP
