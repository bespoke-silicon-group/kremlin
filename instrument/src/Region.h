#ifndef REGION_H
#define REGION_H

#include <stdint.h>
#include <string>

typedef uint64_t RegionId;

class Region
{
	public:
	typedef enum {
		REGION_TYPE_FUNC,
		REGION_TYPE_LOOP,
		REGION_TYPE_LOOP_BODY
	} RegionType;

	public:
	virtual ~Region() {}

	virtual RegionId getId() const = 0;
	virtual const std::string& getRegionType() const = 0;
	virtual const std::string& getFileName() const = 0;
	virtual const std::string& getFuncName() const = 0;
	virtual unsigned int getEndLine() const = 0;
	virtual unsigned int getStartLine() const = 0;

	virtual std::string& formatToString(std::string& buf) const;

	bool operator<(const Region& that) const;
	bool operator==(const Region& that) const;
};

#endif // REGION_H
