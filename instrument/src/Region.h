#ifndef REGION_H
#define REGION_H

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
};

#endif // REGION_H
