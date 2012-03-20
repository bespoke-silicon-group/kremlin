#ifndef REGION_H
#define REGION_H

#include <stdint.h>
#include <string>
#include "FormattableToString.h"

/// The id of a region.
typedef uint64_t RegionId;

/**
 * A region that represents a level to instrument.
 */
class Region : public FormattableToString
{
	public:

    /// The types of regions.
	typedef enum 
    {
		REGION_TYPE_FUNC,
		REGION_TYPE_LOOP,
		REGION_TYPE_LOOP_BODY
	} RegionType;

	public:
	virtual ~Region() {}

    /// @return The id of the region.
	virtual RegionId getId() const = 0;

    /// @return The type of the region.
	virtual const std::string& getRegionType() const = 0;

    /// @return The filename containing the region.
	virtual const std::string& getFileName() const = 0;

    /// @return The function name of the region.
	virtual const std::string& getFuncName() const = 0;

    /// @return The end line in the source code.
	virtual unsigned int getEndLine() const = 0;

    /// @return the start line in the source code.
	virtual unsigned int getStartLine() const = 0;

	virtual std::string& formatToString(std::string& buf) const;

	bool operator<(const Region& that) const;
	bool operator==(const Region& that) const;
};

#endif // REGION_H
