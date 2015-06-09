#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <boost/ptr_container/ptr_set.hpp>
#include <llvm/IR/Instruction.h>
#include "TimestampCandidate.h"
#include <foreach.h>

/**
 * Represents the availability time of an operation.
 */
class Timestamp
{
    public:
    /// The timestamp candidates.
    typedef boost::ptr_set<TimestampCandidate> Candidates;

    /// The timestamp candidates iterator.
    typedef Candidates::iterator iterator;

    /// The timestamp candidates iterator.
    typedef Candidates::const_iterator const_iterator;

    Timestamp();
    virtual ~Timestamp();

    iterator begin();
    const_iterator begin() const;
    iterator end();
    const_iterator end() const;

    void addCandidate(llvm::Value* base_val, unsigned int time_offset);

    size_t size() const;

    private:
    Candidates _timestamp_candidates;
};

bool operator<(const Timestamp& t1, const Timestamp& t2);

template <typename Ostream>
Ostream& operator<<(Ostream& os, const Timestamp& ts)
{
    foreach(const TimestampCandidate& cand, ts)
        os << cand << " ";
    return os;
}

#endif // TIMESTAMP_H
