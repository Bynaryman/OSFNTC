
// TODOs

// One optim for 24 bits would be to compute z² for free by a table using the second unused port of the blockram
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Operator.hpp"
#include "FixSinCosPoly.hpp"
#include "FixSinCosCORDIC.hpp"
#include "FixSinCos.hpp"

#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers */
#include "mpfr.h"


using namespace std;
namespace flopoco{


#define USEBITHEAP 1
#define LARGE_PREC 1000 // 1000 bits should be enough for everybody
#define LOW_PREC 0





	////////////////////////////////////// FixSinCos ///////////////////

	FixSinCos::FixSinCos(OperatorPtr parentOp, Target * target, int lsb_):Operator(parentOp,target), lsb(lsb_){
		//compute the scale factor		
		mpfr_init2(scale, -lsb+1);
		mpfr_set_d(scale, -1.0, GMP_RNDN);           // exact
		mpfr_mul_2si(scale, scale, lsb, GMP_RNDN); // exact
		mpfr_add_d(scale, scale, 1.0, GMP_RNDN);     // exact
		//REPORT(DEBUG, "scale=" << printMPFR(scale, 15));

		// everybody needs many digits of Pi
		mpfr_init2(constPi, -10*lsb);
		mpfr_const_pi( constPi, GMP_RNDN);

		// declaring inputs and outputs
		addInput("X", -lsb+1);
		addOutput("S", -lsb+1, 2);
		addOutput("C", -lsb+1, 2);
	};




	FixSinCos::~FixSinCos() {
	};






	void FixSinCos::emulate(TestCase * tc)
	{
		mpfr_t z, rsin, rcos;
		mpz_class sin_z, cos_z;
		mpfr_init2(z, -10*lsb);
		mpfr_init2(rsin, -10*lsb); 
		mpfr_init2(rcos, -10*lsb); 
		

		/* Get I/O values */
		mpz_class svZ = tc->getInputValue("X");
		
		/* Compute correct value */
		
		mpfr_set_z (z, svZ.get_mpz_t(), GMP_RNDN); //  exact
		mpfr_mul_2si (z, z, lsb, GMP_RNDN); // exact
	
		// No need to manage sign bit etc: modulo 2pi is the same as modulo 2 in the initial format
		mpfr_mul(z, z, constPi, GMP_RNDN);

		mpfr_sin(rsin, z, GMP_RNDN); 
		mpfr_cos(rcos, z, GMP_RNDN);
		mpfr_mul(rsin, rsin, scale, GMP_RNDN);
		mpfr_mul(rcos, rcos, scale, GMP_RNDN);

		mpfr_add_d(rsin, rsin, 6.0, GMP_RNDN); // exact rnd here
		mpfr_add_d(rcos, rcos, 6.0, GMP_RNDN); // exact rnd here
		mpfr_mul_2si (rsin, rsin, -lsb, GMP_RNDN); // exact rnd here
		mpfr_mul_2si (rcos, rcos, -lsb, GMP_RNDN); // exact rnd here

		// Rounding down
		mpfr_get_z (sin_z.get_mpz_t(), rsin, GMP_RNDD); // there can be a real rounding here
		mpfr_get_z (cos_z.get_mpz_t(), rcos, GMP_RNDD); // there can be a real rounding here
		sin_z -= mpz_class(6)<<(-lsb);
		cos_z -= mpz_class(6)<<(-lsb);

		tc->addExpectedOutput ("S", sin_z);
		tc->addExpectedOutput ("C", cos_z);

		// Rounding up
		mpfr_get_z (sin_z.get_mpz_t(), rsin, GMP_RNDU); // there can be a real rounding here
		mpfr_get_z (cos_z.get_mpz_t(), rcos, GMP_RNDU); // there can be a real rounding here
		sin_z -= mpz_class(6)<<(-lsb);
		cos_z -= mpz_class(6)<<(-lsb);

		tc->addExpectedOutput ("S", sin_z);
		tc->addExpectedOutput ("C", cos_z);
		
		// clean up
		mpfr_clears (z, rsin, rcos, NULL);		
	}


	void FixSinCos::buildStandardTestCases(TestCaseList * tcl)
	{
		mpfr_t z;
		mpz_class zz;
		TestCase* tc;

		// No regression tests below, just values used in the initial development.
		mpfr_init2(z, -10*lsb);

		//z=0
		tc = new TestCase (this);
		tc -> addInput ("X", mpz_class(0));
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase (this);
		tc->addComment("Pi/4-eps");
		mpfr_set_d (z, 0.24, GMP_RNDD); 
		mpfr_mul_2si (z, z, -lsb, GMP_RNDD); 
		mpfr_get_z (zz.get_mpz_t(), z, GMP_RNDD);  
		tc -> addInput ("X", zz);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase (this);
		tc->addComment("Pi/6");
		mpfr_set_d (z, 0.166666666666666666666666666666666, GMP_RNDD); 
		mpfr_mul_2si (z, z, -lsb, GMP_RNDD); 
		mpfr_get_z (zz.get_mpz_t(), z, GMP_RNDD);  
		tc -> addInput ("X", zz);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase (this);
		tc->addComment("Pi/3");
		mpfr_set_d (z, 0.333333333333333333333333333333, GMP_RNDD); 
		mpfr_mul_2si (z, z, -lsb, GMP_RNDD); 
		mpfr_get_z (zz.get_mpz_t(), z, GMP_RNDD);  
		tc -> addInput ("X", zz);
		emulate(tc);
		tcl->add(tc);

		mpfr_clears (z, NULL);
	}




	TestList FixSinCos::unitTest(int index)	{
		TestList testStateList;
		vector<pair<string,string>> paramList;
		
		if(index==-1) 
		{ // The unit tests
			for(int method=0; method<=2; method++) {
				for(int w=5; w<=32; w++) {
					paramList.push_back(make_pair("lsb",to_string(-w)));
					paramList.push_back(make_pair("method",to_string(method)));
					if(w<17)
						paramList.push_back(make_pair("TestBench n=","-2"));			
					testStateList.push_back(paramList);
					paramList.clear();
				}
			}
		}
		else     
		{
				// finite number of random test computed out of index
			// TODO
		}	

		return testStateList;
	}


		
	OperatorPtr FixSinCos::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int lsb;
		int method;
		// TODO test if wIn!=wOut works for CORDIC
		UserInterface::parseInt(args, "lsb", &lsb); 
		UserInterface::parseInt(args, "method", &method);
		if(method==0)
			return new FixSinCosPoly(parentOp, target, lsb);
		else if (method==1)
			return new FixSinCosCORDIC(parentOp, target, -lsb+1, -lsb+1, 0);  // TODO we want to expose the constructor parameters in the new  interface, so these "-" are a bug
		else if (method==2)
			return new FixSinCosCORDIC(parentOp, target, -lsb+1, -lsb+1, 1);  // reduced iteration
		else {
			cerr << " ERROR in FixSinCos::parseArguments: Wrong method number, only 0 (polynomial), 1 (standard CORDIC) and 2 (reduced iteration CORDIC) are allowed";
			return NULL;
		}
	}


	void FixSinCos::registerFactory(){
		UserInterface::add("FixSinCos", // name
											 "Computes (1-2^(-w)) sin(pi*x) and (1-2^(-w)) cos(pi*x) for x in -[1,1[, using tables and multipliers.",
											 "ElementaryFunctions",
											 "", // seeAlso
											 "lsb(int): weight of the LSB of the input and outputs; \
                        method(int)=0: 0 for table- and mult-based, 1 for traditional CORDIC, 2 for reduced-iteration CORDIC",
											 "For a fixed-point 2's complement input x in [-1,1[, evaluates (1-2^(lsbIn))*{sin,cos}(pi*x). <br>For more details, see <a href=\"bib/flopoco.html#DinIstSer2013-HEART-SinCos\">this article</a>.",
											 FixSinCos::parseArguments,
											 FixSinCos::unitTest
											 ) ;
	
	}
}
