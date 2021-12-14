#ifndef FP2Fix_HPP
#define FP2Fix_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "Operator.hpp"
#include "TestBenches/FPNumber.hpp"


namespace flopoco{

   /** The FP2Fix class */
   class FP2Fix : public Operator
   {
   public:
		/**
		 * @brief The  constructor
		 * @param[in]		target		the target device
		 * @param[in]		MSB			the MSB of the output number for the conversion result
		 * @param[in]		LSB			the LSB of the output number for the conversion result
		 * @param[in]		wEI			the with of the exponent in input
		 * @param[in]		wFI			the with of the fraction in input
		 * @param[in]		trunc_p			the output is not rounded when trunc_p is true
		 */
		 FP2Fix(Operator* parentOp, Target* target, bool SignedO, int MSBO, int LSBO,  int wEI, int wFI, bool trunc_p);

		/**
		 * @brief destructor
		 */
	  ~FP2Fix();


	  void emulate(TestCase * tc);
	  void buildStandardTestCases(TestCaseList* tcl);
	  /* Overloading the Operator method to limit testing of negative numbers when Signed is 0*/
	  TestCase* buildRandomTestCase(int i);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();

   private:

	  /** The width of the exponent for the input */
	  int wEI;
	  /** The width of the fraction for the input */
	  int wFI;
	  /** are all numbers positive or not */
	  bool Signed;
	  /** The MSB for the output */
	  int MSBO;
	  /** The LSB for the output */
	  int LSBO;
	  bool trunc_p;
	  /** when true the output is not rounded */


   };
}
#endif
