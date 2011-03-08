# The name of the build output.
REFERENCE_BUILD_OUTPUT = referenceBuild.out

# The name of the check output.
REFERENCE_CHECK_OUTPUT = referenceCheck.out

# The build summary.
REFERENCE_BUILD_RESULT = referenceBuild.out

# The check summary.
REFERENCE_CHECK_RESULT = referenceCheck.out

# All the check output.
REFERENCE_CHECK_OUTPUTS = $(addsuffix $(REFERENCE_CHECK_OUTPUT), $(TEST_DIRECTORIES))

# All the build output.
REFERENCE_BUILD_OUTPUTS = $(addsuffix $(REFERENCE_BUILD_OUTPUT), $(TEST_DIRECTORIES))

# ---------------------------------------------------------------------------
#  Rules (alpha order)
# ---------------------------------------------------------------------------

.PHONY: $(REFERENCE_BUILD_OUTPUTS) $(REFERENCE_CHECK_RESULT) $(REFERENCE_CHECK_OUTPUTS) $(REFERENCE_BUILD_RESULT)

# Checks a run against the reference file.
$(REFERENCE_CHECK_OUTPUTS):
	(make -C $(dir $@) $(CLEAN_TASK) $(BUILD_TASK) $(RUN_TASK) $(REFERENCE_CHECK_TASK) || echo "check reference FAILED") &> $@

# Builds the reference file.
$(REFERENCE_BUILD_OUTPUTS):
	(make -C $(dir $@) $(CLEAN_TASK) $(BUILD_TASK) $(RUN_TASK) $(REFERENCE_BUILD_TASK) || echo "make reference FAILED") &> $@

# Builds all the reference files.
$(REFERENCE_BUILD_RESULT): $(REFERENCE_BUILD_OUTPUTS)
	-@grep 'FAILED' $(REFERENCE_BUILD_OUTPUTS) > $@
	@echo "`cat $@ | wc -l` of $(words $(REFERENCE_BUILD_OUTPUTS)) failed" >> $@
	@clear
	@cat $@

# Builds all the reference files.
$(REFERENCE_CHECK_RESULT): $(REFERENCE_CHECK_OUTPUTS)
	-@grep 'FAILED' $(REFERENCE_CHECK_OUTPUTS) > $@
	@echo "`cat $@ | wc -l` of $(words $(REFERENCE_CHECK_OUTPUTS)) failed" >> $@
	@clear
	@cat $@

