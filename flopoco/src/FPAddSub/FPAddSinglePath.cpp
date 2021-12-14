/*
  Floating Point Adder for FloPoCo

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Authors:   Bogdan Pasca, Florent de Dinechin

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, 2008-2010.
  All right reserved.

  */

#include "FPAddSinglePath.hpp"
#include "FPAdd.hpp"

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

//TODO +- inf for exponent => update exception

namespace flopoco{


#define DEBUGVHDL 0


  FPAddSinglePath::FPAddSinglePath(OperatorPtr parentOp, Target* target,
																	 int wE, int wF,
																	 bool sub) :
		Operator(parentOp, target), wE(wE), wF(wF), sub(sub) {

		srcFileName="FPAddSinglePath";

		ostringstream name;
		if(sub)
			name<<"FPSub_";
		else
			name<<"FPAdd_";

		name <<wE<<"_"<<wF;
		setNameWithFreqAndUID(name.str());

		setCopyrightString("Florent de Dinechin, Bogdan Pasca (2010-2017)");

		sizeRightShift = intlog2(wF+3 );
		REPORT(DEBUG, "sizeRightShift = " <<  sizeRightShift);
		/* Set up the IO signals */
		/* Inputs: 2b(Exception) + 1b(Sign) + wE bits (Exponent) + wF bits(Fraction) */
		// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		addFPInput ("X", wE, wF);
		addFPInput ("Y", wE, wF);
		addFPOutput("R", wE, wF);

		//=========================================================================|
		//                          Swap/Difference                                |
		// ========================================================================|
		// In principle the comparison could be done on the exponent only, but then the result of the effective addition could be negative,
		// so we would have to take its absolute value, which would cost the same as comparing exp+frac here.
		vhdl << tab << declare("excExpFracX",2+wE+wF) << " <= X"<<range(wE+wF+2, wE+wF+1) << " & X"<<range(wE+wF-1, 0)<<";"<<endl;
		vhdl << tab << declare("excExpFracY",2+wE+wF) << " <= Y"<<range(wE+wF+2, wE+wF+1) << " & Y"<<range(wE+wF-1, 0)<<";"<<endl;



		vhdl << tab << declare("addCmpOp1",wE+wF+3) << " <= '0'  & excExpFracX;"<<endl;
		vhdl << tab << declare("addCmpOp2",wE+wF+3) << " <= '1'  & not excExpFracY;"<<endl;
		{Signal* i=getSignalByName("addCmpOp1");
			REPORT(DEBUG, "signal " << i->getName() <<  "  Cycle=" << i->getCycle() <<  "  criticalPath=" << i->getCriticalPath() );
	}
		vhdl << tab << declare("addCmpCin") << " <= '1';"<<endl;
		newInstance("IntAdder", "cmpAdder", join("wIn=", (wE+wF+3)), "X=>addCmpOp1,Y=>addCmpOp2,Cin=>addCmpCin", "R=>cmpRes");

		vhdl<< tab << declare(getTarget()->fanoutDelay(2*(wE+wF+3) + 2*wE), "swap")  << " <= cmpRes"<<of(wE+wF+2)<<";"<<endl;

		addComment("exponent difference");
		vhdl<< tab << declare(getTarget()->adderDelay(wE+1),
													"eXmeY",wE)	<< " <= (X"<<range(wE+wF-1,wF)<<") - (Y"<<range(wE+wF-1,wF)<<");"<<endl;
		vhdl<< tab << declare(getTarget()->adderDelay(wE+1),
													"eYmeX",wE) << " <= (Y"<<range(wE+wF-1,wF)<<") - (X"<<range(wE+wF-1,wF)<<");"<<endl;
		vhdl<<tab<<declare(getTarget()->logicDelay(3),
											 "expDiff",wE) << " <= eXmeY when swap = '0' else eYmeX;"<<endl;


		string pmY="Y";
		if ( sub ) {
			vhdl << tab << declare(getTarget()->logicDelay(1), "mY", wE+wF+3)
					<< " <= Y" << range(wE+wF+2,wE+wF+1) << " & not(Y"<<of(wE+wF)<<") & Y" << range(wE+wF-1,0) << ";"<<endl;
			pmY = "mY";
		}

		// depending on the value of swap, assign the corresponding values to the newX and newY signals

		// REPORT(INFO, "Fan-out delay = " << getTarget()->localWireDelay(wE+wF+3));
		addComment("input swap so that |X|>|Y|");
		vhdl<<tab<<declare( getTarget()->logicDelay(3),
											 "newX",wE+wF+3) << " <= X when swap = '0' else "<< pmY << ";"<<endl;
		vhdl<<tab<<declare( getTarget()->logicDelay(3),
											 "newY",wE+wF+3) << " <= " << pmY <<" when swap = '0' else X;"<<endl;

		//break down the signals
		addComment("now we decompose the inputs into their sign, exponent, fraction");
		vhdl << tab << declare("expX",wE) << "<= newX"<<range(wE+wF-1,wF)<<";"<<endl;
		vhdl << tab << declare("excX",2)  << "<= newX"<<range(wE+wF+2,wE+wF+1)<<";"<<endl;
		vhdl << tab << declare("excY",2)  << "<= newY"<<range(wE+wF+2,wE+wF+1)<<";"<<endl;
		vhdl << tab << declare("signX")   << "<= newX"<<of(wE+wF)<<";"<<endl;
		vhdl << tab << declare("signY")   << "<= newY"<<of(wE+wF)<<";"<<endl;
		vhdl << tab << declare(getTarget()->logicDelay(2),
													 "EffSub") << " <= signX xor signY;"<<endl;
		vhdl << tab << declare("sXsYExnXY",6) << " <= signX & signY & excX & excY;"<<endl;
		vhdl << tab << declare("sdExnXY",4) << " <= excX & excY;"<<endl;

		vhdl << tab << declare(getTarget()->logicDelay(2),
				"fracY",wF+1) << " <= "<< zg(wF+1)<<" when excY=\"00\" else ('1' & newY("<<wF-1<<" downto 0));"<<endl;

		addComment("Exception management logic");
		vhdl <<tab<<"with sXsYExnXY select "<<endl;
		vhdl <<tab<<declare(getTarget()->logicDelay(6),"excRt",2) << " <= \"00\" when \"000000\"|\"010000\"|\"100000\"|\"110000\","<<endl
				 <<tab<<tab<<"\"01\" when \"000101\"|\"010101\"|\"100101\"|\"110101\"|\"000100\"|\"010100\"|\"100100\"|\"110100\"|\"000001\"|\"010001\"|\"100001\"|\"110001\","<<endl
				 <<tab<<tab<<"\"10\" when \"111010\"|\"001010\"|\"001000\"|\"011000\"|\"101000\"|\"111000\"|\"000010\"|\"010010\"|\"100010\"|\"110010\"|\"001001\"|\"011001\"|\"101001\"|\"111001\"|\"000110\"|\"010110\"|\"100110\"|\"110110\", "<<endl
				 <<tab<<tab<<"\"11\" when others;"<<endl;
		
		vhdl <<tab<<declare(getTarget()->logicDelay(2), "signR")
				 << "<= '0' when (sXsYExnXY=\"100000\" or sXsYExnXY=\"010000\") else signX;"<<endl;


		vhdl<<tab<<declare(getTarget()->eqConstComparatorDelay(wE+1), "shiftedOut")
				<< " <= '1' when (expDiff > "<<wF+2<<") else '0';"<<endl;
		//shiftVal=the number of positions that fracY must be shifted to the right

		if (wE>sizeRightShift) {
			vhdl<<tab<<declare(getTarget()->logicDelay(2), "shiftVal",sizeRightShift)
					<< " <= expDiff("<< sizeRightShift-1<<" downto 0)"
					//<< " when shiftedOut='0' else \"" << unsignedBinary(wF+3,sizeRightShift) << "\";" << endl;  // was CONV_STD_LOGIC_VECTOR("<<wF+3<<","<<sizeRightShift<<")
					<< " when shiftedOut='0' else CONV_STD_LOGIC_VECTOR("<<wF+3<<","<<sizeRightShift<<");" << endl;
		}
		else if (wE==sizeRightShift) {
 			vhdl<<tab<<declare("shiftVal", sizeRightShift) << " <= expDiff" << range(sizeRightShift-1,0) << ";" << endl ;
		}
		else 	{ //  wE< sizeRightShift
			//vhdl<<tab<<declare("shiftVal",sizeRightShift) << " <= " << zg(sizeRightShift-wE) << " & expDiff;" << endl;  // was CONV_STD_LOGIC_VECTOR(0,"<<sizeRightShift-wE <<") & expDiff;" <<	endl;
			vhdl<<tab<<declare("shiftVal",sizeRightShift) << " <= CONV_STD_LOGIC_VECTOR("<<sizeRightShift-wE <<", 0) & expDiff;" <<	endl;
		}

		// shift right the significand of new Y with as many positions as the exponent difference suggests (alignment)
		REPORT(DETAILED, "Building right shifter");

		newInstance("Shifter",
								"RightShifterComponent",
								"wIn=" + to_string(wF+1) + " maxShift=" + to_string(wF+3) + " dir=1",
								"X=>fracY,S=>shiftVal",
								"R=>shiftedFracY");
		
		vhdl<<tab<< declare(getTarget()->eqConstComparatorDelay(wF+1), "sticky") 
				<< " <= '0' when (shiftedFracY("<<wF<<" downto 0) = " << zg(wF+1) << ") else '1';"<<endl;
		
		//pad fraction of Y [overflow][shifted frac having inplicit 1][guard][round]
		vhdl<<tab<< declare("fracYpad", wF+4)      << " <= \"0\" & shiftedFracY("<<2*wF+3<<" downto "<<wF+1<<");"<<endl;
		vhdl<<tab<< declare("EffSubVector", wF+4) << " <= ("<<wF+3<<" downto 0 => EffSub);"<<endl;
		vhdl<<tab<< declare(getTarget()->logicDelay(2), "fracYpadXorOp", wF+4)
				<< " <= fracYpad xor EffSubVector;"<<endl;
		//pad fraction of X [overflow][inplicit 1][fracX][guard bits]
		vhdl<<tab<< declare("fracXpad", wF+4)      << " <= \"01\" & (newX("<<wF-1<<" downto 0)) & \"00\";"<<endl;

		vhdl<<tab<< declare(getTarget()->logicDelay(2), "cInSigAdd")
				<< " <= EffSub and not sticky; -- if we subtract and the sticky was one, some of the negated sticky bits would have absorbed this carry "<< endl;

		//result is always positive.

		newInstance("IntAdder", "fracAdder", join("wIn=",wF+4), "X=>fracXpad,Y=>fracYpadXorOp,Cin=>cInSigAdd","R=>fracAddResult");
		
		//shift in place
		vhdl << tab << declare("fracSticky",wF+5) << "<= fracAddResult & sticky; "<<endl;

		LZOCShifterSticky* lzocs = (LZOCShifterSticky*)
			newInstance("LZOCShifterSticky",
									"LZCAndShifter",
									"wIn=" + to_string(wF+5) + " wOut=" + to_string(wF+5) + " wCount=" + to_string(intlog2(wF+5)) + " computeSticky=false countType=0",
									"I=>fracSticky",
									"Count=>nZerosNew, O=>shiftedFrac");

		// pipeline: I am assuming the two additions can be merged in a row of luts but I am not sure
		vhdl << tab << declare("extendedExpInc",wE+2) << "<= (\"00\" & expX) + '1';"<<endl;

		vhdl << tab << declare(getTarget()->adderDelay(wE+2),
													 "updatedExp",wE+2) << " <= extendedExpInc - (" << zg(wE+2-lzocs->getCountWidth(),0) <<" & nZerosNew);"<<endl;
		vhdl << tab << declare("eqdiffsign")<< " <= '1' when nZerosNew="<<og(lzocs->getCountWidth(),0)<<" else '0';"<<endl;


		//concatenate exponent with fraction to absorb the possible carry out
		vhdl<<tab<<declare("expFrac",wE+2+wF+1)<<"<= updatedExp & shiftedFrac"<<range(wF+3,3)<<";"<<endl;

		vhdl<<tab<<declare("stk")<<"<= shiftedFrac"<<of(2)<<" or shiftedFrac"<<of(1)<<" or shiftedFrac"<<of(0)<<";"<<endl;
		vhdl<<tab<<declare("rnd")<<"<= shiftedFrac"<<of(3)<<";"<<endl;
		vhdl<<tab<<declare("lsb")<<"<= shiftedFrac"<<of(4)<<";"<<endl;

		vhdl << tab << declare(getTarget()->logicDelay(4),"needToRound")
				 <<"<= '1' when (rnd='1' and stk='1') or (rnd='1' and stk='0' and lsb='1')" << endl
				 << "  else '0';"<<endl;

		vhdl << tab  << declare("zeros", wE+2+wF+1) << "  <= " <<  zg(wE+2+wF+1,0)<<";"<<endl;
		
		newInstance("IntAdder", "roundingAdder", join("wIn=",wE+2+wF+1), "X=>expFrac,Y=>zeros,Cin=>needToRound","R=>RoundedExpFrac");
		
		addComment("possible update to exception bits");
		vhdl << tab << declare("upExc",2)<<" <= RoundedExpFrac"<<range(wE+wF+2,wE+wF+1)<<";"<<endl;
		vhdl << tab << declare("fracR",wF)<<" <= RoundedExpFrac"<<range(wF,1)<<";"<<endl;
		vhdl << tab << declare("expR",wE) <<" <= RoundedExpFrac"<<range(wF+wE,wF+1)<<";"<<endl;

		vhdl << tab << declare("exExpExc",4) << " <= upExc & excRt;"<<endl;
		vhdl << tab << "with exExpExc select "<<endl;
		vhdl << tab << declare(getTarget()->logicDelay(4),
													 "excRt2",2)
				 << "<= \"00\" when \"0000\"|\"0100\"|\"1000\"|\"1100\"|\"1001\"|\"1101\","<<endl
				 <<tab<<tab<<"\"01\" when \"0001\","<<endl
				 <<tab<<tab<<"\"10\" when \"0010\"|\"0110\"|\"1010\"|\"1110\"|\"0101\","<<endl
				 <<tab<<tab<<"\"11\" when others;"<<endl;
		vhdl<<tab<<declare(getTarget()->logicDelay(3),
											 "excR",2) << " <= \"00\" when (eqdiffsign='1' and EffSub='1'  and not(excRt=\"11\")) else excRt2;"<<endl;
		// IEEE standard says in 6.3: if exact sum is zero, it should be +zero in RN
		vhdl<<tab<<declare(getTarget()->logicDelay(3), "signR2")
				<< " <= '0' when (eqdiffsign='1' and EffSub='1') else signR;"<<endl;


		// assign result
		vhdl<<tab<< declare("computedR",wE+wF+3) << " <= excR & signR2 & expR & fracR;"<<endl;
		vhdl << tab << "R <= computedR;"<<endl;
	}



	FPAddSinglePath::~FPAddSinglePath() {
	}


	void FPAddSinglePath::emulate(TestCase * tc)
	{
		// use the generic one defined in FPAdd
		FPAdd::emulate(tc, wE, wF, sub);
	}





	void FPAddSinglePath::buildStandardTestCases(TestCaseList* tcl){
		// use the generic one defined in FPAdd
		// Although standard test cases may be architecture-specific, it can't hurt.
		FPAdd::buildStandardTestCases(this, wE, wF, tcl);
	}



	TestCase* FPAddSinglePath::buildRandomTestCase(int i){
		// use the generic one defined in FPAdd
		return FPAdd::buildRandomTestCase(this, i, wE, wF, sub);
	}


}
