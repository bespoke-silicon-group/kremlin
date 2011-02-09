#!/usr/bin/env python

from MakefileGenerator import MakefileGenerator
from optparse import OptionParser
import os
import subprocess
import sys

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

# Creates an option that should just be passed to GCC through setting CFLAGS
# in the makefile.
class GccOption:

    def __init__(self, parser, *flags, **parser_options):

        """Adds the option to the parser. Has the exact same signature as
        OptionParser.add_option except it takes one as the first argument."""


        parser_option_action = "action"
        parser_option_dest = "dest"
        parser_option_default = "default"

        self.parser = parser
        self.flags = flags
        self.dest = parser_options[parser_option_dest]

        # Default to empty list if no default is specified
        if not parser_option_default in parser_options:
            parser_options[parser_option_default] = []

        # Default to append if no action is specified
        if not parser_option_action in parser_options:
            parser_options[parser_option_action] = "append"

        parser.add_option(*flags, **parser_options)

    def get_cflags_str(self, options):
        """Takes the options returned by OptionParser.parse_args() and returns
        a string of flags to be added to CFLAGS. These flags are the same
        flags passed to this python script."""

        value = options.__dict__[self.dest]

        # If just a plain flag, add it if it's true
        if value == True:
            return self.flags[0]

        # Otherwise, don't add it.
        if value == False:
            return ""

        if isinstance(value, str):
            return self.flags[0] + value

        # If the flag can be specified multiple times, add them all.
        if isinstance(value, list):
            return " ".join([self.flags[0] + value for value in value])

        raise "Uknown type?: " + value

def create_kremlin_mk(src_lang):
    usage = "usage: %prog options sources"
    parser = OptionParser(usage = usage)

    # Output file target
    parser.add_option("-o", dest = "target", default = None)


    # Partial compilation
    parser.add_option("-c", action = "store_true", dest = "assemble", default = False)
    parser.add_option("-S", action = "store_true", dest = "compile", default = False)
    parser.add_option("-E", action = "store_true", dest = "preprocess", default = False)

    # These options should just be passed to GCC by adding them to CFLAGS
    gcc_options = [

        # Includes
        GccOption(parser, "-I", dest = "includeDirs"),

        # Language
        GccOption(parser, "-x", choices = ['none', 'c', 'c++'], action = "store", dest = "language", default = 'none'),

        GccOption(parser, "-D", dest = "preprocessor_defines"),
        GccOption(parser, "-O", dest = "optimization"),
        GccOption(parser, "-f", dest = "fOpts"),
        GccOption(parser, "-g", action = "store_true", dest = "debug"),

        # Load library
        GccOption(parser, "-l", dest = "load_library"),

        GccOption(parser, "-W", dest = "wOpts")]

    #pyrprof specific options
    parser.add_option("--Pno-inline", action = "store_false", dest = "inline", default = True)

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

        if arg_ext == "o": lang_ext = lang_ext
        elif lang_ext == "": lang_ext = arg_ext
        elif lang_ext != arg_ext: 
            sys.exit("ERROR: mixed input languages detected.")

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

        # set LD according to source language
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
