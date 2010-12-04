import sys

class MakefileGenerator:
	plan_rule = "analyze"
	default_makefile_name = "Makefile.kremlin"
	default_executable_name = "a.out"
	make_rule = "compile"
	compile_rule = "compile"
	assemble_rule = "assemble"
	link_rule = "link"

	def __init__(self):
		self.name = MakefileGenerator.default_makefile_name
		self.target = MakefileGenerator.default_executable_name
		self.inline = True
		self.sources = []



	def add_source(self, source):
		"""Adds a source file to the sources list in the Makefile."""


		self.sources.append(source)



	def add_sources(self, sources):
		"""Adds source files to the sources list in the Makefile."""
		

		self.sources.extend(sources)



	def get_makefile_name(self):
		"""Returns the filename of the generated makefile."""


		return self.name



	def get_primary_build_target(self):
		"""Returns the rule in the makefile to build the primary target."""
	

		return MakefileGenerator.make_rule
	


	def set_inline(self, inline):
		"""Sets whether the kremlin library should be inlined into the executable."""


		self.inline = inline



	def set_target(self, target):
		"""Sets the target name of the executable."""


		self.target = target



	def write(self):
		"""Writes the Makefile out to disk."""


		makefile = open(self.name,"w")

		makefile.write("TARGET=%s\n"%(self.target))

		makefile.write("SOURCES=")

		for file in self.sources:
			makefile.write("%s "%(file))

		makefile.write("\n")

		if not self.inline:
			makefile.write("INLINE=0\n")

		makefile.write("LEVEL=%s/../instrument/make\n" % (sys.path[0]))
		makefile.write("\ninclude $(LEVEL)/kremlin.mk\n")

		makefile.close()

