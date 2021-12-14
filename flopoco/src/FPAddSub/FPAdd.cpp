#include "FPAdd.hpp"
#include <Operator.hpp>


using namespace std;
namespace flopoco{

	void FPAdd::emulate(TestCase * tc, int wE, int wF, bool subtract)
	{
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");
		mpz_class svY = tc->getInputValue("Y");

		/* Compute correct value */
		FPNumber fpx(wE, wF, svX);
		FPNumber fpy(wE, wF, svY);
		mpfr_t x, y, r;
		mpfr_init2(x, 1+wF);
		mpfr_init2(y, 1+wF);
		mpfr_init2(r, 1+wF);
		fpx.getMPFR(x);
		fpy.getMPFR(y);
		if(subtract)
			mpfr_sub(r, x, y, GMP_RNDN);
		else
			mpfr_add(r, x, y, GMP_RNDN);

		// Set outputs
		FPNumber  fpr(wE, wF, r);
		mpz_class svR = fpr.getSignalValue();
		tc->addExpectedOutput("R", svR);

		// clean up
		mpfr_clears(x, y, r, NULL);
	}

	void FPAdd::buildStandardTestCases(Operator* op, int wE, int wF, TestCaseList* tcl, bool onlyPositiveIO){
		// Although standard test cases may be architecture-specific, it can't hurt to factor them here.
		TestCase *tc;

		if(!onlyPositiveIO)
		{
			// Regression tests
			tc = new TestCase(op);
			tc->addFPInput("X", 1.0);
			tc->addFPInput("Y", -1.0);
			op->emulate(tc);
			tcl->add(tc);

			tc = new TestCase(op);
			tc->addFPInput("X", 2.0);
			tc->addFPInput("Y", -2.0);
			op->emulate(tc);
			tcl->add(tc);

			tc = new TestCase(op);
			tc->addFPInput("X", 1.0);
			tc->addFPInput("Y", FPNumber::minusDirtyZero);
			op->emulate(tc);
			tcl->add(tc);

			tc = new TestCase(op);
			tc->addFPInput("X", FPNumber::plusInfty);
			tc->addFPInput("Y", FPNumber::minusInfty);
			op->emulate(tc);
			tcl->add(tc);

			tc = new TestCase(op);
			tc->addFPInput("X", FPNumber::minusInfty);
			tc->addFPInput("Y", FPNumber::minusInfty);
			op->emulate(tc);
			tcl->add(tc);

			tc = new TestCase(op);
			tc->addFPInput("X", -4.375e1);
			tc->addFPInput("Y", 4.375e1);
			op->emulate(tc);
			tcl->add(tc);

			// A regression test that breaks (6,10)
			tc = new TestCase(op);
			tc->addFPInput("X", 1.0081e6);
			tc->addFPInput("Y", -2.1475e9);
			op->emulate(tc);
			tcl->add(tc);

			// A generalization of this regression test.
			// This bug has been here since the beginning.
			tc = new TestCase(op);
			tc->addFPInput("X", 1);
			tc->addFPInput("Y", -exp2(-wF-2)-exp2(-wF-3));
			op->emulate(tc);
			tcl->add(tc);
		}

		tc = new TestCase(op);
		tc->addFPInput("X", 1.0);
		tc->addFPInput("Y", FPNumber::plusDirtyZero);
		op->emulate(tc);
		tcl->add(tc);

		tc = new TestCase(op);
		tc->addFPInput("X", FPNumber::plusInfty);
		tc->addFPInput("Y", FPNumber::plusInfty);
		op->emulate(tc);
		tcl->add(tc);
	}




	TestCase* FPAdd::buildRandomTestCase(Operator* op, int i, int wE, int wF, bool subtract, bool onlyPositiveIO){
		TestCase *tc;
		mpz_class x,y;
		mpz_class normalExn = mpz_class(1)<<(wE+wF+1);
		mpz_class negative  = mpz_class(1)<<(wE+wF);

		tc = new TestCase(op);
		/* Fill inputs */
		if ((i & 7) == 0) {// cancellation, same exponent
			mpz_class e = getLargeRandom(wE);
			x  = getLargeRandom(wF) + (e << wF) + normalExn;
			y  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
		}
		else if ((i & 7) == 1) {// cancellation, exp diff=1
			mpz_class e = getLargeRandom(wE);
			x  = getLargeRandom(wF) + (e << wF) + normalExn;
			e++; // may rarely lead to an overflow, who cares
			y  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
		}
		else if ((i & 7) == 2) {// cancellation, exp diff=1
			mpz_class e = getLargeRandom(wE);
			x  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
			e++; // may rarely lead to an overflow, who cares
			y  = getLargeRandom(wF) + (e << wF) + normalExn;
		}
		else if ((i & 7) == 3) {// alignment within the mantissa sizes
			mpz_class e = getLargeRandom(wE);
			x  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
			e +=	getLargeRandom(intlog2(wF)); // may lead to an overflow, who cares
			y  = getLargeRandom(wF) + (e << wF) + normalExn;
		}
		else if ((i & 7) == 4) {// subtraction, alignment within the mantissa sizes
			mpz_class e = getLargeRandom(wE);
			x  = getLargeRandom(wF) + (e << wF) + normalExn;
			e +=	getLargeRandom(intlog2(wF)); // may lead to an overflow
			y  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
		}
		else if ((i & 7) == 5 || (i & 7) == 6) {// addition, alignment within the mantissa sizes
			mpz_class e = getLargeRandom(wE);
			x  = getLargeRandom(wF) + (e << wF) + normalExn;
			e +=	getLargeRandom(intlog2(wF)); // may lead to an overflow
			y  = getLargeRandom(wF) + (e << wF) + normalExn;
		}
		else{ //fully random
			//now, really fully random positive and negative and non-normal numbers
			x = getLargeRandom(wE+wF+3);
			y = getLargeRandom(wE+wF+3);
		}

		if(onlyPositiveIO)
		{
			x = x & ~negative;
			y = y & ~negative;

			if(((mpz_class) (x & negative)))
				{
					throw string("Input X is negative!");
				}
			if(((mpz_class) (y & negative)))
				{
					throw string("Input Y is negative!");
				}
		}

		// Random swap
		mpz_class swap = getLargeRandom(1);
		if (swap == mpz_class(0)) {
			tc->addInput("X", x);
			tc->addInput("Y", y);
		}
		else {
			tc->addInput("X", y);
			tc->addInput("Y", x);
		}

		/* Get correct outputs */
		emulate(tc,wE, wF, subtract);
		return tc;
	}


	
	OperatorPtr FPAdd::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int wE, wF;
		bool sub, dualPath, onlyPositiveIO;
		UserInterface::parseStrictlyPositiveInt(args, "wE", &wE); 
		UserInterface::parseStrictlyPositiveInt(args, "wF", &wF);
		UserInterface::parseBoolean(args, "sub", &sub);
		UserInterface::parseBoolean(args, "dualPath", &dualPath);
		UserInterface::parseBoolean(args, "onlyPositiveIO", &onlyPositiveIO);

		if(onlyPositiveIO && !dualPath)
		{
			throw string("Sorry, onlyPositiveIO is only possible for the dualPath algorithm");
		}

		if(dualPath)
			return new FPAddDualPath(parentOp, target, wE, wF, sub, onlyPositiveIO);
		else
			return new FPAddSinglePath(parentOp, target, wE, wF, sub);
	}

	TestList FPAdd::unitTest(int index)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		
		if(index==-1) 
		{ // The unit tests

			for(int wF=5; wF<53; wF+=1) {
				for(int dualPath = 0; dualPath <2; dualPath++)	{
					for(int sub = 0; sub <2; sub++)	{
						int wE = 6+(wF/10);
						while(wE>wF)
							wE -= 2;
					
						paramList.push_back(make_pair("wF",to_string(wF)));
						paramList.push_back(make_pair("wE",to_string(wE)));
						paramList.push_back(make_pair("sub",to_string(sub)));
						paramList.push_back(make_pair("dualPath",to_string(dualPath)));
						testStateList.push_back(paramList);
						paramList.clear();
					}
					
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

	void FPAdd::registerFactory(){
		UserInterface::add("FPAdd", // name
			"A correctly rounded floating-point adder.",
			"BasicFloatingPoint",
			"", //seeAlso
			"wE(int): exponent size in bits; \
			wF(int): mantissa size in bits; \
			sub(bool)=false: implement a floating-point subtractor instead of an adder;\
			dualPath(bool)=false: use a dual-path algorithm, more expensive but shorter latency;\
			onlyPositiveIO(bool)=false: optimize for only positive input and output numbers;",
			"Single-path is lower hardware, longer latency than dual-path.<br> The difference between single-path and dual-path is well explained in textbooks such as Ercegovac and Lang's <em>Digital Arithmetic</em>, or Muller et al's <em>Handbook of floating-point arithmetic.</em>",
			FPAdd::parseArguments,
			FPAdd::unitTest
			) ;
	}


}
