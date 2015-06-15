#ifndef PASS_LOG_H
#define PASS_LOG_H

#include <string>
#include "llvm/Support/raw_ostream.h"
#include <boost/smart_ptr.hpp>

#define LOG_PLAIN() (PassLog::get().plain() << __FILE__ << ":" << __LINE__ << " ")
#define LOG_DEBUG() (PassLog::get().debug() << __FILE__ << ":" << __LINE__ << " ")
#define LOG_INFO()  (PassLog::get().info()  << __FILE__ << ":" << __LINE__ << " ")
#define LOG_WARN()  (PassLog::get().warn()  << __FILE__ << ":" << __LINE__ << " ")
#define LOG_ERROR() (PassLog::get().error() << __FILE__ << ":" << __LINE__ << " ")
#define LOG_FATAL() (PassLog::get().fatal() << __FILE__ << ":" << __LINE__ << " ")

// llvm's Value->print is very costly...
#ifdef FULL_PRINT
#define PRINT_VALUE(value) (value)
#else
#define PRINT_VALUE(value) (" NOPRINT\n")
#endif

/**
 * Logging class for LLVM Passes
 */
class PassLog
{
	private:
	/**
	 * Singleton instance.
	 */
	static PassLog* singleton;
	
	public:
	static PassLog& get();

	private:
	PassLog();

	public:
	virtual ~PassLog();

	llvm::raw_ostream& fatal();
	llvm::raw_ostream& error();
	llvm::raw_ostream& warn();
	llvm::raw_ostream& info();
	llvm::raw_ostream& debug();
	llvm::raw_ostream& plain();

	void close();
};

#endif // PASS_LOG_H

