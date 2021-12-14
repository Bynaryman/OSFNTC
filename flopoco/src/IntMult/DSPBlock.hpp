#pragma once

#include <string>
#include <iostream>
#include <string>
#include <gmp.h>
#include <gmpxx.h>
#include "Target.hpp"
#include "Operator.hpp"
#include "Table.hpp"
#include "BaseMultiplier.hpp"

namespace flopoco {

/**
* The DSPBlock class represents the implementation of a DSP block commonly found in FPGAs.
*
*/
class DSPBlock : public Operator
{
public:
    /**
     * @brief Constructor of a common DSPBlock that translates to DSP implementations when the sizes are chosen accordingly
     *
     * Depending on the inputs, the following operation is performed.
     * ------------------------------------------------------------------
     * usePostAdder | usePreAdder | preAdderSubtracts | Operation
     * ------------------------------------------------------------------
     *    false     |    false    |    false          |  X * Y
     *    true      |    false    |    false          |  X * Y + Z
     *    false     |    true     |    false          |  (X1+X2) * Y
     *    true      |    true     |    false          |  (X1+X2) * Y + Z
     *    false     |    true     |    true           |  (X1-X2) * Y
     *    true      |    true     |    true           |  (X1-X2) * Y + Z
     * ------------------------------------------------------------------
     *
	 * For Xilinx FPGAs, select wX=25, wY=17 and wZ=42 with usePreAdder=1 usePostAdder=1 for a DSP block which is detectable by synthesis (tested with ISE 13.4 and ISE 14.7)
	 *
     * @param parentOp A pointer to the parent Operator
     * @param target A pointer to the target
     * @param wX input word size of X or X1 and X2 (when pre-adders are used)
     * @param wY input word size of Y
     * @param wZ input word size of Z (when post-adders are used)
     * @param usePostAdder enables post-adders when set to true (see table above)
     * @param usePreAdder enables pre-adders when set to true (see table above)
     */
	DSPBlock(Operator *parentOp, Target* target, int wX, int wY, bool xIsSigned=false, bool yIsSigned=false, bool isPipelined=false, int wZ=0, bool usePostAdder=false, bool usePreAdder=false, bool preAdderSubtracts=false);

//	void emulate (TestCase* tc);
//	void buildStandardTestCases(TestCaseList* tcl);

    /** Factory method */
    static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
    /** Register the factory */
    static void registerFactory();

private:
	bool xIsSigned_;
	bool yIsSigned_;
	int wX_;
	int wY_;
};
}

