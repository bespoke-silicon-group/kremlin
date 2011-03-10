# The output file that contains whether a test is correct for a particular
# test.
MAKE_RESULT_FILENAME = make.out

# The result file
TEST_RESULTS = testResults.txt

# Always re-run tests.
.PHONY: testResults.txt $(MAKE_RESULTS) $(CLEAN_RESULTS)

# Summarizes all the test results into one.
$(TEST_RESULTS): $(MAKE_RESULTS)
	-grep 'TEST_RESULT: FAILED' $(MAKE_RESULTS) > $@
	echo "`cat $@ | wc -l` of $(words $(MAKE_RESULTS)) tests failed" >> $@
	clear
	@cat $@

# How to make all tests.
$(MAKE_RESULTS):
	(make -C $(dir $@) $(CLEAN_TASK) $(BUILD_TASK) $(RUN_TASK) $(CHECK_TASK) || echo "TEST_RESULT: FAILED") &> $@

clean:: $(CLEAN_RESULTS)
	-$(RM) $(MAKE_RESULTS)
