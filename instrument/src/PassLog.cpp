#include "PassLog.h"
#include <iostream>
#include <llvm/Support/CommandLine.h>

using namespace llvm;

static cl::opt<std::string> log_name("log-name",cl::desc("Where to log to."),cl::value_desc("filename"),cl::init("pass.log"));

PassLog* PassLog::singleton;

PassLog::PassLog()
{
	std::string error_message;

	// Allocate the output stream.
	os.reset(new ostream(log_name.c_str(), error_message));

	// Check for an error.
	if(os->has_error())
		std::cerr << "Failed to open log " << log_name << ": " << error_message << std::endl;

	// Logs should be unbuffered.
	os->SetUnbuffered();
}

PassLog::~PassLog()
{
}

/**
 * Prints a fatal message.
 * @return The stream to print fatal messages to.
 */
PassLog::ostream& PassLog::fatal()
{
	*os << "FATAL: ";
	return *os;
}

/**
 * Prints an error message.
 * @return The stream to print error messages to.
 */
PassLog::ostream& PassLog::error()
{
	*os << "ERROR: ";
	return *os;
}

/**
 * Prints a warning message.
 * @return The stream to print warning messages to.
 */
PassLog::ostream& PassLog::warn()
{
	*os << "WARNING: ";
	return *os;
}

/**
 * Prints a info message.
 * @return The stream to print info messages to.
 */
PassLog::ostream& PassLog::info()
{
	*os << "INFO: ";
	return *os;
}

/**
 * Prints a debug message.
 * @return The stream to print debug messages to.
 */
PassLog::ostream& PassLog::debug()
{
	*os << "DEBUG: ";
	return *os;
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
	os->close();
	os.reset();
	delete singleton;
	singleton = NULL;
}
