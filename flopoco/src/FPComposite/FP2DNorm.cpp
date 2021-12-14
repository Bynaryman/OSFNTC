#include <iostream>
#include <sstream>
#include "gmp.h"
#include "mpfr.h"

#include "FP2DNorm.hpp"

#define USE_SQUARER 1

#include "ShiftersEtc/Shifters.hpp"
#include "IntAddSubCmp/IntAdder.hpp"
#if USE_SQUARER
#include "IntMult/IntSquarer.hpp"
#else
#include "IntMult/IntMultiplier.hpp"
#endif

using namespace std;

namespace flopoco {

	FP2DNorm::FP2DNorm(Target* target, int wE_, int wF_) : Operator(target), wE(wE_), wF(wF_) {
		srcFileName="FP2DNorm";
					
		ostringstream name;
		name<<"FP2DNorm_"<<wE<<"_"<<wF<<"_uid"<<getNewUId(); 
		setNameWithFreqAndUID(name.str()); 
	
		setCopyrightString("Florent de Dinechin (2013)");		
	
		addFPInput("X", wE, wF);
		addFPInput("Y", wE, wF);
		addFPOutput("R", wE, wF, 2);




			// guard bits for a faithful result
			int g=3; 

			// The exponent datapath

			//---------------------------------------------------------------------
			vhdl << tab << "-- Extract biased exponents, and mantissa fractions" << endl;
			vhdl << tab << declare("EX", wE) << " <=  X" << range(wE+wF-1, wF) << ";" << endl;
			vhdl << tab << declare("EY", wE) << " <=  Y" << range(wE+wF-1, wF) << ";" << endl;
			vhdl << tab << declare("FX", wF) << " <=  X" << range(wF-1, 0) << ";" << endl;
			vhdl << tab << declare("FY", wF) << " <=  Y" << range(wF-1, 0) << ";" << endl;
		
			// determine the max of the exponents
			vhdl << tab << declare("DEXY", wE+1) << " <=   ('0' & EX) - ('0' & EY);" << endl;
			vhdl << tab << declare("XltY") << " <=   DEXY("<< wE<<");" << endl;
			vhdl << tab << declare("DEYX", wE+1) << " <=   ('0' & EY) - ('0' & EX);" << endl;
		
			vhdl << tab << "-- Possibly swap X and Y, renamed to A and B so that EA >= EB" << endl;
		
			vhdl << tab << declare("EA", wE)  << " <=    -- exponent of the larger, also tentative exponent of the result" << endl
				  << tab << tab << "EY when (XltY='1')   else EX; " << endl;
			vhdl << tab << declare("FA", wF+1)  << " <=    -- mantissa fraction of the larger input" << endl
				  << tab << tab << "'1' & FY when (XltY='1')   else '1' & FX; " << endl;
			vhdl << tab << declare("FB", wF+1)  << " <=    -- mantissa fraction of the smaller input" << endl
				  << tab << tab << "'1' & FX when (XltY='1')   else '1' & FY; " << endl;
		
			vhdl << tab << declare("expDiff", wE)  << " <=  DEYX" << range(wE-2,0) << " when (XltY='1') else DEXY" << range(wE-2,0) << "; " << endl;

			int maxShift = (wF+1+1)/2+g;
			int sizeRightShift = intlog2(maxShift);
			REPORT(DETAILED, "maxShift="<< maxShift << "  sizeRightShift=" << sizeRightShift);

			vhdl << endl << tab << "-- Shift FB before squaring: this way, max shift is only (wF+2)/2+g = "<<maxShift << endl;


			vhdl<<tab<<declare("shiftedOut") << " <= '1' when (expDiff >= "<< maxShift<<") else '0';"<<endl;
			if (wE>sizeRightShift) {
				vhdl<<tab<<declare("shiftVal",sizeRightShift) << " <= expDiff("<< sizeRightShift-1<<" downto 0)"
				    << " when shiftedOut='0' else CONV_STD_LOGIC_VECTOR("<<maxShift<<","<<sizeRightShift<<") ;" << endl; 
			}		
			else {
				THROWERROR("Case wE<=sizeRightShift not implemented. Go complain.");
			}

			Shifter * rightShifter = new Shifter(target, wF+1, maxShift, Shifter::Right);
			rightShifter->changeName(getName()+"_RightShifter");
			addSubComponent(rightShifter);
			inPortMap  (rightShifter, "X", "FB");
			inPortMap  (rightShifter, "S", "shiftVal");
			outPortMap (rightShifter, "R","shiftedFB0");
			vhdl << instance(rightShifter, "RightShifterComponent");

			int shiftedFB0Size = getSignalByName("shiftedFB0")->width();

			vhdl<<tab<<declare("shiftedFB", 1+wF+g) << " <= shiftedFB0" << range(shiftedFB0Size-1, shiftedFB0Size-(1+wF+g)) << ";"<<endl;

			vhdl << endl << tab << "-- Now square A and B" << endl;

			Operator* squarerA;
			Operator* squarerB;
#if  USE_SQUARER
			squarerA = new IntSquarer(target,  1+ wF);
			squarerB = new IntSquarer(target,  1+ wF+g);
#else
			squarerA = new IntMultiplier(target, 1+ wF, 1+ wF);
			squarerB = new IntMultiplier(target, 1+ wF, 1+ wF);
#endif
			squarerA->changeName(getName()+"_squarerA");
			addSubComponent(squarerA);
			inPortMap  (squarerA, "X", "FA");
			outPortMap (squarerA, "R","FAsquare");
			vhdl << instance(squarerA, "FA_squarer");
			
			squarerB->changeName(getName()+"_squarerB");
			addSubComponent(squarerB);
			inPortMap  (squarerB, "X", "shiftedFB");
			outPortMap (squarerB, "R","FBsquare");
			vhdl << instance(squarerB, "FB_squarer");
			

			int FASquareSize = getSignalByName("FAsquare")->width();
			vhdl<<tab<<declare("FAsquareTrunc", 2+wF+g) << " <= '0' & FASquare" << range(FASquareSize-1, FASquareSize-(1+wF+g)) << ";"<<endl;
			int FBSquareSize = getSignalByName("FBsquare")->width();
			vhdl<<tab<<declare("FBsquareTrunc", 2+wF+g) << " <= '0' & FBSquare" << range(FBSquareSize-1, FBSquareSize-(1+wF+g)) << ";"<<endl;
			

			IntAdder* adder = new IntAdder(target,  2 + wF + g);
			adder->changeName(getName()+"_adder");
			addSubComponent(adder);
			inPortMap  (adder, "X", "FAsquareTrunc");
			inPortMap  (adder, "Y", "FBsquareTrunc");
			outPortMap (adder, "R","SumOfSquare");
			vhdl << instance(adder, "adder");
			
			// Now remains to compute the square root of the sum, and normalize.
			
			// With one mantissas in [1,2) and the other in [0, 2) after shifting,
			// the sum of squares is in [1, 4(
			// After square rooting it will be in [1,2) so no hurry to normalize. 
	};

	
	void FP2DNorm::emulate(TestCase * tc) {
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");
		mpz_class svY = tc->getInputValue("Y");

		/* Compute correct value */
		FPNumber fpx(wE, wF);
		fpx = svX;
		FPNumber fpy(wE, wF);
		fpy = svY;

		mpfr_t x,y, x2, y2, t,  ru, rd;
		mpfr_init2(x,  1+wF);
		mpfr_init2(y,  1+wF);

		mpfr_init2(x2,  2*(1+wF)); // the square fits on double the bits
		mpfr_init2(y2,  2*(1+wF));
		mpfr_init2(t,  10*(1+wF)); // temporary computations in ten times the precision

		mpfr_init2(ru, 1+wF);
		mpfr_init2(rd, 1+wF);


		fpx.getMPFR(x); 
		fpy.getMPFR(y); 

		// computation of the squares: exact, therefore shared between RU and RD
		mpfr_mul(x2, x,x, GMP_RNDN); // exact
		mpfr_mul(y2, y,y, GMP_RNDN); // exact

		// computation of result, rounded down
		mpfr_add(t, x2,y2, GMP_RNDD); // may round here
		mpfr_sqrt(rd, t, GMP_RNDD); // sqrt(x^2+y^2), rounded down to target prec
		// Set output
		FPNumber  fprd(wE, wF, rd);
		mpz_class svRd = fprd.getSignalValue();
		tc->addExpectedOutput("R", svRd);

		// computation of result, rounded up
		mpfr_add(t,x2,y2, GMP_RNDU); // may round here
		mpfr_sqrt(ru,t, GMP_RNDU); // sqrt(x^2+y^2), rounded up to target prec
 		// Set output
		FPNumber  fpru(wE, wF, ru);
		mpz_class svRu = fpru.getSignalValue();
		tc->addExpectedOutput("R", svRu);

		// clean up
		mpfr_clears(x, y, x2, y2, t, rd, ru, NULL);

	}


	// void FP2DNorm::buildStandardTestCases(TestCaseList * tcl) {}
	// please fill me with regression tests or corner case tests
}
