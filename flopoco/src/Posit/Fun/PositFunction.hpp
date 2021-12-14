#ifndef _POSITFUNCTION_HPP_
#define _POSITFUNCTION_HPP_

#include <string>
#include <iostream>

#include <sollya.h>
#include <gmpxx.h>
#include "../../TestBenches/TestCase.hpp"
#include "../../TestBenches/PositNumber.hpp"

using namespace std;

/* Stylistic convention here: all the sollya_obj_t have names that end with a capital S */
namespace flopoco{
	
	/** The PositFunction objects does mystical things I do not fully understand. 
			It provides an interface to Sollya services such as 
			parsing it,
			evaluating it at arbitrary precision,
			providing a polynomial approximation on an interval
	*/

	class PositFunction {
	public:


		/**
			 The PositFunctionByTable constructor
			 @param[string] func    a string representing the function
			 @param[int]    lsbX    input LSB weight (-lsbX is the input size)
			 @param[int]    msbOut  output MSB weight, used to determine wOut
			 @param[int]    lsbOut  output LSB weight
			 @param[bool]   signedIn: if true, input range is [0,1], else input range is [0,1]

			 One could argue that MSB weight is redundant, as it can be deduced from an analysis of the function. 
			 This would require quite a lot of work for non-trivial functions (isolating roots of the derivative etc).
			 So this is currently left to the user.
			 There are defaults for lsbOut and msbOut for situations when they are computed afterwards.
		 */
		PositFunction(string sollyaString, int width, int wES);
	  
		PositFunction(sollya_obj_t fS);

		virtual ~PositFunction();

		string getDescription() const;


		void eval(mpz_class x, mpz_class &r) const;

		void emulate(TestCase * tc);

		// All the following public, not good practice I know, but life is complicated enough
		string sollyaString;
		int width;   
		int wES;
	        int wOut;
	        int wIn;
		string description;
		sollya_obj_t fS;
		sollya_obj_t rangeS;
	};

}
#endif // _FIXFUNCTION_HH_
