#ifndef FPADD_HPP
#define FPADD_HPP
#include "../Operator.hpp"
#include "FPAddSinglePath.hpp"
#include "FPAddDualPath.hpp"


namespace flopoco{
	class FPAdd{
	public:
		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		static TestList unitTest(int index);

		/** Factory register method */ 
		static void registerFactory();

		/** emulate() function to be shared by various implementations */
		static void emulate(TestCase * tc, int wE, int wF, bool subtract);

		/** Random FP number generator biased to stress floating-point addition,
				to be shared by various implementations */
		static TestCase* buildRandomTestCase(Operator* op, int i, int wE, int wF, bool subtract, bool onlyPositiveIO=false);

		/** Regression tests */
		static void buildStandardTestCases(Operator* op, int wE, int wF, TestCaseList* tcl, bool onlyPositiveIO=false);

	};
}
#endif
