#ifndef PASS_LOG_H
#define PASS_LOG_H

#include <string>
#include <llvm/Support/raw_os_ostream.h>
#include <boost/smart_ptr.hpp>

#define LOG_PLAIN() (PassLog::get().plain() << __FILE__ << ":" << __LINE__ << " ")
#define LOG_DEBUG() (PassLog::get().debug() << __FILE__ << ":" << __LINE__ << " ")
#define LOG_INFO()  (PassLog::get().info()  << __FILE__ << ":" << __LINE__ << " ")
#define LOG_WARN()  (PassLog::get().warn()  << __FILE__ << ":" << __LINE__ << " ")
#define LOG_ERROR() (PassLog::get().error() << __FILE__ << ":" << __LINE__ << " ")
#define LOG_FATAL() (PassLog::get().fatal() << __FILE__ << ":" << __LINE__ << " ")

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
	typedef llvm::raw_os_ostream ostream;

	private:
	boost::scoped_ptr<ostream> os;

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
