/*
   An arctangent(y/x) implementation

	Author:  Florent de Dinechin, Matei Istoan

	This file is part of the FloPoCo project

	Initial software.
	Copyright © INSA Lyon, INRIA, CNRS, UCBL,
	2014.
	All rights reserved.
*/


#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "../utils.hpp"
#include "Operator.hpp"
#include "FixAtan2.hpp"
#include "ShiftersEtc/LZOC.hpp"
#include "ShiftersEtc/Shifters.hpp"
#include "FixAtan2ByRecipMultAtan.hpp"
#include "FixAtan2ByCORDIC.hpp"
#include "FixAtan2ByBivariateApprox.hpp"

using namespace std;

namespace flopoco {


	// The constructor for a stand-alone operator
	FixAtan2::FixAtan2(Target* target_, int wIn_, int wOut_,  map<string, double> inputDelays_):
		Operator(target_, inputDelays_), wIn(wIn_), wOut(wOut_)
	{
		// Some code that is present in all the variants of FixAtan2
		//manage the pipeline
		setCriticalPath(getMaxInputDelays(inputDelays_));
		//declare the inputs and the outputs
		// man atan2 says "atan2(y,x) is atan(y/x)" so we will provide the inputs in the same order...
		addInput ("X",  wIn);
		addInput ("Y",  wIn);
		addOutput("A",  wOut, 2 /*number of possible output values*/);

		// everybody needs many digits of Pi (used by emulate etc)
		mpfr_init2(constPi, 10*(wIn+wOut));
		mpfr_const_pi( constPi, GMP_RNDN);
	}


	FixAtan2::~FixAtan2()
	{
		mpfr_clears (constPi, NULL);
	}




	void FixAtan2::buildQuadrantRangeReduction(){
		vhdl << tab << declare("sgnX") << " <= X" << of(wIn-1) << ";" << endl;
		vhdl << tab << declare("sgnY") << " <= Y" << of(wIn-1) << ";" << endl;

		vhdl << tab << "-- First saturate x and y in case they touch -1" <<endl;
		vhdl << tab << declare("Xsat", wIn) << " <= \"1" << zg(wIn-2,-2) << "1\" when X=\"1" <<zg (wIn-1,-2) << "\" else X ;" <<endl;
		vhdl << tab << declare("Ysat", wIn) << " <= \"1" << zg(wIn-2,-2) << "1\" when Y=\"1" <<zg (wIn-1,-2) << "\" else Y ;" <<endl;

		// When pipelining I noticed that we perform a subtraction for the comparison X<Y in parallel to the negation anyway
		// so negateByComplement should always be false,
		//except if some day we do an ASIC target where it will cost less hardware.

		if (negateByComplement)	{
			manageCriticalPath( getTarget()->lutDelay());
			vhdl << tab << declare("pX", wIn) << " <=      Xsat;" << endl;
			vhdl << tab << declare("pY", wIn) << " <=			 Ysat;" << endl;
			vhdl << tab << declare("mX", wIn) << " <= (not Xsat);	 -- negation by not, implies one ulp error." << endl;
			vhdl << tab << declare("mY", wIn) << " <= (not Ysat);	 -- negation by not, implies one ulp error. " << endl;
		}else {
			manageCriticalPath( getTarget()->adderDelay(wIn));
			vhdl << tab << declare("pX", wIn) << " <= Xsat;" << endl;
			vhdl << tab << declare("pY", wIn) << " <= Ysat;" << endl;
			vhdl << tab << declare("mX", wIn) << " <= (" << zg(wIn) << " - Xsat);" << endl;
			vhdl << tab << declare("mY", wIn) << " <= (" << zg(wIn) << " - Ysat);" << endl;
		}

		// TODO: replace the following with LUT-based comparators
		// and first maybe experiment with synthesis tools
		vhdl << tab << declare("XmY", wIn+1) << " <= (sgnX & Xsat)-(sgnY & Ysat);" << endl;
		vhdl << tab << declare("XpY", wIn+1) << " <= (sgnX & Xsat)+(sgnY & Ysat);" << endl;
		vhdl << tab << declare("XltY") << " <= XmY" << of(wIn) <<";" << endl;
		vhdl << tab << declare("mYltX") << " <= not XpY" << of(wIn) <<";" << endl;
		// Range reduction: we define 4 quadrants, each centered on one axis (these are not just the sign quadrants)
		// Then each quadrant is decomposed in its positive and its negative octant.

		manageCriticalPath( getTarget()->lutDelay() + getTarget()->localWireDelay());

		vhdl << tab << "-- quadrant will also be the angle to add at the end" <<endl;
		vhdl << tab << declare("quadrant", 2) << " <= " << endl;
		vhdl << tab << tab << "\"00\"  when (not sgnX and not XltY and     mYltX)='1' else"    << endl;
		vhdl << tab << tab << "\"01\"  when (not sgnY and     XltY and     mYltX)='1' else"    << endl;
		vhdl << tab << tab << "\"10\"  when (    sgnX and     XltY and not mYltX)='1' else"    << endl;
		vhdl << tab << tab << "\"11\";"    << endl;



		int sizeXYR = wIn-1; // no need for sign bit any longer
		vhdl << tab << declare("XR", sizeXYR) << " <= " << endl;
		vhdl << tab << tab << "pX" << range(sizeXYR-1, 0) << " when quadrant=\"00\"   else " << endl;
		vhdl << tab << tab << "pY" << range(sizeXYR-1, 0) << " when quadrant=\"01\"   else " << endl;
		vhdl << tab << tab << "mX" << range(sizeXYR-1, 0) << " when quadrant=\"10\"   else " << endl;
		vhdl << tab << tab << "mY" << range(sizeXYR-1, 0) << ";"    << endl;

		vhdl << tab << declare("YR", sizeXYR) << " <= " << endl;
		vhdl << tab << tab << "pY" << range(sizeXYR-1, 0) << " when quadrant=\"00\" and sgnY='0'  else " << endl;
		vhdl << tab << tab << "mY" << range(sizeXYR-1, 0) << " when quadrant=\"00\" and sgnY='1'  else " << endl;
		vhdl << tab << tab << "pX" << range(sizeXYR-1, 0) << " when quadrant=\"01\" and sgnX='0'  else " << endl;
		vhdl << tab << tab << "mX" << range(sizeXYR-1, 0) << " when quadrant=\"01\" and sgnX='1'  else " << endl;
		vhdl << tab << tab << "pY" << range(sizeXYR-1, 0) << " when quadrant=\"10\" and sgnY='0'  else " << endl;
		vhdl << tab << tab << "mY" << range(sizeXYR-1, 0) << " when quadrant=\"10\" and sgnY='1'  else " << endl;
		vhdl << tab << tab << "pX" << range(sizeXYR-1, 0) << " when quadrant=\"11\" and sgnX='0'  else "    << endl;
		vhdl << tab << tab << "mX" << range(sizeXYR-1, 0) << " ;"    << endl;

		vhdl << tab << declare("finalAdd") << " <= " << endl;
		vhdl << tab << tab << "'1' when (quadrant=\"00\" and sgnY='0') or(quadrant=\"01\" and sgnX='1') or (quadrant=\"10\" and sgnY='1') or (quadrant=\"11\" and sgnX='0')" << endl;
		vhdl << tab << tab << " else '0';  -- this information is sent to the end of the pipeline, better compute it here as one bit"    << endl;
	}

	void FixAtan2::buildQuadrantReconstruction(){
		manageCriticalPath(getTarget()->lutDelay() + getTarget()->adderDelay(wOut));
		vhdl << tab << declare("qangle", wOut) << " <= (quadrant & " << zg(wOut-2) << ");" << endl;
		vhdl << tab << "A <= "
				 << tab << tab << "     qangle + finalZ  when finalAdd='1'" << endl
				 << tab << tab << "else qangle - finalZ;" << endl;
		getOutDelayMap()["A"] = getCriticalPath();
	}




	void FixAtan2::buildScalingRangeReduction(){
		int sizeXYR = wIn-1; // no need for sign bit any longer
		manageCriticalPath( getTarget()->lutDelay() + getTarget()->localWireDelay());
		vhdl << tab << declare("XorY", sizeXYR-1) << " <= XR" << range(sizeXYR-1,1) << " or YR" << range(sizeXYR-1,1) << ";" << endl;
		// The LZC
		LZOC* lzc = new	LZOC(getTarget(), sizeXYR-1);
		addSubComponent(lzc);

		inPortMap(lzc, "I", "XorY");
		inPortMapCst(lzc, "OZB", "'0'");
		outPortMap(lzc, "O", "S");
		vhdl << instance(lzc, "lzc");
		syncCycleFromSignal("S");
		setCriticalPath(lzc->getOutputDelay("O"));

		//setCycleFromSignal("lzo");
		//		setCriticalPath( lzc->getOutputDelay("O") );

		// The two shifters are two instance of the same component
		Shifter* lshift = new Shifter(getTarget(), sizeXYR, sizeXYR-1, Shifter::Left);
		addSubComponent(lshift);

		inPortMap(lshift, "S", "S");

		inPortMap(lshift, "X", "XR");
		outPortMap(lshift, "R", "XRSfull");
		vhdl << instance(lshift, "Xshift");
		vhdl << tab << declare("XRS", sizeXYR) << " <=  XRSfull " << range(sizeXYR-1,0) << ";" << endl;

		inPortMap(lshift, "X", "YR");
		outPortMap(lshift, "R", "YRSfull");
		vhdl << instance(lshift, "Yshift");
		vhdl << tab << declare("YRS", sizeXYR) << " <=  YRSfull " << range(sizeXYR-1,0) << ";" << endl;

		syncCycleFromSignal("YRSfull");
		setCriticalPath(lshift->getOutputDelay("R"));
}



	void FixAtan2::emulate(TestCase * tc)
	{
#if 0
		mpfr_t x,y,a, constPi;
		mpfr_init2(x, 10*wIn);
		mpfr_init2(y, 10*wIn);
		mpfr_init2(a, 10*wIn);
		mpfr_init2(constPi, 10*wIn);
		mpfr_const_pi( constPi, GMP_RNDN);

		mpz_class az;

		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");
		mpz_class svY = tc->getInputValue("Y");

		// interpret as signed two'ss complement
		if (1==(svX >> (wIn-1))) // sign bit
			svX -= (1<<wIn);
		if (1==(svY >> (wIn-1))) // sign bit
			svY -= (1<<wIn);
		/* Compute correct value */

		mpfr_set_z (x, svX.get_mpz_t(), GMP_RNDN); //  exact
		mpfr_set_z (y, svY.get_mpz_t(), GMP_RNDN); //  exact

		mpfr_atan2(a, y, x, GMP_RNDN); // a between -pi and pi
		mpfr_div(a, a, constPi, GMP_RNDN); // a between -1 and 1

		// Now convert a to fix point
		// Align to fix point by adding 6 -- if we just add 4 there is a 1-bit shift in case a<0
		mpfr_add_d(a, a, 6.0, GMP_RNDN);
		mpfr_mul_2si (a, a, wOut-1, GMP_RNDN); // exact scaling

		mpz_class mask = (mpz_class(1)<<wOut) -1;

		mpfr_get_z (az.get_mpz_t(), a, GMP_RNDD); // there can be a real rounding here
		az -= mpz_class(6)<<(wOut-1);
		az &= mask;
		tc->addExpectedOutput ("R", az);

		mpfr_get_z (az.get_mpz_t(), a, GMP_RNDU); // there can be a real rounding here
		az -= mpz_class(6)<<(wOut-1);
		az &= mask;
		tc->addExpectedOutput ("R", az);

		// clean up
		mpfr_clears (x,y,a, constPi, NULL);
#else
		mpfr_t x,y,a;
		mpfr_init2(x, 10*wIn);
		mpfr_init2(y, 10*wIn);
		mpfr_init2(a, 10*wOut);

		mpz_class az;

		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");
		mpz_class svY = tc->getInputValue("Y");

		// interpret as signed two'ss complement
		if (1==(svX >> (wIn-1))) // sign bit
			svX -= (mpz_class(1)<<wIn);
		if (1==(svY >> (wIn-1))) // sign bit
			svY -= (mpz_class(1)<<wIn);
		/* Compute correct value */

		mpfr_set_z (x, svX.get_mpz_t(), GMP_RNDN); //  exact
		mpfr_set_z (y, svY.get_mpz_t(), GMP_RNDN); //  exact

		mpfr_atan2(a, y, x, GMP_RNDN); // a between -pi and pi
		mpfr_div(a, a, constPi, GMP_RNDN); // a between -1 and 1

		// Now convert a to fix point
		// Align to fix point by adding 6 -- if we just add 4 there is a 1-bit shift in case a<0
		mpfr_add_d(a, a, 6.0, GMP_RNDN);
		mpfr_mul_2si (a, a, wOut-1, GMP_RNDN); // exact scaling

		mpz_class mask = (mpz_class(1)<<wOut) -1;

		mpfr_get_z (az.get_mpz_t(), a, GMP_RNDD); // there can be a real rounding here
		az -= mpz_class(6)<<(wOut-1);
		az &= mask;
 		tc->addExpectedOutput ("A", az);

		mpfr_get_z (az.get_mpz_t(), a, GMP_RNDU); // there can be a real rounding here
		az -= mpz_class(6)<<(wOut-1);
		az &= mask;
 		tc->addExpectedOutput ("A", az);

		// clean up
		mpfr_clears (x,y,a, NULL);
#endif
	}



	void FixAtan2::buildStandardTestCases(TestCaseList * tcl)
	{
		//mpfr_t z;
		//mpz_class zz;
		TestCase* tc;

		// 0
		tc = new TestCase (this);
		tc -> addInput ("X", mpz_class(1)<< (wIn-2));
		tc -> addInput ("Y", mpz_class(0));
		emulate(tc);
		tcl->add(tc);


		//pi/4
		tc = new TestCase (this);
		tc -> addInput ("X", mpz_class(1)<< (wIn-2));
		tc -> addInput ("Y", mpz_class(1)<< (wIn-2));
		emulate(tc);
		tcl->add(tc);

		// pi/2
		tc = new TestCase (this);
		tc -> addInput ("X", mpz_class(0));
		tc -> addInput ("Y", mpz_class(1)<< (wIn-2));
		emulate(tc);
		tcl->add(tc);


		// 3pi/4
		tc = new TestCase (this);
		tc -> addInput ("X", (mpz_class(1)<< (wIn)) -  (mpz_class(1)<< (wIn-2)) );
		tc -> addInput ("Y", mpz_class(1)<< (wIn-2));
		emulate(tc);
		tcl->add(tc);


	}

	OperatorPtr FixAtan2::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {		
		int lsb, method;
		UserInterface::parseInt(args, "lsb", &lsb);
		UserInterface::parsePositiveInt(args, "method", &method);
		//select the method
		if(method < 8){	
			return new FixAtan2ByRecipMultAtan(target, -lsb,-lsb, method);
		}
		else if(method<10) {
			return new FixAtan2ByCORDIC(target, -lsb,-lsb);
		}
		else {
			return new FixAtan2ByBivariateApprox(target, -lsb, -lsb, method-10);
		}
			
	}

	void FixAtan2::registerFactory(){
		UserInterface::add("FixAtan2", // name
											 "Computes atan(x/y) as a=(angle in radian)/pi so a in [-1,1[.",
											 "ElementaryFunctions",
											 "", // seeAlso
											 "lsb(int): weight of the LSB of both inputs and outputs; \
                        method(int): parameter select between: InvMultAtan with approximations of the corresponding degree (0..7), plain CORDIC (8), CORDIC with scaling (9), a method using surface approximation (10), Taylor approximation of order 1 (11) and 2 (12)",
											 "For more details, see <a href=\"bib/flopoco.html#DinIsto2015\">this article</a>.",
											 FixAtan2::parseArguments
											 ) ;
		
	}



}
