#include "analysis/timestamp/Timestamp.h"

using namespace llvm;

/**
 * Constructs a timestamp with no candidates.
 */
Timestamp::Timestamp()
{
}

Timestamp::~Timestamp()
{
}

/**
 * @return An iterator to the first element.
 */
Timestamp::iterator Timestamp::begin()
{
    return timestamps.begin();
}

/**
 * @return An iterator to the first element.
 */
Timestamp::const_iterator Timestamp::begin() const
{
    return timestamps.begin();
}

/**
 * @return An iterator one past the last element.
 */
Timestamp::iterator Timestamp::end()
{
    return timestamps.end();
}

/**
 * @return An iterator one past the last element.
 */
Timestamp::const_iterator Timestamp::end() const
{
    return timestamps.end();
}

/**
 * Adds a potential candidate timestamp. The candidate is the timestamp of
 * the base plus the constant offset. If the base timestamp already existed,
 * only the one with the greatest offset will exist.
 *
 * @param base The LLVM Value the base timestamp is calculated from.
 * @param offset The constant amount of work from the base value.
 */
void Timestamp::insert(llvm::Value* base, unsigned int offset)
{
    // Only insert of the one to be inserted dominates the one that exists.
    TimestampCandidate cand(base, offset);
    Candidates::iterator it = timestamps.find(cand);
    if(it != timestamps.end())
    {
        // Do nothing if the existing ts dominates the one to insert.
        if(it->getOffset() >= offset)
            return;
        else
            timestamps.erase(cand);
    }
    timestamps.insert(new TimestampCandidate(base, offset));
}

/**
 * @return the number of candidates.
 */
size_t Timestamp::size() const
{
    return timestamps.size();
}

/**
 * Returns if the first timestamp is less than the second? This is just here
 * to make timestamps comparable for the boost::ptr_set.
 */
bool operator<(const Timestamp& t1, const Timestamp& t2)
{
    Timestamp::Candidates::const_iterator i1 = t1.begin();
    Timestamp::Candidates::const_iterator i2 = t2.begin();
    while(i1 != t1.end() && i2 != t2.end())
    {
        if(*i1 < *i2)
            return true;
        i1++;
        i2++;
    }
    if(i1 == t1.end() && i2 != t2.end())
        return true;
    return false;
}
