ifndef GENERIC_TEST_MK

GENERIC_TEST_MK = 1

# Function:          GENERIC_TEST
# Description:       This will generate a rule "$(1)Test" that will run the
#                    test in a particular test directory.
#
#                    This will also generate a rule "$(1)TestAll" that will
#                    run the test for all test directories and summarize the
#                    results.
# 
# Parameters:        $(1)   The test prefix.
#                    $(2)   The dependencies of the test.
#                    $(3)   The commands to run the test.
# Return:            Nothing.
GENERIC_TEST = $(eval $(call GENERIC_TEST_TEXT,$1,$2,$3))

# Function:          GENERIC_TEST_TEXT
# Description:       Returns rules for running a test.
#
#                    This will generate a rule "$(1)Test" that will run the
#                    test in a particular test directory.
#
#                    This will also generate a rule "$(1)TestAll" that will
#                    run the test for all test directories and summarize the
#                    results.
# 
# Parameters:        $(1)   The test prefix.
#                    $(2)   The dependencies of the test.
#                    $(3)   The commands to run the test.
# Return:            The makefile text that creates the rule.
define GENERIC_TEST_TEXT

# The test prefix.
$(eval TEST_PREFIX := $$(strip $1))

# The name of the test.
$(eval TEST_RULE := $(TEST_PREFIX))

# The dependencies of the test.
$(eval TEST_DEPENDENCIES := $$(2))

# The commands to run.
$(eval TEST_COMMANDS := $$(3))

# The name of the run-all rule.
$(eval TEST_ALL_RULE := $(TEST_RULE)All)

# The output files from running them all.
$(eval TEST_OUTPUT := $(TEST_PREFIX).out)

# The output of all the tests.
$(eval OUTPUTS = $$(addsuffix /$(TEST_OUTPUT), $(TEST_DIRECTORIES)))

$$(info defining rule for $(TEST_RULE))

$(TEST_RESULTS): $(MAKE_RESULTS)
	-grep 'FAILED' $(MAKE_RESULTS) > $@
	echo "`cat $@ | wc -l` of $(words $(MAKE_RESULTS)) tests failed" >> $@
	clear
	@cat $@

# Mark the test as phony so we'll always run it.
.PHONY: $(TEST_RULE) $(TEST_ALL_RULE) $(OUTPUTS)

# Define a rule to run the test
$(TEST_RULE): $(TEST_DEPENDENCIES)
	$(TEST_COMMANDS)

# The rule to make all tests.
$(TEST_ALL_RULE): $(OUTPUTS)
	-@grep 'FAILED' $(OUTPUTS) > $$@
	@echo "`cat $$@ | wc -l` of $(words $(OUTPUTS)) failed" >> $$@
	@clear
	@cat $$@
	
# How to make all the tests.
$(OUTPUTS):
	(make -C $$(dir $$@) $(CLEAN_TASK) $(TEST_RULE) || echo "test FAILED") &> $$@

$(CLEAN_TASK)::
	-@rm $(OUTPUTS)

endef

endif # GENERIC_TEST_MK
