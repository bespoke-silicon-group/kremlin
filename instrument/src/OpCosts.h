#ifndef OP_COSTS_H
#define OP_COSTS_H

#include <string>
#include <llvm/Support/raw_ostream.h>

#define INT_ADD 1
#define INT_SUB 1
#define INT_MUL 2
#define INT_DIV 10
#define INT_MOD 10

#define FP_ADD 2
#define FP_SUB 2
#define FP_MUL 5
#define FP_DIV 20
#define FP_MOD 20

#define LOGIC 1
#define INT_CMP 1
#define FP_CMP 2 

#define FILE_READ 10
#define FILE_WRITE 10

/**
 * Holds all the costs for operations.
 */
struct OpCosts {

	/**
	 * The cost of an integer add operation.
	 */
	unsigned int int_add;

	/**
	 * The cost of an integer subtract operation.
	 */
	unsigned int int_sub;

	/**
	 * The cost of an integer multiplication operation.
	 */
	unsigned int int_mul;

	/**
	 * The cost of an integer division operation.
	 */
	unsigned int int_div;
	
	/**
	 * The cost of an integer modulus operation.
	 */
	unsigned int int_mod;

	/**
	 * The cost of an integer compare operation.
	 */
	unsigned int int_cmp;

	/**
	 * The cost of an floating point add operation.
	 */
	unsigned int fp_add;

	/**
	 * The cost of an floating point subtract operation.
	 */
	unsigned int fp_sub;

	/**
	 * The cost of an floating point multiplication operation.
	 */
	unsigned int fp_mul;

	/**
	 * The cost of an floating point division operation.
	 */
	unsigned int fp_div;
	
	/**
	 * The cost of an floating point modulus operation.
	 */
	unsigned int fp_mod;

	/**
	 * The cost of an floating point compare operation.
	 */
	unsigned int fp_cmp;

	/**
	 * The cost of logic operations.
	 */
	unsigned int logic;

	OpCosts();

	void parseFromFile(const std::string& filename);
};

std::ostream& operator<<(std::ostream& os, const OpCosts& costs);
llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const OpCosts& costs);

#endif // OP_COSTS_H

