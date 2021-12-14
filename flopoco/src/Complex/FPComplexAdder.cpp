
#include <fstream>
#include <sstream>
#include "ComplexAdder.hpp"
#include "../TestBenches/FPNumber.hpp"


using namespace std;

namespace flopoco{

	extern vector<Operator *> oplist;



	ComplexAdder::ComplexAdder(Target* target, int wE, int wF)
		: Operator(target), wE(wE), wF(wF)
	{
		ostringstream o;
		o << "ComplexAdder_" << wE << "_" << wF;
		setNameWithFreqAndUID(o.str());

		addFPInput("Xi", wE, wF);
		addFPInput("Xr", wE, wF);
		addFPInput("Yi", wE, wF);
		addFPInput("Yr", wE, wF);
		addFPOutput("Zi", wE, wF);
		addFPOutput("Zr", wE, wF); 


		FPAddSinglePath* add =  new FPAddSinglePath(target, wE, wF, wE, wF, wE, wF);
		oplist.push_back(add);
	
		inPortMap (add, "X", "Xi");
		inPortMap (add, "Y", "Yi");
		outPortMap(add, "R", "Zi", false);
		vhdl << instance(add, "ADD_I");
		
		inPortMap (add, "X", "Xr");
		inPortMap (add, "Y", "Yr");
		outPortMap(add, "R", "Zr", false);
		vhdl << instance(add, "ADD_R");
	
	}	


	ComplexAdder::~ComplexAdder()
	{
	}
	
	
	void ComplexAdder::emulate(TestCase * tc)
	{
		/* Get I/O values - both real and imaginary */
		mpz_class svXr = tc->getInputValue("Xr");
		mpz_class svXi = tc->getInputValue("Xi");
		mpz_class svYr = tc->getInputValue("Yr");
		mpz_class svYi = tc->getInputValue("Yi");
	
		/* Verify real part */
		/* Compute correct value for real part */
		FPNumber fpxr(wE, wF), fpyr(wE, wF);
		fpxr = svXr;
		fpyr = svYr;
		mpfr_t xr, yr, rr;
		mpfr_init2(xr, 1+wF);
		mpfr_init2(yr, 1+wF);
		mpfr_init2(rr, 1+wF); 
		fpxr.getMPFR(xr);
		fpyr.getMPFR(yr);
		mpfr_add(rr, xr, yr, GMP_RNDN);
		/* Set outputs for real part */
		FPNumber  fprr(wE, wF, rr);
		mpz_class svRr = fprr.getSignalValue();
		tc->addExpectedOutput("Zr", svRr);
		
		/* Verify imaginary part */
		/* Compute correct value for imaginary part */
		FPNumber fpxi(wE, wF), fpyi(wE, wF);
		fpxi = svXi;
		fpyi = svYi;
		mpfr_t xi, yi, ri;
		mpfr_init2(xi, 1+wF);
		mpfr_init2(yi, 1+wF);
		mpfr_init2(ri, 1+wF); 
		fpxi.getMPFR(xi);
		fpyi.getMPFR(yi);
		mpfr_add(ri, xi, yi, GMP_RNDN);
		/* Set outputs for imaginary part */
		FPNumber  fpri(wE, wF, ri);
		mpz_class svRi = fpri.getSignalValue();
		tc->addExpectedOutput("Zi", svRi);

		// clean up
		mpfr_clears(xr, yr, rr, xi, yi, ri, NULL);
	}
	
	
	void ComplexAdder::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;

		// Regression tests 
		tc = new TestCase(this); 
		tc->addFPInput("Xr", 1.0);
		tc->addFPInput("Yr", -1.0);
		tc->addFPInput("Xi", 1.0);
		tc->addFPInput("Yi", -1.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addFPInput("Xr", 1.0);
		tc->addFPInput("Yr", FPNumber::plusDirtyZero);
		tc->addFPInput("Xi", 1.0);
		tc->addFPInput("Yi", FPNumber::plusDirtyZero);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addFPInput("Xr", 1.0);
		tc->addFPInput("Yr", FPNumber::minusDirtyZero);
		tc->addFPInput("Xi", 1.0);
		tc->addFPInput("Yi", FPNumber::minusDirtyZero);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addFPInput("Xr", FPNumber::plusInfty);
		tc->addFPInput("Yr", FPNumber::minusInfty);
		tc->addFPInput("Xi", FPNumber::plusInfty);
		tc->addFPInput("Yi", FPNumber::minusInfty);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addFPInput("Xr", FPNumber::plusInfty);
		tc->addFPInput("Yr", FPNumber::plusInfty);
		tc->addFPInput("Xi", FPNumber::plusInfty);
		tc->addFPInput("Yi", FPNumber::plusInfty);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addFPInput("Xr", FPNumber::minusInfty);
		tc->addFPInput("Yr", FPNumber::minusInfty);
		tc->addFPInput("Xi", FPNumber::minusInfty);
		tc->addFPInput("Yi", FPNumber::minusInfty);
		emulate(tc);
		tcl->add(tc);
	
	}	
	
	TestCase* ComplexAdder::buildRandomTestCase(int i){

		TestCase *tc;
		mpz_class xr,yr, xi, yi;
		mpz_class normalExn = mpz_class(1)<<(wE+wF+1);
		mpz_class negative  = mpz_class(1)<<(wE+wF);

		tc = new TestCase(this); 
		/* Fill inputs */
		if ((i & 7) == 0) {// cancellation, same exponent
			mpz_class e = getLargeRandom(wE);
			xr  = getLargeRandom(wF) + (e << wF) + normalExn;
			yr  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
			
			e = getLargeRandom(wE);
			xi  = getLargeRandom(wF) + (e << wF) + normalExn;
			yi  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
		}
		else if ((i & 7) == 1) {// cancellation, exp diff=1
			mpz_class e = getLargeRandom(wE);
			xr  = getLargeRandom(wF) + (e << wF) + normalExn;
			e++; // may rarely lead to an overflow, who cares
			yr  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
			
			e = getLargeRandom(wE);
			xi  = getLargeRandom(wF) + (e << wF) + normalExn;
			e++; // may rarely lead to an overflow, who cares
			yi  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
		}
		else if ((i & 7) == 2) {// cancellation, exp diff=1
			mpz_class e = getLargeRandom(wE);
			xr  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
			e++; // may rarely lead to an overflow, who cares
			yr  = getLargeRandom(wF) + (e << wF) + normalExn;
			
			e = getLargeRandom(wE);
			xi  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
			e++; // may rarely lead to an overflow, who cares
			yi  = getLargeRandom(wF) + (e << wF) + normalExn;
		}
		else if ((i & 7) == 3) {// alignment within the mantissa sizes
			mpz_class e = getLargeRandom(wE);
			xr  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
			e +=	getLargeRandom(intlog2(wF)); // may lead to an overflow, who cares
			yr  = getLargeRandom(wF) + (e << wF) + normalExn;
			
			e = getLargeRandom(wE);
			xi  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
			e +=	getLargeRandom(intlog2(wF)); // may lead to an overflow, who cares
			yi  = getLargeRandom(wF) + (e << wF) + normalExn;
		}
		else if ((i & 7) == 4) {// subtraction, alignment within the mantissa sizes
			mpz_class e = getLargeRandom(wE);
			xr  = getLargeRandom(wF) + (e << wF) + normalExn;
			e +=	getLargeRandom(intlog2(wF)); // may lead to an overflow
			yr  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
			
			e = getLargeRandom(wE);
			xi  = getLargeRandom(wF) + (e << wF) + normalExn;
			e +=	getLargeRandom(intlog2(wF)); // may lead to an overflow
			yi  = getLargeRandom(wF) + (e << wF) + normalExn + negative;
		}
		else if ((i & 7) == 5 || (i & 7) == 6) {// addition, alignment within the mantissa sizes
			mpz_class e = getLargeRandom(wE);
			xr  = getLargeRandom(wF) + (e << wF) + normalExn;
			e +=	getLargeRandom(intlog2(wF)); // may lead to an overflow
			yr  = getLargeRandom(wF) + (e << wF) + normalExn;
			
			e = getLargeRandom(wE);
			xi  = getLargeRandom(wF) + (e << wF) + normalExn;
			e +=	getLargeRandom(intlog2(wF)); // may lead to an overflow
			yi  = getLargeRandom(wF) + (e << wF) + normalExn;
		}
		else{ //fully random
			xr = getLargeRandom(wE+wF+3);
			yr = getLargeRandom(wE+wF+3);
			
			xi = getLargeRandom(wE+wF+3);
			yi = getLargeRandom(wE+wF+3);
		}
		// Random swap
		mpz_class swap = getLargeRandom(1);
		if (swap == mpz_class(0)) {
			tc->addInput("Xr", xr);
			tc->addInput("Yr", yr);
			
			tc->addInput("Xi", xi);
			tc->addInput("Yi", yi);
		}
		else {
			tc->addInput("Xr", yr);
			tc->addInput("Yr", xr);
			
			tc->addInput("Xi", yi);
			tc->addInput("Yi", xi);
		}
		/* Get correct outputs */
		emulate(tc);
		return tc;
	}

}
