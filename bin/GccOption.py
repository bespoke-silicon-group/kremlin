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

        self.separator = parser_options.pop("separator","")

        # Default to empty list if no default is specified
        if not parser_option_default in parser_options:
            parser_options[parser_option_default] = []

        # Default to store if no action is specified
        if not parser_option_action in parser_options:
            parser_options[parser_option_action] = "store"

        parser.add_argument(*flags, **parser_options)

    def get_cflags_str(self, options):
        """Takes the options returned by OptionParser.parse_args() and returns
        a string of flags to be added to CFLAGS. These flags are the same
        flags passed to this python script."""

        value = options.__dict__[self.dest]

        if value == None:
            return ""

        # If just a plain flag, add it if it's true
        if value == True:
            return self.flags[0]

        # Otherwise, don't add it.
        if value == False:
            return ""

        def add_escapes(my_str, single_escape, double_escape):
            """ Return string that has escape char (\) added before any
			character in single_escape and double escape before any character
			in double_escape."""
            unique_symbols = set(single_escape)
            fixed_str = my_str
            for sym in unique_symbols:
                fixed_str = fixed_str.replace(str(sym), '\\' + str(sym))
            unique_symbols = set(double_escape)
            for sym in unique_symbols:
                fixed_str = fixed_str.replace(str(sym), '\\\\' + str(sym))
            return fixed_str

        if isinstance(value, str):
            return self.flags[0] + self.separator + add_escapes(value,"'<>",'"')

        # If the flag can be specified multiple times, add them all.
        if isinstance(value, list):
            return " ".join([self.flags[0] + self.separator + add_escapes(v,"'<>",'"') for v in value if v])

        raise "Unknown type?: " + value

