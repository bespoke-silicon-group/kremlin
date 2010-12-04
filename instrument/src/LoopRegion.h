#ifndef LOOP_REGION_H
#define LOOP_REGION_H

#include "Region.h"

class LoopRegion : public Region
{
	private:
	Loop* loop;
	RegionId id;

};


#endif // LOOP_REGION_H
