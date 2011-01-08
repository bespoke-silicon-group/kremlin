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

	os  << getId() << "\t" 
		<< getRegionType() << "\t"
		<< getFileName() << "\t" 
		<< getFuncName() << "\t" 
		<< getStartLine() << "\t"
		<< getEndLine() << "\t";

	buf = os.str();

	return buf;
}
