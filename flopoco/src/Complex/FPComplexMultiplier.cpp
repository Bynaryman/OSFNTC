
#include <fstream>
#include <sstream>
#include "ComplexMultiplier.hpp"
#include "../TestBenches/FPNumber.hpp"


using namespace std;

namespace flopoco{

	extern vector<Operator *> oplist;



	ComplexMultiplier::ComplexMultiplier(Target* target, int wE, int wF, bool hasLessMultiplications)
		: Operator(target), wE(wE), wF(wF)
	{
		ostringstream o;
		o << "ComplexMultiplier_" << wE << "_" << wF;
		setNameWithFreqAndUID(o.str());

		addFPInput("Xi", wE, wF);
		addFPInput("Xr", wE, wF);
		addFPInput("Yi", wE, wF);
		addFPInput("Yr", wE, wF);
		addFPOutput("Zi", wE, wF);
		addFPOutput("Zr", wE, wF); 


		FPMult* mult = new FPMult(target, wE, wF, wE, wF, wE, wF);
		oplist.push_back(mult);
		FPAddSinglePath* add =  new FPAddSinglePath(target, wE, wF, wE, wF, wE, wF);
		oplist.push_back(add);
	
		if(!hasLessMultiplications){
			inPortMap (mult, "X", "Xi");
			inPortMap (mult, "Y", "Yi");
			outPortMap(mult, "R", "XiYi");
			vhdl << instance(mult, "MUL_XiYi");
			
			inPortMap (mult, "X", "Xr");
			inPortMap (mult, "Y", "Yr");
			outPortMap(mult, "R", "XrYr");
			vhdl << instance(mult, "MUL_XrYr");
			
			inPortMap (mult, "X", "Xr");
			inPortMap (mult, "Y", "Yi");
			outPortMap(mult, "R", "XrYi");
			vhdl << instance(mult, "MUL_XrYi");
			
			inPortMap (mult, "X", "Xi");
			inPortMap (mult, "Y", "Yr");
			outPortMap(mult, "R", "XiYr");
			vhdl << instance(mult, "MUL_XiYr");
			
			syncCycleFromSignal("XiYr", false);
			
			// invert the sign of XiYi to obtain a subtraction
			vhdl << tab << declare("neg_XiYi", wE+wF+3) << " <= XiYi;" << endl;
			vhdl << tab << "neg_XiYi(" << wE+wF << ") <= neg_XiYi( " << wE+wF << " ) xor \'1\';" << endl;
			
			syncCycleFromSignal("neg_XiYi", false);
			nextCycle();
			
			inPortMap (add, "X", "XrYr");
			inPortMap (add, "Y", "neg_XiYi");
			outPortMap(add, "R", "Zr", false);
			vhdl << instance(add, "ADD_XrYrMinXiYi");
			
			inPortMap (add, "X", "XrYi");
			inPortMap (add, "Y", "XiYr");
			outPortMap(add, "R", "Zi", false);
			vhdl << instance(add, "ADD_XrYiAddXiYr");
		}
		else{
			try{
			vhdl << tab << declare("negationMask", wE+wF+3) << " <= ( " << wE+wF << " => \'1\', others => \'0\');" << endl;
			
			inPortMap (add, "X", "Xr");
			inPortMap (add, "Y", "Xi");
			outPortMap(add, "R", "XrAddXi");
			vhdl << instance(add, "ADD_XrXi");
			
			vhdl << tab << declare("neg_Yr", wE+wF+3) << " <= Yr xor negationMask;" << endl;
			
			inPortMap (add, "X", "Yi");
			inPortMap (add, "Y", "neg_Yr");
			outPortMap(add, "R", "YiAddnegYr");
			vhdl << instance(add, "ADD_YiAddnegYr");
			
			inPortMap (add, "X", "Yi");
			inPortMap (add, "Y", "Yr");
			outPortMap(add, "R", "YrAddYi");
			vhdl << instance(add, "ADD_YrAddYi");
			
			syncCycleFromSignal("YrAddYi", false);
			//nextCycle(); 
			
			inPortMap (mult, "X", "Yr");
			inPortMap (mult, "Y", "XrAddXi");
			outPortMap(mult, "R", "K1");
			vhdl << instance(mult, "MUL_K1");
			
			inPortMap (mult, "X", "Xr");
			inPortMap (mult, "Y", "YiAddnegYr");
			outPortMap(mult, "R", "K2");
			vhdl << instance(mult, "MUL_K2");
			
			inPortMap (mult, "X", "Xi");
			inPortMap (mult, "Y", "YrAddYi");
			outPortMap(mult, "R", "K3");
			vhdl << instance(mult, "MUL_K3");
			
			syncCycleFromSignal("K3", false);
			//nextCycle(); 
			
			vhdl << tab << declare("neg_K3", wE+wF+3) << " <= K3 xor negationMask;" << endl;
			
			syncCycleFromSignal("neg_K3", false);
			nextCycle();
			
			inPortMap (add, "X", "K1");
			inPortMap (add, "Y", "neg_K3");
			outPortMap(add, "R", "Zr", false);
			vhdl << instance(add, "ADD_K1AddnegK3");
			
			inPortMap (add, "X", "K1");
			inPortMap (add, "Y", "K2");
			outPortMap(add, "R", "Zi", false);
			vhdl << instance(add, "ADD_K1AddK2");
			}catch(std::string str){
				cout << "execution interrupted: " << str << endl;
				exit(1);
			}
		}
	
	}	


	ComplexMultiplier::~ComplexMultiplier()
	{
	}
	
	
	void ComplexMultiplier::emulate(TestCase * tc)
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
		mpfr_mul(rr, xr, yr, GMP_RNDN);
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
		mpfr_mul(ri, xi, yi, GMP_RNDN);
		/* Set outputs for imaginary part */
		FPNumber  fpri(wE, wF, ri);
		mpz_class svRi = fpri.getSignalValue();
		tc->addExpectedOutput("Zi", svRi);

		// clean up
		mpfr_clears(xr, yr, rr, xi, yi, ri, NULL);
	}
	
	
	void ComplexMultiplier::buildStandardTestCases(TestCaseList* tcl){
	
	}
	
	
	TestCase* ComplexMultiplier::buildRandomTestCase(int i){
		TestCase *tc;
		mpz_class xr,yr, xi, yi;
		
		tc = new TestCase(this);
		
		//fully random
		xr = getLargeRandom(wE+wF+3);
		yr = getLargeRandom(wE+wF+3);
		
		xi = getLargeRandom(wE+wF+3);
		yi = getLargeRandom(wE+wF+3);
		
		emulate(tc);
		return tc;
	}

}
