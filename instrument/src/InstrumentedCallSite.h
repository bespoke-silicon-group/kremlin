#ifndef INSTRUMENTED_CALL_SITE
#define INSTRUMENTED_CALL_SITE

class InstrumentedCallSite
{
    public:
    typedef unsigned long long Id;

    private:

    public:
    InstrumentedCallSite();

    std::string& formatToString(std::string& buf);
};

#endif // INSTRUMENTED_CALL_SITE
