ifndef LLVM_RULES_MK
LLVM_RULES_MK = 1

UNROLL_COUNT = 0
UNROLL_THRESH = 150

UNROLL_OPT_STRING = --unroll-allow-partial --unroll-count=$(UNROLL_COUNT) --unroll-threshold=$(UNROLL_THRESH) -debug

MIN_ADB_CHAIN_LEN = 3

ADB_OPT_STRING = --min-chain-length=$(MIN_ADB_CHAIN_LEN)

# ---------------------------------------------------------------------------
# Chained rules
#
# Each call generates a chained rule of the following form:
#
# %.$(PASS_NAME).bc: %.bc $(OTHER_DEPENDENCIES)
#   ...
#
# Parameters:        $(1)   The name of the shared object. If $(1) is empty,
#                           then no load is performed.
#                    $(2)   The command to run the pass.
#                    $(3)   Any additional flags to append to the opt command.
# ---------------------------------------------------------------------------

$(call OPT_PASS_RULE,  ,                            -mem2reg,               EMPTY)
$(call OPT_PASS_RULE,  ,                            -indvars,               EMPTY)
$(call OPT_PASS_RULE,  ,                            -simplifycfg,           EMPTY)
$(call OPT_PASS_RULE,  ,                            -constprop,             EMPTY)
$(call OPT_PASS_RULE,  ,                            -O1,               		EMPTY)
$(call OPT_PASS_RULE,  ,                            -O2,               		EMPTY)
$(call OPT_PASS_RULE,  ,                            -O3,               		EMPTY)
$(call OPT_PASS_RULE,  ,                            -loop-rotate,   		EMPTY)
$(call OPT_PASS_RULE,  ,                            -loop-unroll,   		UNROLL_OPT_STRING)
$(call OPT_PASS_RULE,  KremlinInstrument.so,        -elimsinglephis,        EMPTY)
$(call OPT_PASS_RULE,  KremlinInstrument.so,        -splitbbatfunccall,     EMPTY)
$(call OPT_PASS_RULE,  KremlinInstrument.so,        -assoc-dep-break,       ADB_OPT_STRING)
$(call OPT_PASS_RULE,  KremlinInstrument.so,        -criticalpath,          EMPTY)
$(call OPT_PASS_RULE,  KremlinInstrument.so,        -regioninstrument,      EMPTY)
$(call OPT_PASS_RULE,  KremlinInstrument.so,        -renamemain,      		EMPTY)

# ---------------------------------------------------------------------------
# Required files
# ---------------------------------------------------------------------------
REQUIRED_MAKEFILES := paths.mk llvmGenericRules.mk eliminateImplicitRules.mk
ifdef CHECK_REQUIRED_MAKEFILES_INCLUDED 
    $(call CHECK_REQUIRED_MAKEFILES_INCLUDED, $(REQUIRED_MAKEFILES))
else
    $(error Include checkRequiredFiles.mk first)
endif # CHECK_REQUIRED_MAKEFILES_INCLUDED

endif # LLVM_RULES_MK
