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
    std::string delimiter = "_krem_";
	std::ostringstream os;

	os  << "krem_region"
	    << getId() << delimiter
		<< getRegionType() << delimiter
		<< getFileName() << delimiter
		<< getFuncName() << delimiter
		<< getStartLine() << delimiter
		<< getEndLine() << delimiter;

	buf = os.str();

	return buf;
}
