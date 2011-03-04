#!/usr/bin/env python

from MakefileGenerator import MakefileGenerator
from optparse import OptionParser
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

    # If -o is specified, we build that target.
    if options.target:
        if options.preprocess:
            return options.target, "PREPROCESS_OUTPUT_FILE = " + options.target
        if options.assemble:
            return options.target, "ASM_OUTPUT_FILE = " + options.target
        if options.compile:
            return options.target, "COMPILE_OUTPUT_FILE = " + options.target
        return options.target, "LINK_OUTPUT_FILE = " + options.target

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

    if options.target:
        make_target = options.target
        
    return make_target, ""


def create_kremlin_mk(src_lang):
    usage = "usage: %prog options sources"
    parser = OptionParser(usage = usage)

    # Output file target
    parser.add_option("-o", dest = "target", default = None)

    # Partial compilation
    parser.add_option("-c", action = "store_true", dest = "assemble", default = False, help = "Compile and assemble, but do not link")
    parser.add_option("-S", action = "store_true", dest = "compile", default = False, help = "Compile only; do not assemble or link")
    parser.add_option("-E", action = "store_true", dest = "preprocess", default = False, help = "Preprocess only; do not compile, assemble or link")

    # These options should just be passed to GCC by adding them to CFLAGS
    gcc_options = [

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

    # kremlin specific options
    parser.add_option("--krem-inline", action = "store_true", dest = "krem_inline", default = False)
    parser.add_option("--krem-debug", action = "store_true", dest = "krem_debug", default = False)

    (options, args) = parser.parse_args()

    def check_options(parser, options, args):
        if options.target and len(args) > 1 and (options.compile or options.assemble):
            parser.error("cannot specify -o with -c or -S with multiple files")

    check_options(parser, options, args)

    lang_ext = ""

    print "args: "
    for arg in args:
        print arg
        arg_ext = arg.split(".")[1]

        if arg_ext == "o": pass
        elif lang_ext == "": lang_ext = arg_ext
        elif lang_ext != arg_ext: 

            # This is not an error. kremlin-gcc -c foo.c bar.s baz.f should
            # work because gcc handles this properly. --chris
            raise "ERROR: mixed input languages detected."

    make_target, make_defines = get_make_target(options)

    makefile_name = "kremlin.mk"
    make_args = ["make", "-f", makefile_name, make_target]

    def write_makefile(out):
        def write_stdout_and_makefile(str):
            line = str + "\n"
            sys.stdout.write(line)
            out.write(line)

        write = write_stdout_and_makefile
        write("SOURCES = " + " ".join(args))
        write(make_defines)
        write("CFLAGS += " + " ".join([option.get_cflags_str(options) for option in gcc_options]))
        if options.krem_debug:
            write("DEBUG = 1")

        # set LD according to source language
        #
        # Setting language specific options is not common. It sounds like they
        # should go in kremlin-gcc/kremlin-g++/kremlin-gfortran frontends
        # Also, 
        #    --chris
        if src_lang == "fortran":
            if lang_ext in ["f95","f90","f",""]: write("LD = gfortran")
            else: sys.exit("specified fortran with non-fortran extension: %s" % lang_ext)
        elif src_lang == "cpp":
            if lang_ext in ["cpp","C",""]: write("LD = g++")
            else: sys.exit("specified c++ with non-c++ extension: %s" % lang_ext)
        else: 
            if lang_ext in ["c",""]: write("LD = gcc")
            else: sys.exit("specified C with non-C extension: %s" % lang_ext)

        write("include " + sys.path[0] + "/../instrument/make/kremlin.mk")

    # Write the makefile to disk
    makefile = open(makefile_name, "w")
    write_makefile(makefile)
    makefile.close()

    # Run make
    print "running: " + ' '.join(make_args)

    make_process = subprocess.Popen(make_args, stdin = subprocess.PIPE)
    # write_makefile(make_process.stdin)
    make_process.stdin.close()
    make_process.wait()

    # check that make_process exited correctly
    assert make_process.returncode == 0
