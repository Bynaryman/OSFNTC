/*
  IEEE-compatible Floating Point Adder for FloPoCo

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Authors: Florent de Dinechin, Valentin Huguet

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, 2016-2019.
  All right reserved.

  */

#include "IEEEAdd.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <string.h>

#include <gmp.h>
#include <mpfr.h>

#include <gmpxx.h>
#include <utils.hpp>
#include <Operator.hpp>


using namespace std;


namespace flopoco{
	
	
#define DEBUGVHDL 0
	
	
	IEEEAdd::IEEEAdd(OperatorPtr parentOp, Target* target,
		int wE, int wF,
		bool sub) :
		Operator(parentOp, target), wE(wE), wF(wF), sub(sub) {
		
		srcFileName="IEEEAdd";

		ostringstream name;
		if(sub)
			name<<"IEEESub_";
		else
			name<<"IEEEAdd_";

		name <<wE<<"_"<<wF;
		setNameWithFreqAndUID(name.str());

		setCopyrightString("Florent de Dinechin, Valentin Huguet (2016)");

		sizeRightShift = intlog2(wF+3);

	/* Set up the IO signals */
	/* Inputs: 2b(Exception) + 1b(Sign) + wE bits (Exponent) + wF bits(Fraction) */
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		addIEEEInput ("X", wE, wF);
		addIEEEInput ("Y", wE, wF);
		addIEEEOutput("R", wE, wF);

	//=========================================================================|
	//                          Swap/Difference                                |
	// ========================================================================|
		vhdl << endl;
		addComment("Exponent difference and swap", tab);
  	
  	vhdl << tab << declare(.0, "expFracX", wE+wF) << " <= X" << range(wE+wF-1, 0) << ";" << endl;
  	vhdl << tab << declare(.0, "expFracY", wE+wF) << " <= Y" << range(wE+wF-1, 0) << ";" << endl;
  	vhdl << tab << declare(target->adderDelay(wE+1), "expXmExpY",wE+1) << " <= ('0' & X" << range(wE+wF-1, wF) << ") - ('0'  & Y" << range(wE+wF-1, wF) << ") " << ";" << endl;
  	vhdl << tab << declare(target->adderDelay(wE+1), "expYmExpX",wE+1) << " <= ('0' & Y" << range(wE+wF-1, wF) << ") - ('0'  & X" << range(wE+wF-1, wF) << ") " << ";" << endl;

  	vhdl << tab << declare(target->adderDelay(wE+wF), "swap") << " <= '0' when expFracX >= expFracY else '1';" << endl;

  	string pmY="Y";
		if ( sub ) {
			vhdl << tab << declare("mY",wE+wF+1) << " <= not(Y"<<of(wE+wF)<<") & expFracY"<<endl;
			pmY = "mY";
		}

		vhdl << tab << declare(target->logicDelay(1), "newX", wE+wF+1) << " <= X when swap = '0' else " << pmY << ";" << endl;
		vhdl << tab << declare(target->logicDelay(1), "newY", wE+wF+1) << " <= " << pmY << " when swap = '0' else X;" << endl;
		vhdl << tab << declare(target->logicDelay(1), "expDiff",wE+1) << " <= expXmExpY when swap = '0' else expYmExpX;"<<endl;
		vhdl << tab << declare(.0, "expNewX", wE) << " <= newX" << range(wE+wF-1, wF) << ";" << endl;
		vhdl << tab << declare(.0, "expNewY", wE) << " <= newY" << range(wE+wF-1, wF) << ";" << endl;
		vhdl << tab << declare(.0, "signNewX")   << " <= newX"<<of(wE+wF)<<";"<<endl;
		vhdl << tab << declare(.0, "signNewY")   << " <= newY"<<of(wE+wF)<<";"<<endl;
		vhdl << tab << declare(target->logicDelay(2), "EffSub") << " <= signNewX xor signNewY;"<<endl;		

		addComment("Special case dectection", tab);		
		vhdl << tab << declare(target->logicDelay(wE), "xExpFieldZero")    << " <= '1' when expNewX=" << zg(wE) << " else '0';" << endl;
		vhdl << tab << declare(target->logicDelay(wE), "yExpFieldZero")    << " <= '1' when expNewY=" << zg(wE) << " else '0';" << endl;
		vhdl << tab << declare(target->logicDelay(wE), "xExpFieldAllOnes") << " <= '1' when expNewX=" << og(wE) << " else '0';" << endl;
		vhdl << tab << declare(target->logicDelay(wE), "yExpFieldAllOnes") << " <= '1' when expNewY=" << og(wE) << " else '0';" << endl;
		vhdl << tab << declare(target->logicDelay(wF), "xSigFieldZero")    << " <= '1' when newX" << range(wF-1, 0) << "=" << zg(wF) << " else '0';" << endl;
		vhdl << tab << declare(target->logicDelay(wF), "ySigFieldZero")    << " <= '1' when newY" << range(wF-1, 0) << "=" << zg(wF) << " else '0';" << endl;
		vhdl << tab << declare(target->logicDelay(2),  "xIsNaN")           << " <= xExpFieldAllOnes and not xSigFieldZero;"<<endl;
		vhdl << tab << declare(target->logicDelay(2),  "yIsNaN")           << " <= yExpFieldAllOnes and not ySigFieldZero;"<<endl;
		vhdl << tab << declare(target->logicDelay(2), "xIsInfinity")       << " <= xExpFieldAllOnes and xSigFieldZero;"<<endl;
		vhdl << tab << declare(target->logicDelay(2), "yIsInfinity")       << " <= yExpFieldAllOnes and ySigFieldZero;"<<endl;
		vhdl << tab << declare(target->logicDelay(2), "xIsZero")           << " <= xExpFieldZero and xSigFieldZero;"<<endl;
		vhdl << tab << declare(target->logicDelay(2), "yIsZero")           << " <= yExpFieldZero and ySigFieldZero;"<<endl;
		
		vhdl << tab << declare(target->logicDelay(2), "bothSubNormals") << " <=  xExpFieldZero and yExpFieldZero;" << endl;

		vhdl << tab << declare(target->logicDelay(5), "resultIsNaN") << " <=  xIsNaN or yIsNaN  or  (xIsInfinity and yIsInfinity and EffSub);" << endl;
		
		vhdl << tab << declare("significandNewX", wF+1) << " <= not(xExpFieldZero) & newX" << range(wF-1, 0) << ";" << endl;
		vhdl << tab << declare("significandNewY", wF+1) << " <= not(yExpFieldZero) & newY" << range(wF-1, 0) << ";" << endl;


		//=========================================================================|
		//                             Alignment                                   |
		// ========================================================================|
		vhdl << endl;
		addComment("Significand alignment", tab);

		vhdl << tab << declare(target->adderDelay(wE), "allShiftedOut") << " <= '1' when (expDiff >= "<<wF+3<<") else '0';"<<endl;
		vhdl << tab << declare(target->logicDelay(1), "rightShiftValue",sizeRightShift) << " <= expDiff("<< sizeRightShift-1<<" downto 0) when allShiftedOut='0' else CONV_STD_LOGIC_VECTOR("<<wF+3<<","<<sizeRightShift<<") ;" << endl;
		vhdl << tab << declare(target->logicDelay(2), "shiftCorrection") << " <= '1' when (yExpFieldZero='1' and xExpFieldZero='0') else '0'; -- only other cases are: both normal or both subnormal" << endl;
		vhdl << tab << declare(target->adderDelay(sizeRightShift), "finalRightShiftValue", sizeRightShift) << " <= rightShiftValue - ("<<zg(sizeRightShift-1)<<" & shiftCorrection);" << endl;
		
#if 0 // shift, then sticky
		newInstance(
				"Shifter", 
				"RightShifterComponent", 
				"wIn=" + to_string(wF+1) + " maxShift=" + to_string(wF+2) + " dir=1", 
				"X=>significandNewY,S=>finalRightShiftValue",
				"R=>shiftedSignificandY"
			);


		vhdl << tab << declare("stickyInBits", wF) << " <= shiftedSignificandY" << range(wF-1, 0) << ";"<<endl;
		vhdl << tab << declare(target->eqConstComparatorDelay(wF), "stickyLow") << " <= '0' when stickyInBits = "<< zg(wF) <<" else '1';" << endl;
		vhdl << tab << declare(target->logicDelay(2), "summandY", wF+4) << " <= ('0' & shiftedSignificandY" << range(2*wF+2, wF) << ") xor " << rangeAssign(wF+3,0,"EffSub") << ";"<<endl;

#else
		vhdl << tab << declare("significandY00", wF+3) << " <= significandNewY & "<<zg(2)<<";"<<endl;
		newInstance(
				"Shifter", 
				"RightShifterComponent", 
				"wIn=" + to_string(wF+3) + " maxShift=" + to_string(wF+2) + " dir=1 " + "wOut=" + to_string(wF+3) + " computeSticky=1", 
				"X=>significandY00,S=>finalRightShiftValue",
				"R=>shiftedSignificandY,Sticky=>stickyLow"
			);
		vhdl << tab << declare(target->logicDelay(2), "summandY", wF+4) << " <= ('0' & shiftedSignificandY) xor " << rangeAssign(wF+3,0,"EffSub") << ";"<<endl;
#endif

		vhdl << endl;

		//=========================================================================|
		//                              Addition                                   |
		// ========================================================================|
		vhdl << endl;
		addComment("Significand addition", tab);

		//expSigShiftedNewY size = exponent size + RightShifter's wOut_ size
		vhdl << tab << declare("summandX", wF+4) << " <= '0' & significandNewX & '0' & '0';" << endl;


		vhdl << tab << declare("carryIn") << " <= EffSub and not stickyLow;" << endl; // TODO break this dependency, send the stickyLow to the final round adder
		// like		vhdl << tab << declare("carryIn") << " <= '0';" << endl; and we win 3ns
		
		newInstance(
				"IntAdder", 
				"fracAdder", 
				"wIn=" + to_string(wF+4),
				"X=>summandX,Y=>summandY,Cin=>carryIn", 		/*This carry completes the subtraction*/
				"R=>significandZ" 
			);
		vhdl << endl;



		//=========================================================================|
		//                             Renormalize                                 |
		// ========================================================================|

		/* Now we have a significandZ of size wF+4 with the following alignment WRT the exponent expNewX: ab.xxxxxxxxxx (here 2*wF+2 'x')
			 We first normalize it, then round it.
			 Let us define "normalize" as: shift significand in such a way that we can drop its MSB to get a fraction, and subtract deltaexp to the exponent 
			 The various cases are:
			 A/ lzc=0 <=> a=1 : there was an overflow in the sig. add.:  shift=0=lzc, deltaexp=-1
			 B/ lzc=1 => a=0, b=1. Two subcases:
			   B.1/ X was subnormal (hence Y too) and there has been an overflow in the addition. shift=1=lzc, deltaexp=1 so expR will be 0001. roundBit=0 
				 B.2/ X was normal and there has been no overflow: shift=1, deltaexp=0
       C/ lzc>=2: there is a cancellation. 
    			 Again two sub-cases:
					C.1/ X was subnormal: shift=1, deltaexp=0
					C.2/ X was normal (expNewX >=1): we have 0 <= lzc <= wF+3
					    C.2.a  if lzc <= expNewX and lzc<>wF+3
                   shift = lzc; deltaexp = lzc-1   and we end up with a normal significand
              C.2.b  if lzc < wf+3   and lzc > expNewX
                   shift = expNewX, deltaexp =expNewX,    and we end up with a subnormal significand
              C.2.c  if wF+3 = lzc , whatever expNewX: result is zero
                   shift = wF+3=lzc, deltaexp =expNewX,    and we end up with a zero
									 
			 examples: 
			   expNewX=3 ab.xxxxxxxxxx = 00.1111111  C.2.a  exp will be 1, sig will be (1)11111100
				 expNewX=2 ab.xxxxxxxxxx = 00.1111111  C.2.b  exp will be 0, sig will be (0)11111110  
				 expNewX=1 ab.xxxxxxxxxx = 00.1111111  C.2.b  exp will be 0, sig will be (0)01111111

			 Parameters:
					1/ LZC: count at most wF+3 bits:
			     the maximum possible cancellation  is wF+1 bits, ex. when wF=4
	 	          010000
						 -  11111
		        = 0000001  
						but we also input to the LZC  the leftmost 0 which is the carry-out bit of the adder)
				 So if lzc=wF+3 we know that the result is zero.

				 2/ Maximum shift distance is full cancellation in the normal case (case C.2.a) so again
				 After the shift we can do the rounding. The bit to add (roundBit=0 or 1) can simply be computed out of lsb, round and sticky, no need of a g bit.
				 

				 Implementation finetuning: 
				 Since the case A and B can be detected by looking at the two first bit, we only need to LZC among the 'x' above, but then we need to add 2 to the computed value.
				 Probably a win areawise and a loss delaywise: TODO try it;
				 
		 */


		//						 TODO: all this assumes that wE>log2(wF+2), true for all the standard IEEE formats. We should test properly when it doesnt work anymore
		addComment("Cancellation detection, renormalization (see explanations in IEEEAdd.cpp) ", tab);

		vhdl << tab << declare("z1") << " <=  significandZ" << of(wF+3) << "; -- bit of weight 1" << endl;
		vhdl << tab << declare("z0") << " <=  significandZ" << of(wF+2) << "; -- bit of weight 0" << endl;

		vhdl << tab << declare("lzcZInput", wF+3) << " <= significandZ" << range(wF+3,1) << ";"<<endl;
		newInstance(
				"LZOC", 
				getName()+"LeadingZeroCounter", 
				"wIn=" + to_string(wF+3) + " countType=0", 
				"I=>lzcZInput",
				"O=>lzc"
			);
		int lzcSize = getSignalByName("lzc")->width();
		if(lzcSize>wE) {
			THROWERROR("These values of parameters wE and wF are not supported (wE too small WRT wF) -- if you want this to work, contact us");
		}

		vhdl << tab << declare(target->adderDelay(lzcSize)+target->logicDelay(1), "leftShiftVal", lzcSize)	 << " <= " << endl
				 << tab << tab << "lzc when ("
				 << "(z1='1')" // case A/
				 <<" or (z1='0' and z0='1' and xExpFieldZero='1')" // case B.1
				 <<" or (z1='0' and z0='0' and xExpFieldZero='0' and lzc<=expNewX) " // case C.2.a
				 <<" or (xExpFieldZero='0' and lzc>=" << wF+3 <<") " // case C.2.c
				 << ") " << endl
				 << tab << tab << "else (expNewX"<< range(lzcSize-1,0) <<") when ("
				 << "xExpFieldZero='0' and (lzc < "<< wF+3 << ") and ((" << zg(wE-lzcSize) << "&lzc)>=expNewX)"  // case C.2.b 
				 << ") " << endl
				 << tab << tab << " else " << zg(lzcSize-1) << "&'1'"  // case B.2 and C.1 
				 <<";" << endl;
		

		newInstance(
				"Shifter", 
				"LeftShifterComponent", 
				"wIn=" + to_string(wF+4) + " maxShift=" + to_string(wF+3) + " dir=0",
				"X=>significandZ,S=>leftShiftVal",
				"R=>normalizedSignificand" // output size will be 2*wF+6 TODO: not output unused bits
								); 
		
		vhdl << tab << declare("significandPreRound", wF) << " <= normalizedSignificand" << range(wF+2, 3) << "; -- remove the implicit zero/one" << endl;
		vhdl << tab << declare("lsb") << " <= normalizedSignificand" << of(3) << ";" << endl;
		vhdl << tab << declare("roundBit") << " <= normalizedSignificand" << of(2) << ";" << endl;
		vhdl << tab << declare(target->adderDelay((wF+2)/target->lutInputs()), "stickyBit") << " <= stickyLow or  normalizedSignificand"<< of(1) << "or  normalizedSignificand"<< of(0) << ";" << endl;		

		vhdl << tab << declare("deltaExp", wE)	 << " <=    -- value to subtract to exponent for normalization" << endl
				 << tab << tab << zg(wE) << " when ( (z1='0' and z0='1' and xExpFieldZero='0')" << endl // 0: case B.2
				 << tab << tab <<         "    or  (z1='0' and z0='0' and xExpFieldZero='1') )" << endl // case C.1
				 << tab << tab << "else " << og(wE) << " when ( (z1='1')" // case A/
				 <<                     "  or  (z1='0' and z0='1' and xExpFieldZero='1')" // case B.1
				 << ")" << endl 
				 << tab << tab << "else (" << zg(wE-lzcSize) << " & lzc)-'1' when (z1='0' and z0='0' and xExpFieldZero='0' and lzc<=expNewX and lzc<" << wF+3 << ")" // lzc: case C.2.a
				 << tab << tab << "else expNewX" // case C2.2.=b and C.2.c
				 <<";" << endl;
				
		vhdl << tab << declare(target->adderDelay(lzcSize/target->lutInputs()), "fullCancellation") << " <= '1' when (lzc>="<< wF+3<<") else '0';"<<endl;
		// Now do the shift and the exponent addition, done conservatively (it could probably be smaller)


		vhdl << tab << declare(target->adderDelay(wE), "expPreRound", wE) << " <= expNewX - deltaExp; -- we may have a first overflow here" << endl;
		vhdl << tab << declare("expSigPreRound", wE+wF) << " <= expPreRound & significandPreRound; " << endl;



		
		addComment("Final rounding, with the mantissa overflowing in the exponent  ", tab);
		
		vhdl << tab << declare(target->logicDelay(3), "roundUpBit")<<" <= '1' when roundBit='1' and (stickyBit='1' or (stickyBit='0' and lsb='1')) else '0';"<<endl;
		vhdl << tab << declare("zeroadd", wE+wF) << " <=  " << zg(wE+wF) << ";" << endl;
		newInstance(
				"IntAdder", 
				"roundingAdder", 
				"wIn=" + to_string(wE+wF), 
				"X=>expSigPreRound,Cin=>roundUpBit,Y=>zeroadd",
				"R=>expSigR"
			);

		addComment("Final packing", tab);
		vhdl << tab << declare(target->adderDelay(sizeLeftShift+1), "resultIsZero") << " <= '1' when (fullCancellation='1' and expSigR" << range(wE+wF-1, wF) << "=" << zg(wE) << ") else '0';"<<endl;
		vhdl << tab << declare(target->adderDelay(sizeLeftShift+1), "resultIsInf") << " <= '1' when resultIsNaN='0' and (((xIsInfinity='1' and yIsInfinity='1'  and EffSub='0')  or (xIsInfinity='0' and yIsInfinity='1')  or (xIsInfinity='1' and yIsInfinity='0')  or  (expSigR" << range(wE+wF-1, wF) << "=" << og(wE) << "))) else '0';"<<endl;
		
		vhdl<<tab<< declare("constInf",wE+wF) << " <= " << og(wE) << " & " << zg(wF) << ";"<<endl;
		vhdl<<tab<< declare("constNaN",wE+wF) << " <= " << og(wE+wF) << ";"<<endl;
		vhdl<<tab<< declare(target->logicDelay(2), "expSigR2",wE+wF) << " <= constInf when resultIsInf='1' else constNaN when resultIsNaN='1' else expSigR;"<<endl;

		vhdl << tab << declare(target->logicDelay(5), "signR") << " <= '0' when ("
				 << "(resultIsNaN='1' "
				 << " or (resultIsZero='1' and xIsInfinity='0' and yIsInfinity='0'))"
				 << " and (xIsZero='0' or yIsZero='0' or (signNewX /= signNewY))"
				 <<	" )  else signNewX;" << endl;

		/*Result*/
		vhdl<<tab<< declare("computedR",wE+wF+1) << " <= signR & expSigR2;"<<endl;
		
		vhdl << tab << "R <= computedR;"<<endl;
	}


	IEEEAdd::~IEEEAdd() {
	}


	void IEEEAdd::emulate(TestCase * tc)
	{
		/* Get I/O values */
			mpz_class svX = tc->getInputValue("X");
			mpz_class svY = tc->getInputValue("Y");

		/* Compute correct value */
			IEEENumber ieeex(wE, wF, svX);
			IEEENumber ieeey(wE, wF, svY);
			mpfr_t x, y, r;
			mpfr_init2(x, 1+wF);
			mpfr_init2(y, 1+wF);
			mpfr_init2(r, 1+wF);
			ieeex.getMPFR(x);
			ieeey.getMPFR(y);

			// Here now we compute in r the MPFR correctly rounded result,
			// except in the cases of over/underflow.
			// These cases will be handled by IEEE number.
			// The ternary info allows us to avoid double-rounding issues
			// This is useless here, as an addition/subtraction that returns a subnormal is always errorless 
			// but this code will be copied by other functions

			int ternaryRoundInfo; 
			if(sub)
				ternaryRoundInfo = mpfr_sub(r, x, y, GMP_RNDN);
			else
				ternaryRoundInfo = mpfr_add(r, x, y, GMP_RNDN);

			IEEENumber  ieeer(wE, wF, r, ternaryRoundInfo); 
			mpz_class svR = ieeer.getSignalValue();
			tc->addExpectedOutput("R", svR);

		// clean up
			mpfr_clears(x, y, r, NULL);
	}


	void IEEEAdd::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;

		tc = new TestCase(this);
		tc->addIEEEInput("X", -1.0);
		tc->addIEEEInput("Y", 1.25);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addIEEEInput("X", 1.0);
		tc->addIEEEInput("Y", -1.25);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addIEEEInput("X", -1.25);
		tc->addIEEEInput("Y", 1.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addIEEEInput("X", 1.25);
		tc->addIEEEInput("Y", -1.0);
		emulate(tc);
		tcl->add(tc);


		vector<mpz_class> specialCaseList;
		specialCaseList.push_back(IEEENumber(wE, wF, IEEENumber::plusZero).getSignalValue());
		specialCaseList.push_back(IEEENumber(wE, wF, IEEENumber::smallestSubNormal).getSignalValue());
		specialCaseList.push_back(IEEENumber(wE, wF, IEEENumber::greatestSubNormal).getSignalValue());
		specialCaseList.push_back(IEEENumber(wE, wF, IEEENumber::smallestNormal).getSignalValue());
		specialCaseList.push_back(IEEENumber(wE, wF, IEEENumber::greatestNormal).getSignalValue());
		specialCaseList.push_back(IEEENumber(wE, wF, IEEENumber::plusInfty).getSignalValue());
		specialCaseList.push_back(IEEENumber(wE, wF, 1.0).getSignalValue());
		specialCaseList.push_back(IEEENumber(wE, wF, 1.5).getSignalValue()); // so we test 1.5-1.0 and 1.0-1.5

		// First add all the negative special cases 
		size_t size = specialCaseList.size();
		for (size_t i=0; i<size; i++){
			specialCaseList.push_back(specialCaseList[i] + (mpz_class(1)<<(wE+wF)));
			}

		// No build all the possible special case combinations
		for (size_t i=0; i<specialCaseList.size(); i++){
			for (size_t j=0; j<specialCaseList.size(); j++){
				tc = new TestCase(this);
				tc->addInput("X", specialCaseList[i]);
				tc->addInput("Y", specialCaseList[j]);
				emulate(tc);
				tcl->add(tc);
			}
		} 

		// Other regression tests, mostly for single prec
		tc = new TestCase(this);
		tc->addIEEEInput("X", 1.1754945e-38);
		tc->addIEEEInput("Y", 1.1754945e-38);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addIEEEInput("X", -4.375e1);
		tc->addIEEEInput("Y", 4.375e1);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addIEEEInput("X", 2.4e-44);
		tc->addIEEEInput("Y", 2.8e-45);
		emulate(tc);
		tcl->add(tc);
		
		tc = new TestCase(this);
		tc->addIEEEInput("X", 1.469368e-39);
		tc->addIEEEInput("Y", 4.5e-44);
		emulate(tc);
		tcl->add(tc);

		
}


	TestCase* IEEEAdd::buildRandomTestCase(int i){

		TestCase *tc;
		mpz_class x,y;
		mpz_class negative  = mpz_class(1)<<(wE+wF);

		tc = new TestCase(this);
		/* Fill inputs */
		if ((i & 7) == 0) {// cancellation, same exponent
			mpz_class e = getLargeRandom(wE);
			x  = getLargeRandom(wF) + (e << wF);
			y  = getLargeRandom(wF) + (e << wF) + negative;
		}
		else if ((i & 7) == 1) {// cancellation, exp diff=1
			mpz_class e = getLargeRandom(wE);
			x  = getLargeRandom(wF) + (e << wF);
			if(e < (mpz_class(1)<<(wE)-1))
				e++; // may rarely lead to an overflow, who cares
			y  = getLargeRandom(wF) + (e << wF) + negative;
		}
		else if ((i & 7) == 2) {// cancellation, exp diff=1
			mpz_class e = getLargeRandom(wE);
			x  = getLargeRandom(wF) + (e << wF) + negative;
			e++; // may rarely lead to an overflow, who cares
			y  = getLargeRandom(wF) + (e << wF);
		}
		else if ((i & 7) == 3) {// alignment within the mantissa sizes
			mpz_class e = getLargeRandom(wE);
			x  = getLargeRandom(wF) + (e << wF) + negative;
			e +=	getLargeRandom(intlog2(wF)); // may lead to an overflow, who cares
			y  = getLargeRandom(wF) + (e << wF);
		}
		else if ((i & 7) == 4) {// subtraction, alignment within the mantissa sizes
			mpz_class e = getLargeRandom(wE);
			x  = getLargeRandom(wF) + (e << wF);
			e +=	getLargeRandom(intlog2(wF)); // may lead to an overflow
			if(e > (mpz_class(1)<<(wE)-1))
				e = (mpz_class(1)<<(wE)-1);
			y  = getLargeRandom(wF) + (e << wF) + negative;
		}
		else if ((i & 7) == 5 || (i & 7) == 6) {// addition, alignment within the mantissa sizes
			mpz_class e = getLargeRandom(wE);
			x  = getLargeRandom(wF) + (e << wF);
			e +=	getLargeRandom(intlog2(wF)); // may lead to an overflow
			y  = getLargeRandom(wF) + (e << wF);
		}
		else{ //fully random
			x = getLargeRandom(wE+wF+1);
			y = getLargeRandom(wE+wF+1);
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
		emulate(tc);
		return tc;
	}


	TestList IEEEAdd::unitTest(int index)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		
		if(index==-1) 
		{ // The unit tests

			paramList.push_back(make_pair("wE","5"));
			paramList.push_back(make_pair("wF","10"));		
			testStateList.push_back(paramList);

			paramList.clear();
			paramList.push_back(make_pair("wE","8"));
			paramList.push_back(make_pair("wF","23"));			
			testStateList.push_back(paramList);

			paramList.clear();
			paramList.push_back(make_pair("wE","11"));
			paramList.push_back(make_pair("wF","52"));	
			testStateList.push_back(paramList);

			paramList.clear();
			paramList.push_back(make_pair("wE","15"));
			paramList.push_back(make_pair("wF","112"));			
			testStateList.push_back(paramList);

			paramList.clear();
			paramList.push_back(make_pair("wE","19"));
			paramList.push_back(make_pair("wF","236"));			
			testStateList.push_back(paramList);
		}
		else     
		{
				// finite number of random test computed out of index
		}	

		return testStateList;
	}

	OperatorPtr IEEEAdd::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int wE;
		UserInterface::parseStrictlyPositiveInt(args, "wE", &wE);
		int wF;
		UserInterface::parseStrictlyPositiveInt(args, "wF", &wF);
		return new IEEEAdd(parentOp, target, wE, wF);
	}

	void IEEEAdd::registerFactory(){
		UserInterface::add("IEEEAdd", // name
											 "A floating-point adder with a new, more compact single-path architecture.",
											 "BasicFloatingPoint", // categories
											 "",
											 "wE(int): exponent size in bits; \
											 wF(int): mantissa size in bits;",
											 "",
											 IEEEAdd::parseArguments,
											 IEEEAdd::unitTest
											 ) ;
	}
}
