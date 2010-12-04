# ---------------------------------------------------------------------------
# Macros
# ---------------------------------------------------------------------------

# All the makefiles included without their directories prepended.
MAKEFILE_LIST_WITHOUT_DIRS = $(notdir $(MAKEFILE_LIST))

# ---------------------------------------------------------------------------
# Functions (alpha order)
# ---------------------------------------------------------------------------

# Function:          CHECK_REQUIRED_MAKEFILES_INCLUDED
# Description:       Halts make unless all the required files have been
#                    included.
#
# Parameters:        $(1)   A list of required files.
# Return:            Nothing.
CHECK_REQUIRED_MAKEFILES_INCLUDED = \
	$(if $(strip $(call MISSING_MAKEFILES,$(1))), \
		$(error The following makefiles must be included first: $(call MISSING_MAKEFILES,$(1))))

# Function:          MISSING_MAKEFILES
# Description:       Returns all the missing makefiles.
#
# Parameters:        $(1)   A list of required files.
# Return:            All the missing makefiles.
MISSING_MAKEFILES = $(filter-out $(MAKEFILE_LIST_WITHOUT_DIRS), $(1))
