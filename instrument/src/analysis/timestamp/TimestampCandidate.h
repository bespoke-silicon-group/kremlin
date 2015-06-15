#ifndef TIMESTAMP_CANDIDATE_H
#define TIMESTAMP_CANDIDATE_H

#include <llvm/IR/Instruction.h>

class TimestampCandidate
{
    public:
    TimestampCandidate(llvm::Value* base, unsigned int offset);
    virtual ~TimestampCandidate();

    llvm::Value* getBase() const;
    unsigned int getOffset() const;

    private:
    llvm::Value* _baseVal;
    unsigned int _timeOffset;
};

bool operator<(const TimestampCandidate& t1, const TimestampCandidate& t2);
bool operator==(const TimestampCandidate& t1, const TimestampCandidate& t2);

template <typename Ostream>
Ostream& operator<<(Ostream& os, const TimestampCandidate& cand)
{
    os << "(" << *cand.getBase() << "," << cand.getOffset() << ")";
    return os;
}

#endif // TIMESTAMP_CANDIDATE_H
