#ifndef FUNC_CONTEXT_HPP
#define FUNC_CONTEXT_HPP

// TODO: better name (DynamicFunctionRegion)
class FuncContext {
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

	void init(CID cid) { 
		this->table = NULL;
		this->return_register = FuncContext::DUMMY_RETURN_REG;
		this->error_checking_code = FuncContext::ERROR_CHECK_CODE;
		this->call_site_id = cid;
	}

	Reg getCallSiteID() { return this->call_site_id; }
	Reg getReturnRegister() { return this->return_register; }
	Table* getTable() { return this->table; }

	void sanityCheck() {
		assert(error_checking_code == FuncContext::ERROR_CHECK_CODE);
	}

};

#endif // FUNC_CONTEXT_HPP
