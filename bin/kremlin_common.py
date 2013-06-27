#!/usr/bin/env python

from MakefileGenerator import MakefileGenerator
#from optparse import OptionParser
import argparse
import os
import subprocess
import sys
from GccOption import GccOption

# This is kind of ugly. Refactoring highly welcome!
def get_make_target(options):

    """ 
    Returns a tuple. The first value is the target to make. That is, make will
    be run as "make <first value returned by this function>"

    The second is any text that needs to be added to the makefile.
    """

    # When gcc is passed options to only preprocess, compile, assemble, it
    # choose the option that requires the least amount of work. That is, if -E
    # is specified to preprocess only, it will only preprocess. It will not
    # compile, assemble or link even if -c or -S are specified.

    make_target = "link"

    if options.assemble:
        make_target = "assemble"

    if options.compile:
        make_target = "compile"

    # TODO: Write this rule in the makefile.
    if options.preprocess:
        make_target = "preprocess"

    # if -o is specified, set that as the output filename
    if options.target:
        return make_target, "#" + options.target
    else:
        return make_target, ""


def create_kremlin_mk(src_lang):
    parser = argparse.ArgumentParser(prog='kremlin-cc')

    # Output file target
    parser.add_argument("-o", dest="target", help="Place output in file.")

    # Partial compilation
    parser.add_argument("-c", dest = "assemble", action="store_true",
						help="Compile or assemble the source files, \
								but do not link.")
    parser.add_argument("-S", dest = "compile", action="store_true",
						help="Stop after the stage of compilation proper; \
								do not assemble.")
    parser.add_argument("-E", dest = "preprocess", action="store_true",
						help="Stop after the preprocessing stage; do not \
								run the compiler proper.")

    # These options should just be passed to Clang by adding them to CFLAGS
    # (Note: this becomes CCFLAGS for scons environment)
    compiler_options = [

        # Includes
        GccOption(parser, "-I", dest = "includeDirs"),

        # Language
        GccOption(parser, "-x", choices = ['none', 'c', 'c++'], action = "store", dest = "language", default = 'none'),

        GccOption(parser, "-D", dest = "preprocessor_defines"),
        GccOption(parser, "-O", dest = "optimization"),
        GccOption(parser, "-f", dest = "fOpts"),
        GccOption(parser, "-p", dest = "profiler"),
        GccOption(parser, "-g", action = "store_true", dest = "debug"),

        # Load library
        GccOption(parser, "-l", dest = "load_library"),

        GccOption(parser, "-W", dest = "wOpts"),

        GccOption(parser, "-r", dest = "linker_symbols")]

    def fix_flags(s):
        if s.startswith("-Wl,"):
            return s.replace("-Wl,","-Wl=")
        elif s.startswith("-Wa,"):
            return s.replace("-Wa,","-Wa=")
        elif s.startswith("-Wp,"):
            return s.replace("-Wp,","-Wp=")
        else:
            return s

    fixed = map(fix_flags, sys.argv[1:])
    options, args = parser.parse_known_args(fixed)

    # TODO: FIXME
    def check_args(parser, options, args):
        if options.target and len(args) > 1 and (options.compile or options.assemble):
            parser.error("cannot specify -o with -c or -S with multiple files")

    check_args(parser, options, args)

    lang_ext = ""

    #print "args: " + " ".join(args)

    make_target, output_filename = get_make_target(options)

    makefile_name = "SConstruct.kremlin"
    make_args = ["scons", "-f", makefile_name, make_target]

    def write_makefile(out):
        def write_stdout_and_makefile(str):
            line = str + "\n"
            #sys.stdout.write(line)
            out.write(line)

        write = write_stdout_and_makefile
        write("env = Environment(CCFLAGS = \'" + \
                " ".join([option.get_cflags_str(options) \
                    for option in compiler_options]) + "\')")
        write("input_files = " + str(['#{0}'.format(i) for i in args]))
        write("target = \'" + make_target + "\'")
        write("output_file = \'" + output_filename + "\'")
        #if options.krem_debug:
        #    write("DEBUG = 1")

        # set LD according to source language
        #
        # Setting language specific options is not common. It sounds like they
        # should go in kremlin-gcc/kremlin-g++/kremlin-gfortran frontends
        # Also, 
        #    --chris
        """
        if src_lang == "fortran":
            if lang_ext in ["f95","f90","f",""]: write("LD = gfortran")
            else: sys.exit("specified fortran with non-fortran extension: %s" % lang_ext)
        elif src_lang == "cpp":
            if lang_ext in ["cpp","C",""]: write("LD = g++")
            else: sys.exit("specified c++ with non-c++ extension: %s" % lang_ext)
        else: 
            if lang_ext in ["c",""]: write("LD = g++")
            else: sys.exit("specified C with non-C extension: %s" % lang_ext)
        """

        #write("include " + sys.path[0] + "/../instrument/make/kremlin.mk")
        write("Export(\'env\', \'input_files\', \'target\', \'output_file\')")
        write("SConscript(\'" + sys.path[0] + "/../instrument/make/SConscript\')")

    # Write the makefile to disk
    makefile = open(makefile_name, "w")
    write_makefile(makefile)
    makefile.close()

    # Run make
    #print("running: " + ' '.join(make_args))

    make_process = subprocess.Popen(make_args, stdin = subprocess.PIPE)
    make_process.stdin.close()
    make_process.wait()

    # check that make_process exited correctly
    assert make_process.returncode == 0
