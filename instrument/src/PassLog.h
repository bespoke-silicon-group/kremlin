#ifndef PASS_LOG_H
#define PASS_LOG_H

#include <string>
#include <llvm/Support/raw_ostream.h>
#include <boost/smart_ptr.hpp>

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
	typedef llvm::raw_fd_ostream ostream;

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

	void close();
};

#endif // PASS_LOG_H

