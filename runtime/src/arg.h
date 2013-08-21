#ifndef _KREMLIN_ARGPARSE_HPP_
#define _KREMLIN_ARGPARSE_HPP_

class KremlinConfiguration;

void parseKremlinOptions(KremlinConfiguration &config, int argc, char* argv[], unsigned& num_args, char**& real_args);

#endif // _KREMLIN_ARGPARSE_HPP_
