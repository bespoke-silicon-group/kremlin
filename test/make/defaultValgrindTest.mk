RPAREN = )
LPAREN = (

# The file produced by the check.
VALGRIND_CHECK_OUT = valgrind-check.out

# Valgrind error strings
VALGRIND_UNINITIALIZED_USE = Use of uninitialised value of size
VALGRIND_BRANCH_ON_UNINITIALIZED = Conditional jump or move depends on uninitialised value(s)

# The list of error string variables
VALGRIND_ERROR_NAMES = VALGRIND_UNINITIALIZED_USE \
					   VALGRIND_BRANCH_ON_UNINITIALIZED

# The error strings grouped into a grep string
$(eval VALGRIND_ERROR_GREP = '$$$(LPAREN)$(shell echo $(strip $(VALGRIND_ERROR_NAMES)) | sed 's/ /$(RPAREN)\\|$$$(LPAREN)/g')$(RPAREN)')

# ---------------------------------------------------------------------------
#  Rules (alpha order)
# ---------------------------------------------------------------------------

# Builds a valgrind output file.
$(VALGRIND_BUILD_TASK): $(TARGET)
	@echo "TODO: implement valgrind build task"

# Checks the valgrind output file.
$(VALGRIND_CHECK_TASK): $(TARGET)
	valgrind --leak-check=full $(RUN_COMMAND) &> $(VALGRIND_CHECK_OUT)
	@(if grep $(VALGRIND_ERROR_GREP) $(VALGRIND_CHECK_OUT); \
		then echo "valgrind check FAILED"; \
	    else echo "valgrind check PASSED"; \
	fi) \
