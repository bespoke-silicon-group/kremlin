#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <boost/ptr_container/ptr_set.hpp>
#include <llvm/Instruction.h>
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

    public:
    Timestamp();
    virtual ~Timestamp();

    iterator begin();
    const_iterator begin() const;
    iterator end();
    const_iterator end() const;

    void insert(llvm::Value* base, unsigned int offset);

    size_t size() const;

    private:
    Candidates timestamps;
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
