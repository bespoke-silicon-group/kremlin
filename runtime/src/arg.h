#ifndef _KREMLIN_ARGPARSE_HPP_
#define _KREMLIN_ARGPARSE_HPP_

#include <vector>

class KremlinConfiguration;

void parseKremlinOptions(KremlinConfiguration &config, 
							int argc, char* argv[], 
							std::vector<char*> &native_args);

#endif // _KREMLIN_ARGPARSE_HPP_
