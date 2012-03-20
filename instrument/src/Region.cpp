#include "Region.h"
#include <sstream>
#include <iomanip>

/**
 * @param that The other region to compare.
 * @return true if the id of this region is less than the other.
 */
bool Region::operator<(const Region& that) const
{
	return getId() < that.getId();
}

/**
 * @param that The other region to compare.
 * @return true if the id of this region is equal than the other.
 */
bool Region::operator==(const Region& that) const
{
	return getId() == that.getId();
}

/**
 * Formats the region id, type, file and line information into a string.
 *
 * @param buf The buffer to place the formatted data.
 * @return The buffer.
 */
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
