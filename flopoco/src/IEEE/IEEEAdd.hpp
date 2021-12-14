#ifndef FPADDERSPIEEE_HPP
#define FPADDERSPIEEE_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "Operator.hpp"
#include "ShiftersEtc/LZOC.hpp"
#include "ShiftersEtc/Shifters.hpp"
#include "ShiftersEtc/LZOCShifterSticky.hpp"
#include "TestBenches/FPNumber.hpp"
#include "IntAddSubCmp/IntAdder.hpp"
#include "IntAddSubCmp/IntDualAddSub.hpp"

namespace flopoco{

	/** The IEEEAdd class */
	class IEEEAdd : public Operator
	{
	public:
		/**
		 * The IEEEAdd constructor
		 * @param[in]		target		the target device
		 * @param[in]		wE			the the with of the exponent
		 * @param[in]		wF			the the with of the fraction
		 */
		IEEEAdd(OperatorPtr parentOp, Target* target, int wE, int wF, bool sub=false);

		/**
		 * IEEEAdd destructor
		 */
		~IEEEAdd();


		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);
		TestCase* buildRandomTestCase(int i);

		static TestList unitTest(int index);

		// User-interface stuff
		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		static void registerFactory();


	private:
		/** The width of the exponent */
		int wE;
		/** The width of the fraction */
		int wF;
		/** Is this an FPAdd or an FPSub? */
		bool sub;

		int sizeRightShift;
		
		int sizeLeftShift;

	};

}

#endif
