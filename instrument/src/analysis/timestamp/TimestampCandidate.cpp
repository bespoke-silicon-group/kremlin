#include "analysis/timestamp/TimestampCandidate.h"

using namespace llvm;

/**
 * Constructs a new timestamp candidate. The candidate's timestamp is the
 * timestamp of the base value plus the constant offset.
 *
 * @param base The value associated with the base timestamp.
 * @param offset The constant offset from the base timestamp.
 */
TimestampCandidate::TimestampCandidate(
    llvm::Value* base, unsigned int offset) :
    _baseVal(base),
    _timeOffset(offset)
{
}

TimestampCandidate::~TimestampCandidate()
{
}

/**
 * Returns the base.
 */
Value* TimestampCandidate::getBase() const
{
    return _baseVal;
}

/**
 * Returns the offset.
 */
unsigned int TimestampCandidate::getOffset() const
{
    return _timeOffset;
}

/**
 * Returns true if the bases are the same.
 */
bool operator==(const TimestampCandidate& t1, const TimestampCandidate& t2)
{
    return t1.getBase() == t2.getBase();
}

/**
 * Returns true if the first base is less than the second.
 */
bool operator<(const TimestampCandidate& t1, const TimestampCandidate& t2)
{
    return t1.getBase() < t2.getBase();
}
