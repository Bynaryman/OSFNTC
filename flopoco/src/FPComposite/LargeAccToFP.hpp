#ifndef LARGACCUMULATORTOFP_HPP
#define LARGACCUMULATORTOFP_HPP
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "Operator.hpp"
#include "ShiftersEtc/LZOCShifterSticky.hpp"
#include "IntAddSubCmp/IntAdder.hpp"


namespace flopoco{

	/** Operator which converts the output of the long accumulator to the desired FP format
	 */
	class LargeAccToFP : public Operator
	{
	public:

		/** Constructor
		 * @param target the target device
		 * @param MaxMSBX the weight of the MSB of the expected exponent of X
		 * @param LSBA the weight of the least significand bit of the accumulator
		 * @param MSBA the weight of the most significand bit of the accumulator
		 * @param wEOut the width of the output exponent 
		 * @param eFOut the width of the output fractional part
		 */ 
		LargeAccToFP(Target* target, int MSBA, int LSBA, int wEOut, int wFOut);

		/** Destructor */
		~LargeAccToFP();
		
		void emulate(TestCase * tc);
		
		void buildStandardTestCases(TestCaseList* tcl);
		
		TestCase* buildRandomTestCase(int i);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();


	protected:
		int LSBA_;    /**< the weight of the least significand bit of the accumulator */
		int MSBA_;    /**< the weight of the most significand bit of the accumulator */
		int wEOut_;   /**< the width of the output exponent */
		int wFOut_;   /**< the width of the output fractional part */

	private:
		Target* ownTarget_;
		IntAdder* adder_;
		LZOCShifterSticky* lzocShifterSticky_;   
		int      sizeAcc_;       /**< The size of the accumulator  = MSBA-LSBA+1; */
		int      expBias_;       /**< the exponent bias value */
		int      countWidth_;    /**< the number of bits that the leading zero/one conunter outputs the result on */

	};
}
#endif
