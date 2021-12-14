#ifndef FPADDER3INPUT_HPP
#define FPADDER3INPUT_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "../Operator.hpp"
#include "../ShiftersEtc/LZOC.hpp"
#include "../ShiftersEtc/Shifters.hpp"
#include "../ShiftersEtc/LZOCShifterSticky.hpp"
#include "../TestBenches/FPNumber.hpp"
#include "../IntAddSubCmp/IntAdder.hpp"
#include "../IntAddSubCmp/IntDualSub.hpp"

namespace flopoco{

	/** The FPAdd3Input class */
	class FPAdd3Input : public Operator
	{
	public:
		/**
		 * The FPAdd3Input constructor
		 * @param[in]		target		the target device
		 * @param[in]		wEX			the the with of the exponent for the f-p number X
		 * @param[in]		wFX			the the with of the fraction for the f-p number X
		 * @param[in]		wEY			the the with of the exponent for the f-p number Y
		 * @param[in]		wFY			the the with of the fraction for the f-p number Y
		 * @param[in]		wER			the the with of the exponent for the addition result
		 * @param[in]		wFR			the the with of the fraction for the addition result
		 */
		FPAdd3Input(Target* target, int wE, int wF, map<string, double> inputDelays = emptyDelayMap);

		/**
		 * FPAdd3Input destructor
		 */
		~FPAdd3Input();


		void emulate(TestCase * tc);
// 		void buildStandardTestCases(TestCaseList* tcl);
// 		TestCase* buildRandomTestCase(int i);

		// User-interface stuff
		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		static void registerFactory();



	private:


		/** The combined leading zero counter and shifter for the close path */
		LZOCShifterSticky* lzocs;
		/** The integer adder object for subtraction in the close path */
		IntAdder *fracSubClose;
		/** The dual subtractor for the close path */
		IntDualSub *dualSubClose;
		/** The fraction adder for the far path */
		IntAdder *fracAddFar;
		/** The adder that does the final rounding */
		IntAdder *finalRoundAdd;
		/** The right shifter for the far path */
		Shifter* rightShifter;


		int sizeRightShift;

		int wE;
		int wF;
	};

}

#endif
