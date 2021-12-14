#ifndef FPCONSTDIV_HPP
#define FPCONSTDIV_HPP
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "Operator.hpp"
#include "IntConstDiv.hpp"


namespace flopoco{

	class FPConstDiv : public Operator
	{
	public:
		/** @brief The generic constructor */
		FPConstDiv(Target* target, int wEIn, int wFIn, int wEOut, int wFOut, int d, int dExp=0, int alpha=-1, int arch=0);


		~FPConstDiv();

		int wEIn;
		int wFIn;
		int wEOut;
		int wFOut;




		void emulate(TestCase *tc);
		void buildStandardTestCases(TestCaseList* tcl);


		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();


	private:
		int d; /**< The operator divides by d.2^dExp */
		int dExp;  /**< The operator divides by d.2^dExp */
		int alpha;
		IntConstDiv *icd;
		bool mantissaIsOne;
		double dd; // the value of the actual constant in double: equal to d*2^dExp
		/// \todo replace the above with the mpd that we have in emulate
	};

}
#endif
