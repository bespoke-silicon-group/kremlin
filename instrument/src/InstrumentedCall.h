#ifndef INSTRUMENTED_CALL
#define INSTRUMENTED_CALL

#include "FormattableToString.h"
#include "InstrumentationCall.h"

template <typename Callable>
class InstrumentedCall : public FormattableToString, public InstrumentationCall
{
    public:
    typedef unsigned long long Id;

    private:
    Callable* ci;
    Id id;

    public:
    static llvm::Function* untangleCall(Callable* ci);

    InstrumentedCall(Callable* ci);
    virtual ~InstrumentedCall();

    virtual std::string& formatToString(std::string& buf) const;
    virtual void instrument();

    Id getId() const;
};

#include "InstrumentedCall.tcc"

#endif // INSTRUMENTED_CALL
