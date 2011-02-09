#ifndef FORMATTABLE_TO_STRING
#define FORMATTABLE_TO_STRING

#include <string>

class FormattableToString
{
    public:
    static const std::string DELIMITER;
    static const std::string PREFIX;

    virtual ~FormattableToString();
    virtual std::string& formatToString(std::string& buf) const = 0;
};

#endif // FORMATTABLE_TO_STRING

