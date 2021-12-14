#ifndef _FIXSINCOS_H
#define _FIXSINCOS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <gmpxx.h>

#include "Operator.hpp"

#include "utils.hpp"


namespace flopoco{



	class FixSinCos: public Operator {
	public:	
		FixSinCos(OperatorPtr parentOp, Target * target, int lsb);
	
		~FixSinCos();

	

	
	
		void emulate(TestCase * tc);
	
		void buildStandardTestCases(TestCaseList * tcl);
	
		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();
	
		static TestList unitTest(int index);

	protected:
		int lsb; /** LSB of both input and output, each a signed number in [-1, 1) */
		mpfr_t scale;              /**< 1-2^(wOut-1)*/
		mpfr_t constPi;
	};

}
#endif // header guard

