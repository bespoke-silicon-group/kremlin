ifndef ELIMINATE_OVERRIDES_MK
ELIMINATE_OVERRIDES_MK = 1

ifdef KREMLIN_VERBOSE_BUILD
$(info sources before: $(SOURCES), orgin: $(origin SOURCES))
endif

ifeq ($(origin SOURCES),command line)
unexport SOURCES
endif # SOURCES

endif # ELIMINATE_OVERRIDES_MK
