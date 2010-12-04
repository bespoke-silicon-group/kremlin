include buildTasks.mk

# How many concurrent make targets can be done at once.
PARALLEL_MAKE = 4

# The result file
TEST_RESULTS = testResults.txt

# All the directories to run tests for.
# They're expected to have their own makefile that can clean, build and run
# the test.
#
# Looks for any directory with a makefile in it. This is considered to be a
# test directory.
# mindepth 2 to ignore the makefile in the current directory
TEST_DIRECTORIES = $(dir $(shell find . -mindepth 2 -name Makefile))

# The output file that contains whether a test is correct for a particular
# test.
MAKE_RESULT_FILENAME = make.out

# The output from make for a particular test
MAKE_RESULTS = $(addsuffix $(MAKE_RESULT_FILENAME), $(TEST_DIRECTORIES))

# Rule to clean the directories
CLEAN_RESULTS = $(addsuffix clean, $(TEST_DIRECTORIES))

# Always re-run tests.
.PHONY: testResults.txt $(MAKE_RESULTS) $(CLEAN_RESULTS)

parallel:
	make -j $(PARALLEL_MAKE) $(TEST_RESULTS)

# Summarizes all the test results into one.
$(TEST_RESULTS): $(MAKE_RESULTS)
	grep 'TEST_RESULT: FAILED' $(MAKE_RESULTS) > $@
	echo 'ran $(words $(MAKE_RESULTS)) tests' >> $@
	clear
	@cat $@

# How to make all tests.
$(MAKE_RESULTS):
	@echo "Making: $@ in dir $(dir $@)"
	(make -C $(dir $@) $(CLEAN_TASK) $(BUILD_TASK) $(RUN_TASK) $(CHECK_TASK) || echo "TEST_RESULT: FAILED") > $@

$(CLEAN_RESULTS):
	make -C $(dir $@) clean

clean:: $(CLEAN_RESULTS)
	-$(RM) $(MAKE_RESULTS)
