#ifndef PASS_LOG_H
#define PASS_LOG_H

#include <string>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/raw_ostream.h>
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

	public:
	/**
	 * The type of the stream backing the log.
	 */
	typedef llvm::raw_ostream ostream;
	typedef llvm::raw_os_ostream os_ostream;
	typedef llvm::raw_null_ostream nstream;

	private:
	boost::scoped_ptr<os_ostream> os;
	//boost::scoped_ptr<nstream> ns;
	nstream* ns;

	PassLog();

	public:
	virtual ~PassLog();

	ostream& fatal();
	ostream& error();
	ostream& warn();
	ostream& info();
	ostream& debug();
	ostream& plain();

	void close();
};

#endif // PASS_LOG_H

