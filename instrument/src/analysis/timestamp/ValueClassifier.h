#ifndef VALUE_CLASSIFIER_H
#define VALUE_CLASSIFIER_H

#include <llvm/IR/Value.h>
#include "analysis/ReductionVars.h"

class ValueClassifier
{
    public:

    /**
     * The classifications of timestamps.
     */
    enum Class
    {
        /// Reduction variables.
        REDUCTION_VAR,

        /// Induction variables.
        INDUCTION_VAR,

        /// Constants (always available)
        CONSTANT,

        /// Operations that do a constant amount of work from their operands.
        CONSTANT_WORK_OP,

        /// Timestamps that cannot be calculated or partially evaluated statically.
        LIVE_IN
    };

    public:
    ValueClassifier(ReductionVars& rv);
    virtual ~ValueClassifier() {}

    Class operator()(llvm::Value* val) const;

    private:
    ReductionVars& rv;
};

#endif // VALUE_CLASSIFIER_H
