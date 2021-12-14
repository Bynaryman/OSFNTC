#ifndef FPDIV_HPP
#define FPDIV_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "Operator.hpp"
#include "TestBenches/FPNumber.hpp"

namespace flopoco{

	/** The FPDiv class */
	class FPDiv : public Operator
	{
	public:
		/**
		 * The FPDiv constructor
		 * @param[in]		target		the target device
		 * @param[in]		wE			the width of the exponent for the f-p number X
		 * @param[in]		wF			the width of the fraction for the f-p number X
		 */
		FPDiv(OperatorPtr parentOp, Target* target, int wE, int wF, int radix=0);

		/**
		 * FPDiv destructor
		 */
		~FPDiv();


		/**
		 * compute the selection function table 
		 * See the Digital Arithmetic book by Ercegovac and Lang for details
     * Some of these parameters are computed by the utility NbBitsMin

		 * @param dMin 0.5  if there is no prescaling; In case of prescaling, can be larger
		 * @param dMax 1 if there is no prescaling; In case of prescaling, can be larger
		 * @param nbBitD  number of bits of d to input to the selection table
		 * @param number of bits of w to input to the selection table
		 * @param alpha digit set is {-alpha, .. alpha} 
		 * @param radix radix used
		 * @param wIn  table input size
		 * @param wOut table output size 
		 */

		vector<mpz_class> selFunctionTable(double dMin, double dMax, int nbBitD, int nbBitW, int alpha, int radix, int wIn, int wOut);
		
		/**
		 * Emulate a correctly rounded division using MPFR.
		 * @param tc a TestCase partially filled with input values
		 */
		void emulate(TestCase * tc);

		/* Overloading the Operator method */
		void buildStandardTestCases(TestCaseList* tcl);

		static TestList unitTest(int index);

		// User-interface stuff
		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args);
		static void registerFactory();

	private:
		static void plotPDDiagram(int delta, int t, int radix, int digitSet);
		static bool checkDistrib(int delta, int t, int radix, int digitSet);
		static double L(int k, double ro, double d);
		static double U(int k, double ro, double d);
		static double estimateCost(int nbBit, int radix, int digitSet);
		static void computeNbBit(int radix, int digitSet);
	public:
		static void NbBitsMinRegisterFactory();
		static OperatorPtr NbBitsMinParseArguments(OperatorPtr parentOp, Target *target, vector<string> &args);

		
	private:
		/** The width of the exponent for the input X */
		int wE;
		/** The width of the fraction for the input X */
		int wF;
		/** The number of iterations */
		int nDigit;


		
	};
}
#endif //FPDIV_HPP
