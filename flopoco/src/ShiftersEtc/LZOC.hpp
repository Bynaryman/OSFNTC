#ifndef LZOC_HPP
#define LZOC_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "Operator.hpp"


namespace flopoco{

	/** The Leading zero/one counter class.  
	 * Recursive structure with intlog2(wIn) stages: 
	 if wIn=2^n then the output has n+1 bits 


	 If wIn is a power of two, we may count up to 
	 */
	class LZOC : public Operator{

	public:
		/** The LZOC constructor
		 * @param[in] target the target device for this operator
		 * @param[in] wIn the width of the input
		 */
		LZOC(OperatorPtr parentOp, Target* target, int wIn, int countType=-1);
	
		/** The LZOC destructor	*/
		~LZOC();




		/**
		 * Emulate a correctly rounded division using MPFR.
		 * @param tc a TestCase partially filled with input values 
		 */
		void emulate(TestCase * tc);
	
	protected:

		int wIn;    /**< The width of the input */
		int wOut;   /**< The width of the output */
		int p2wOut; /**< The value of 2^wOut, which is computed as 1<<wOut */
		int countType;  /**< 0: count zeroes; 1: count 1s; -1: have an input that tells what to count */

	public:
		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();
	};

}
#endif
