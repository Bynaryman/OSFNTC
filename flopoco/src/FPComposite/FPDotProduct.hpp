#ifndef FPDOTPRODUCT_HPP
#define FPDOTPRODUCT_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <cstdlib>

#include "Operator.hpp"
#include "IntMult/IntMultiplier.hpp"
#include "FPLargeAcc.hpp"

namespace flopoco{

	/** The FPDotProduct class.  */
	class FPDotProduct : public Operator
	{
	public:

		/**
		 * The FPDotProduct constructor
		 * @param[in]		target	the target device
		 * @param[in]		wE			the width of the exponent for the inputs X and Y
		 * @param[in]		wFX     the width of the fraction for the input X
		 * @param[in]		wFY     the width of the fraction for the input Y
		 * @param[in]		MaxMSBX	maximum expected weight of the MSB of the summand
		 * @param[in]		LSBA    The weight of the LSB of the accumulator; determines the final accuracy of the result
		 * @param[in]		MSBA    The weight of the MSB of the accumulator; has to greater than that of the maximal expected result
		 **/ 
		FPDotProduct(Target* target, int wE, int wFX, int wFY, int MaxMSBX, int MSBA, int LSBA, map<string, double> inputDelays = emptyDelayMap);

		/**
		 * FPDotProduct destructor
		 */
		~FPDotProduct();

	
		/**
		 * Tests the operator accuracy and relative error
		 */
		void test_precision(int n);
		
		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();
	
	protected:
		/** The width of the exponent for the inputs X and Y*/
		int wE; 
		/** The width of the fraction for the input X */
		int wFX;
		/** The width of the fraction for the input Y */
		int wFY; 
		/** Maximum expected weight of the MSB of the summand */
		int MaxMSBX; 
		/** The weight of the LSB of the accumulator; determines the final accuracy of the result.*/	
		int LSBA;
		/** The weight of the MSB of the accumulator; has to greater than that of the maximal expected result*/
		int MSBA;
		/** The width in bits of the accumulator*/
		int sizeAcc_;

	private:
	};
}
#endif
