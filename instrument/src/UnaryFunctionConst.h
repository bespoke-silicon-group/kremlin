#ifndef UNARY_FUNCTION_CONST_H
#define UNARY_FUNCTION_CONST_H

#include <functional>

/**
 * Base class for const unary function objects.
 * @tparam ArgType      The type of the argument of the unary function.
 * @tparam ResultType   The result type of the unary function.
 */
template <typename ArgType, typename ResultType>
struct UnaryFunctionConst : public std::unary_function<ArgType, ResultType>
{
    /**
     * An unary function.
     *
     * @param arg The argument.
     */
	virtual ResultType operator()(ArgType arg) const = 0;
	virtual ~UnaryFunctionConst() {};
};

#endif // UNARY_FUNCTION_CONST
