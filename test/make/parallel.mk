ifndef PARALLEL_MK
PARALLEL_MK = 1

# TODO: implement.

PARALLEL_JOBS = 4

parallel:
	make $(MAKECMDGOALS)

endif # PARALLEL_MK
