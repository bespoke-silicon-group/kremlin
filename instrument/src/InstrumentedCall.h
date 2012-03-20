#ifndef INSTRUMENTED_CALL
#define INSTRUMENTED_CALL

#include "FormattableToString.h"
#include "InstrumentationCall.h"

template <typename Callable>
class InstrumentedCall : public FormattableToString, public InstrumentationCall
{
    public:
    /// The Id type
    typedef unsigned long long Id;

    public:
    static llvm::Function* untangleCall(Callable* ci);

    InstrumentedCall(Callable* ci, uint64_t bb_call_idx);
    virtual ~InstrumentedCall();

    virtual std::string& formatToString(std::string& buf) const;
    virtual void instrument();

    Id getId() const;

    private:
    Callable* ci;
    Id id;

};

#include "InstrumentedCall.tcc"

#endif // INSTRUMENTED_CALL
