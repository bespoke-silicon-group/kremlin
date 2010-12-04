#ifndef UNARY_FUNCTION_CONST_H
#define UNARY_FUNCTION_CONST_H

template <typename ArgType, typename ResultType>
struct UnaryFunctionConst : public std::unary_function<ArgType, ResultType>
{
	virtual ResultType operator()(ArgType) const = 0;
	virtual ~UnaryFunctionConst() {};
};

#endif // UNARY_FUNCTION_CONST
