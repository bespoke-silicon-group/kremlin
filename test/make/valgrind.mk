# ---------------------------------------------------------------------------
# Defines tests to build and test reference files. These are the assumed
# correct output files.
#
# The following build rules are added:
# ------------------------------------
# make valgrindBuildTest
# make valgrindCheckTest
#
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Defines
# ---------------------------------------------------------------------------
RPAREN = )
LPAREN = (

# The file produced by the check.
VALGRIND_OUT = valgrind.out

# Strings to find
VALGRIND_DEFINITE_NO_LEAK = definitely lost: 0 bytes in 0 blocks
VALGRIND_INDIRECT_LEAK = indirectly lost: 0 bytes in 0 blocks
VALGRIND_POSSIBLE_LEAK = possibly lost: 0 bytes in 0 blocks

# The list of variables containing strings to find
VALGRIND_MUST_FIND_NAMES = VALGRIND_DEFINITE_NO_LEAK \
						   VALGRIND_INDIRECT_LEAK \
						   VALGRIND_POSSIBLE_LEAK \

# Valgrind error strings
VALGRIND_UNINITIALIZED_USE = Use of uninitialised value of size
VALGRIND_BRANCH_ON_UNINITIALIZED = Conditional jump or move depends on uninitialised value(s)


# The list of error string variables
VALGRIND_ERROR_NAMES = VALGRIND_UNINITIALIZED_USE \
					   VALGRIND_BRANCH_ON_UNINITIALIZED \

# The error strings grouped into a grep string
# 
# Convert white spaces between variable names to a single space.
# Convert single spaces to )$(
# Wrap with $( and )
# Evaluate expression as make syntax.
$(eval VALGRIND_ERROR_GREP = '$$$(LPAREN)$(shell echo $(strip $(VALGRIND_ERROR_NAMES)) | sed 's/ /$(RPAREN)\\|$$$(LPAREN)/g')$(RPAREN)')

# The dependencies the test needs.
VALGRIND_DEPENDENCIES := $(VALGRIND_OUT) $(VALGRIND_MUST_FIND_NAMES)

# ---------------------------------------------------------------------------
# Functions (alpha order)
# ---------------------------------------------------------------------------

# Function:          VALGRIND_DEFINE_FIND_NAME
# Description:       Defines a rule for checking that a particular string
#                    exists in the valgrind output.
# 
# Parameters:        $(1)   The name of the variable containing the text to
#                           grep for. The variable name will also be the name
#                           of the rule.
# Return:            The makefile text that creates the rule.
define VALGRIND_DEFINE_FIND_NAME
$(eval RULE_NAME := $$(strip $(1)))

$(RULE_NAME): $(VALGRIND_OUT)
	grep "$(value $(RULE_NAME))" $(VALGRIND_OUT) &>/dev/null

endef # VALGRIND_DEFINE_FIND_NAME

# How to run the test
define VALGRIND_CHECK_COMMANDS
	@if (grep $(VALGRIND_ERROR_GREP) $(VALGRIND_OUT) > /dev/null); \
		then \
			echo "valgrind check FAILED"; \
	    else  \
			echo "valgrind check PASSED"; \
		fi
endef # VALGRIND_CHECK_COMMANDS

# ---------------------------------------------------------------------------
#  Rules (alpha order)
# ---------------------------------------------------------------------------

# The temporary file should be deleted.
.INTERMEDIATE: $(VALGRIND_OUT)

# Builds a valgrind output file.
$(VALGRIND_BUILD_TASK): $(TARGET)
	@echo "TODO: implement valgrind build task"

$(VALGRIND_OUT): $(RUN_TASK)
	valgrind --leak-check=full $(RUN_COMMAND) 2>&1 | tee $(VALGRIND_OUT)

# Define rules to find all the necessary strings.
$(foreach VALGRIND_MUST_FIND,$(VALGRIND_MUST_FIND_NAMES),$(eval $(call VALGRIND_DEFINE_FIND_NAME,$(VALGRIND_MUST_FIND))))

# Create the rule to check.
$(call GENERIC_TEST, valgrindCheck, $(VALGRIND_DEPENDENCIES), $(VALGRIND_CHECK_COMMANDS))
