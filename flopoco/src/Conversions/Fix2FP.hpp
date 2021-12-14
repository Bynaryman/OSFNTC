#ifndef Fix2FP_HPP
#define Fix2FP_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "Operator.hpp"
#include "TestBenches/FPNumber.hpp"
#include "ShiftersEtc/LZOCShifterSticky.hpp"
#include "ShiftersEtc/LZOC.hpp"
#include "IntAddSubCmp/IntAdder.hpp"


namespace flopoco{

	/** The Fix2FP class */
	class Fix2FP : public Operator
	{
	public:
		/**
		 * @brief The  constructor
		 * @param[in]		target		the target device
		 * @param[in]		Signed	is the Fix number signed?
		 * @param[in]		MSB			the MSB of the input number
		 * @param[in]		LSB			the LSB of the input number
		 * @param[in]		wER			the with of the exponent for the convertion result
		 * @param[in]		wFR			the with of the fraction for the convertion result
		 */
		Fix2FP(Target* target, bool Signed, int MSBI, int LSBI, int wER, int wFR);

		/**
		 * @brief destructor
		 */
		~Fix2FP();


		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();



	private:
		/** The MSB for the input */
		int MSBI;
		/** The LSB for the input */
		int LSBI;
		/** are all numbers positive or not */
		bool Signed;
		/** The width of the exponent for the output R */
		int wER;
		/** The width of the fraction for the output R */
		int wFR;


		/**The leading sign counter	*/
		LZOCShifterSticky* lzocs;
		/**The leading zero counter	*/
		LZOCShifterSticky* lzcs;
		/** The integer adder object for subtraction from the MSB the position of the leading 1, for shifting the number */
		IntAdder* fractionConvert;
		/** The integer adder object for adding 1 to the fraction part*/
		IntAdder* roundingAdder;
		/** The integer adder object for substracting 1 from the remainder of the fraction to establish if it is zero*/
		IntAdder* oneSubstracter;
		/** The integer adder object for transforming the Count of LZO to exponent*/
		IntAdder* exponentConversion;
		/** The integer adder object for adding the biass to the exponent*/
		IntAdder* exponentFinal;
		/** The integer adder object for zero detector of the input*/
		IntAdder* zeroD;
		/** The integer adder object for correcting the exponent*/
		IntAdder* expCorrect;




		int wF;
		int wE;
		int sizeExponentValue;
		int sizeFractionPlusOne;
		int MSB;
		int LSB;
		int inputWidth;


	};
}
#endif
