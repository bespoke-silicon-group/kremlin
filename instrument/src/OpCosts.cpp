#include "OpCosts.h"

#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>
#include <vector>

/**
 * Constructs a new OpCost with the default values.
 */
OpCosts::OpCosts() : 
	int_add(INT_ADD),
	int_sub(INT_SUB),
	int_mul(INT_MUL),
	int_div(INT_DIV),
	int_mod(INT_MOD),
	int_cmp(INT_CMP),
	fp_add(FP_ADD),
	fp_sub(FP_SUB),
	fp_mul(FP_MUL),
	fp_div(FP_DIV),
	fp_mod(FP_MOD),
	fp_cmp(FP_CMP),
	logic(LOGIC)
{
}

/**
 * Parses op costs from a file.
 *
 * This will fill the struct with the values read from the file. The file is
 * expected to have the name of the operation, whitespace, and the cost of the 
 * operation as an integer. Each of these pairs is expected to be separated by 
 * new lines.
 *
 * @param filename The filename to read and parse values from.
 */
void OpCosts::parseFromFile(const std::string& filename) {
	std::string line;
	std::ifstream opcosts_file;

	opcosts_file.open(filename.c_str());

	// make sure this file is actually open, otherwise print error and abort
	if(!opcosts_file.is_open()) {
		std::cerr << "ERROR: Specified op costs file (" << filename << ") could not be opened.\n";
		assert(0);
	}

	while(!opcosts_file.eof()) {
		getline(opcosts_file,line);

		// skip empty lines
		if(line == "") { continue; }

		std::cerr << "read in op cost line: " << line;

		// tokenize the whitespace separated fields of this line
		std::istringstream iss(line);

		std::vector<std::string> tokens;
		std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), std::back_inserter<std::vector<std::string> >(tokens));

		// should be of format "OP_NAME cost"
		assert(tokens.size() == 2 && "Incorect format for op cost file");

		// convert cost to unsigned int
		std::istringstream cost_ss(tokens[1]);
		unsigned int cost;
		cost_ss >> cost;

		std::string cost_name = tokens[0];

		// assign cost to correct field
		if(cost_name == "INT_ADD") { int_add = cost; }
		else if(cost_name == "INT_SUB") { int_sub = cost; }
		else if(cost_name == "INT_MUL") { int_mul = cost; }
		else if(cost_name == "INT_DIV") { int_div = cost; }
		else if(cost_name == "INT_MOD") { int_mod = cost; }
		else if(cost_name == "INT_CMP") { int_cmp = cost; }
		else if(cost_name == "FP_ADD") { fp_add = cost; }
		else if(cost_name == "FP_SUB") { fp_sub = cost; }
		else if(cost_name == "FP_MUL") { fp_mul = cost; }
		else if(cost_name == "FP_DIV") { fp_div = cost; }
		else if(cost_name == "FP_MOD") { fp_mod = cost; }
		else if(cost_name == "FP_CMP") { fp_cmp = cost; }
		else if(cost_name == "LOGIC") { logic = cost; }
		else {
			std::cerr << "ERROR: unknown op name (" << cost_name << ") in op cost file.\n";
			assert(0);
		}
	}

	opcosts_file.close();
}

template <typename Ostream>
Ostream& operator<<(Ostream& os, const OpCosts& costs)
{
	os << "op costs:" << "\n";
	os << "\tINT_ADD: " << costs.int_add << "\n";
	os << "\tINT_SUB: " << costs.int_sub << "\n";
	os << "\tINT_MUL: " << costs.int_mul << "\n";
	os << "\tINT_DIV: " << costs.int_div << "\n";
	os << "\tINT_MOD: " << costs.int_mod << "\n";
	os << "\tINT_CMP: " << costs.int_cmp << "\n";
	os << "\tFP_ADD: " << costs.fp_add << "\n";
	os << "\tFP_SUB: " << costs.fp_sub << "\n";
	os << "\tFP_MUL: " << costs.fp_mul << "\n";
	os << "\tFP_DIV: " << costs.fp_div << "\n";
	os << "\tFP_MOD: " << costs.fp_mod << "\n";
	os << "\tFP_CMP: " << costs.fp_cmp << "\n";
	os << "\tLOGIC: " << costs.logic << "\n";

	return os;
}

/**
 * Prints this struct to the stream.
 *
 * @param os The stream to print to.
 * @param costs The struct to print.
 * @return os.
 */
llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const OpCosts& costs)
{
	operator<< <llvm::raw_ostream>(os, costs);

	return os;
}

/**
 * Prints this struct to the stream.
 *
 * @param os The stream to print to.
 * @param costs The struct to print.
 * @return os.
 */
std::ostream& operator<<(std::ostream& os, const OpCosts& costs)
{
	os << costs;
	return os;
}
