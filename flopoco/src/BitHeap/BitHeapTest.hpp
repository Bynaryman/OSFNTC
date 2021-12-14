#ifndef BITHEAPTEST_HPP
#define BITHEAPTEST_HPP


#include <iostream>
#include <sstream>
#include <vector>
#include <gmpxx.h>
#include <stdio.h>
#include <string>
#include <time.h>

#include "gmp.h"
#include "mpfr.h"

#include "../Operator.hpp"
#include "BitHeap.hpp"

#include "../utils.hpp"

namespace flopoco
{

	/**
	 * Random tests for the new bitheap
	 */
	class BitheapTest : public Operator
	{
	public:

		/**
		 * A basic constructor
		 */
		BitheapTest(Target * target, int msb, int lsb, int nbInputs = -1, bool isSigned = false, int weight = 0);

		/**
		 * Destructor
		 */
		~BitheapTest();


		void emulate(TestCase * tc);

		//void buildStandardTestCases(TestCaseList* tcl);

		// User-interface stuff
		/**
		 * Factory method
		 */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/**
		 * Register the factory
		 */
		static void registerFactory();

	public:
		BitHeap *bitheap;                /**< the heights of the columns */
		int nbInputs;                       /**< the number of inputs to the bitheap */
		vector<int> weights;                /**< the weights of the inputs */
		int weight;                         /**< the weight for all the inputs */
		int size;                           /**< the number of bits at the input of the compressor */
		int msb;                            /**< the msb of the bitheap */
		int lsb;                            /**< the lsb of the bitheap */
		bool isSigned;                      /**< is the bitheap signed, or not */
	};
}

#endif
