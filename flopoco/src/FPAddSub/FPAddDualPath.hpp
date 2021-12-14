#ifndef FPADDERDUALPATH_HPP
#define FPADDERDUALPATH_HPP
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
#include "../IntAddSubCmp/IntDualAddSub.hpp"

namespace flopoco{

	/** The FPAddDualPath class */
	class FPAddDualPath : public Operator
	{
	public:
		/**
		 * The FPAddDualPath constructor
		 * @param[in]		target		the target device
		 * @param[in]		wE			the the with of the exponent for the f-p number X
		 * @param[in]		wF			the the with of the fraction for the f-p number X
		 * @param[in]		sub			perform subtraction
		 * @param[in]		onlyPositiveIO selects special case for only positive float input and output numbers
		 */
		FPAddDualPath(OperatorPtr parentOp, Target* target, int wE, int wF, bool sub=false, bool onlyPositiveIO=false);

		/**
		 * FPAddDualPath destructor
		 */
		~FPAddDualPath();


		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);
		TestCase* buildRandomTestCase(int i);



	private:
		/** The width of the exponent */
		int wE;
		/** The width of the fraction */
		int wF;
		/** do you want an adder or a subtractor? */
		bool sub;

		/** selects special case for only positive float input and output numbers */
		bool onlyPositiveIO;

		/** The combined leading zero counter and shifter for the close path */
		LZOCShifterSticky* lzocs;
		/** The integer adder object for subtraction in the close path */
		IntAdder *fracSubClose;
		/** The dual subtractor for the close path */
		IntDualAddSub *dualSubClose;
		/** The fraction adder for the far path */
		IntAdder *fracAddFar;
		/** The adder that does the final rounding */
		IntAdder *finalRoundAdd;
		/** The right shifter for the far path */
		Shifter* rightShifter;

		int sizeRightShift;

	};

}

#endif
