#ifndef INTCONSTMULTOPT_HPP
#define INTCONSTMULTOPT_HPP

#if defined(HAVE_PAGLIB) && defined(HAVE_OSCM) && defined(HAVE_SCALP)

#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <cstdlib>

#include "../Operator.hpp"
#include "IntAddSubCmp/IntAdder.hpp"
#include "IntConstMultShiftAdd.hpp"

/**
	Integer constant multiplication using minimum number of adders due to

	Gustafsson, O., Dempster, A., Johansson, K., Macleod, M., & Wanhammar, L. (2006).
	Simplified Design of Constant Coefficient Multipliers. Circuits, Systems, and Signal Processing, 25(2), 225–251.


	All constants up to 19 bit will be realized optimal using precomputed tables provided by the SPIRAL project (http://spiral.ece.cmu.edu/mcm/).

*/


namespace flopoco{

    class IntConstMultShiftAddOpt : public IntConstMultShiftAdd
	{
	public:
		/** The standard constructor, inputs the number to implement */ 
        IntConstMultShiftAddOpt(Operator* parentOp, Target* target, int wIn, int c, bool syncInOut=true, int epsilon=0);

//		void emulate(TestCase* tc);
//        void buildStandardTestCases(TestCaseList* tcl);
		static TestList unitTest(int index);

        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args );
        static void registerFactory();
	private:
        int coeff;  /**< The constant */
        int wIn;

        void generateAOp(int a, int b, int c, int eA, int eB, int signA, int signB, int preFactor=1);
		void buildAdderGraph(int c, int preFactor=1);
		void generateVHDLOptSCM(int c);

        stringstream adderGraph;
#else
namespace flopoco{
	class IntConstMultShiftAddOpt
	{
	public:
		static void registerFactory();
#endif //defined(HAVE_PAGLIB) && defined(HAVE_OSCM)
	};
}

#endif
