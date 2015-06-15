#include "PassLog.h"
//#include <iostream>
#include "llvm/Support/CommandLine.h"

using namespace llvm;

static cl::opt<std::string> log_name("log-name",cl::desc("Where to log to."),cl::value_desc("filename"),cl::init("pass.log"));

PassLog* PassLog::singleton;

PassLog::PassLog() {}

PassLog::~PassLog() {}

/**
 * Prints a fatal message.
 * @return The stream to print fatal messages to.
 */
raw_ostream& PassLog::fatal()
{
	outs() << "FATAL: ";
	return outs();
}

/**
 * Prints an error message.
 * @return The stream to print error messages to.
 */
raw_ostream& PassLog::error()
{
	outs() << "ERROR: ";
	return outs();
}

/**
 * Prints a warning message.
 * @return The stream to print warning messages to.
 */
raw_ostream& PassLog::warn()
{
	outs() << "WARNING: ";
	return outs();
}

/**
 * Prints a info message.
 * @return The stream to print info messages to.
 */
raw_ostream& PassLog::info()
{
	outs() << "INFO: ";
	return outs();
}

/**
 * Prints a debug message.
 * @return The stream to print debug messages to.
 */
raw_ostream& PassLog::debug()
{
	outs() << "DEBUG: ";
	return outs();
}

/**
 * Prints a non-decorated message.
 * @return A stream to print undecorated messages to.
 */
raw_ostream& PassLog::plain()
{
	return outs();
}

/**
 * Returns an instance of this class.
 * @return an instance of this class.
 */
PassLog& PassLog::get()
{
	return singleton ? *singleton : *(singleton = new PassLog());
}

/**
 * Closes the log and destroys the object.
 */
void PassLog::close()
{
	delete singleton;
	singleton = NULL;
}
