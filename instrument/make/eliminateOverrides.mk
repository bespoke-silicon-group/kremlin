ifndef ELIMINATE_OVERRIDES_MK
ELIMINATE_OVERRIDES_MK = 1

$(info sources before: $(SOURCES), orgin: $(origin SOURCES))

ifeq ($(origin SOURCES),command line)
unexport SOURCES
endif # SOURCES

endif # ELIMINATE_OVERRIDES_MK
