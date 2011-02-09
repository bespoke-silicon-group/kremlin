#include "Region.h"
#include <sstream>

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

	os  << PREFIX
	    << getId() << DELIMITER
		<< getRegionType() << DELIMITER
		<< getFileName() << DELIMITER
		<< getFuncName() << DELIMITER
		<< getStartLine() << DELIMITER
		<< getEndLine() << DELIMITER;

	buf = os.str();

	return buf;
}
