#include "Region.h"
#include <sstream>
#include <iomanip>

bool Region::operator<(const Region& that) const
{
	return getId() < that.getId();
}

bool Region::operator==(const Region& that) const
{
	return getId() == that.getId();
}

std::string& Region::formatToString(std::string& buf) const
{
	std::ostringstream os;
    os.fill('0');

	os  << PREFIX
	    << std::setw(16) << std::hex << getId() << std::dec << DELIMITER
		<< getRegionType() << DELIMITER
		<< getFileName() << DELIMITER
		<< getFuncName() << DELIMITER
		<< getStartLine() << DELIMITER
		<< getEndLine() << DELIMITER;

	buf = os.str();

	return buf;
}
