/*
  Floating Point Adder for FloPoCo

  This file is part of the FloPoCo project

  Authors:   Bogdan Pasca, Florent de Dinechin

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA-Lyon
  2008-2016.

  All rights reserved
  */

// TODO rework the pipeline properly using the newer framework
// TODO move close path prenormalization up to the Swap Difference box
//   if it becomes a part of the critical path

#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <string.h>

#include <gmp.h>
#include <mpfr.h>

#include <gmpxx.h>
#include <utils.hpp>

#include "FPAddDualPath.hpp"
#include "FPAdd.hpp"

using namespace std;

namespace flopoco{

#define DEBUGVHDL 0


	FPAddDualPath::FPAddDualPath(OperatorPtr parentOp, Target* target, int wE, int wF, bool sub, bool onlyPositiveIO) :
		Operator(parentOp, target), wE(wE), wF(wF),  sub(sub), onlyPositiveIO(onlyPositiveIO){

		ostringstream name, synch, synch2;

		srcFileName="FPAddDualPath";

		if(sub)
			name<<"FPSub_";
		else
			name<<"FPAdd_";
		name<<wE<<"_"<<wF;
		
		setNameWithFreqAndUID(name.str());

		setCopyrightString("Jérémie Detrey, Bogdan Pasca, Florent de Dinechin (2008-2017)");

		sizeRightShift = intlog2(wF+3);

		/* Set up the IO signals */
		/* Inputs: 2b(Exception) + 1b(Sign) + wE bits (Exponent) + wF bits(Fraction) */
		// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		addFPInput ("X", wE, wF);
		addFPInput ("Y", wE, wF);
		addFPOutput("R", wE, wF);

		//=========================================================================|
		//                          Swap/Difference                                |
		// ========================================================================|

		vhdl<<"-- Exponent difference and swap  --"<<endl;
		vhdl<<tab<<declare("inX",wE+wF+3) << " <= X;"<<endl;
		vhdl<<tab<<declare("inY",wE+wF+3) << " <= Y;"<<endl;

		// signal which indicates whether or not the exception bits of X are greater or equal than/to the exception bits of Y
		vhdl<<tab<<declare(getTarget()->adderDelay(wE+1), "exceptionXSuperiorY") << " <= '1' when inX("<<wE+wF+2<<" downto "<<wE+wF+1<<") >= inY("<<wE+wF+2<<" downto "<<wE+wF+1<<") else '0';"<<endl;

		// signal which indicates whether or not the exception bits of X are equal to the exception bits of Y
		vhdl<<tab<<declare("exceptionXEqualY") << " <= '1' when inX("<<wE+wF+2<<" downto "<<wE+wF+1<<") = inY("<<wE+wF+2<<" downto "<<wE+wF+1<<") else '0';"<<endl;

		// make the difference between the exponents of X and Y; expX - expY = expX + not(expY) + 1
		// pad exponents with sign bit
		vhdl<<tab<<declare("signedExponentX",wE+1) << " <= \"0\" & inX("<<wE+wF-1<<" downto "<<wF<<");"<<endl;
		vhdl<<tab<<declare("signedExponentY",wE+1) << " <= \"0\" & inY("<<wE+wF-1<<" downto "<<wF<<");"<<endl;
		vhdl<<tab<<declare(getTarget()->adderDelay(wE+1), "exponentDifferenceXY",wE+1) << " <= signedExponentX - signedExponentY ;"<<endl;
		vhdl<<tab<<declare(getTarget()->adderDelay(wE+1), "exponentDifferenceYX",wE) << " <= signedExponentY("<<wE-1<<" downto 0) - signedExponentX("<<wE-1<<" downto 0);"<<endl;

		// SWAP when: [excX=excY and expY>expX] or [excY>excX]
		vhdl<<tab<<declare(getTarget()->logicDelay(), "swap") << " <= (exceptionXEqualY and exponentDifferenceXY("<<wE<<")) or (not(exceptionXSuperiorY));"<<endl;


		string pmY="inY";
		if ( sub ) {
			vhdl << tab << declare(getTarget()->logicDelay(), "mY",wE+wF+3)   << " <= inY" << range(wE+wF+2,wE+wF+1) << " & not(inY"<<of(wE+wF)<<") & inY" << range(wE+wF-1,0) << ";"<<endl;
			pmY = "mY";
		}

		// depending on the value of swap, assign the corresponding values to the newX and newY signals
		vhdl<<tab<<declare("newX",wE+wF+3) << " <= " << pmY << " when swap = '1' else inX;"<<endl;
		vhdl<<tab<<declare("newY",wE+wF+3) << " <= inX when swap = '1' else " << pmY << ";"<<endl;
		vhdl<<tab<<declare("exponentDifference",wE) << " <= " << "exponentDifferenceYX"
			 << " when swap = '1' else exponentDifferenceXY("<<wE-1<<" downto 0);"<<endl;


		// determine if the fractional part of Y was shifted out of the operation //
		if (wE>sizeRightShift){
			vhdl<<tab<<declare(getTarget()->adderDelay(wE-sizeRightShift),  "shiftedOut") << " <= ";
			for (int i=wE-1;i>=sizeRightShift;i--)
				if (i==sizeRightShift)
					vhdl<< "exponentDifference("<<i<<")";
				else
					vhdl<< "exponentDifference("<<i<<") or ";
			vhdl<<";"<<endl;
		}
		else
			vhdl<<tab<<declare("shiftedOut") << " <= '0';"<<endl;

		//shiftVal=the number of positions that fracY must be shifted to the right
		vhdl<<tab<<declare("shiftVal",sizeRightShift) << " <= " ;
		if (wE>sizeRightShift) {
			vhdl << "exponentDifference("<< sizeRightShift-1<<" downto 0)"
				  << " when shiftedOut='0'"<<endl
				  <<tab << tab << "    else CONV_STD_LOGIC_VECTOR("<<wF+3<<","<<sizeRightShift<<") ;" << endl;
		}
		else if (wE==sizeRightShift) {
			vhdl<<tab<<"exponentDifference;" << endl ;
		}
		else 	{ //  wE< sizeRightShift
			vhdl<<tab<<"CONV_STD_LOGIC_VECTOR(0,"<<sizeRightShift-wE <<") & exponentDifference;" <<	endl;
		}

		// compute EffSub as (signA xor signB) at cycle 1
		if(onlyPositiveIO)
		{
			vhdl<<tab<<declare("EffSub") << " <= '0';"<<endl;

		} else {
			vhdl<<tab<<declare(getTarget()->lutDelay(),
							   "EffSub")
				<< " <= newX("<<wE+wF<<") xor newY("<<wE+wF<<");"<<endl;
		}

		// compute the close/far path selection signal at cycle1
		// the close path is considered only when (signA!=signB) and |exponentDifference|<=1
		if(!onlyPositiveIO)
		{
			vhdl<<tab<<declare("selectClosePath") << " <= EffSub when exponentDifference("<<wE-1<<" downto "<<1<<") = ("<<wE-1<<" downto "<<1<<" => '0') else '0';"<<endl;
		}

		// sdExnXY is a concatenation of the exception bits of X and Y, after swap, so exnX > exnY
		vhdl<<tab<<declare("sdExnXY",4) << " <= newX("<<wE+wF+2<<" downto "<<wE+wF+1<<") "
			 << "& newY("<<wE+wF+2<<" downto "<<wE+wF+1<<");"<<endl;
		vhdl<<tab<<declare("pipeSignY") << " <= newY("<<wE+wF<<");"<<endl;


		if(onlyPositiveIO)
		{
			vhdl << tab << declare("resSign") << " <= newX(" << wE + wF << ");" << endl;
		}
		else
		{

			//=========================================================================|
			//                            close path                                   |
			//=========================================================================|


			vhdl << endl << "-- Close Path --" << endl;

			// build the fraction signals
			// padding: [sign bit][inplicit "1"][fracX][guard bit]
			vhdl << tab << declare("fracXClose1", wF + 3) << " <= \"01\" & newX(" << wF - 1 << " downto " << 0
				 << ") & '0';" << endl;

			// the close path is considered when the |exponentDifference|<=1, so
			// the alignment of fracY is of at most 1 position
			vhdl << tab << "with exponentDifference(0) select" << endl;
			vhdl << tab << declare("fracYClose1", wF + 3) << " <=  \"01\" & newY(" << wF - 1 << " downto " << 0
				 << ") & '0' when '0'," << endl;
			vhdl << tab << "               \"001\" & newY(" << wF - 1 << " downto " << 0 << ")       when others;"
				 << endl;

			// substract the fraction signals for the close path;

			// instanciate the box that computes X-Y and Y-X. Note that it could take its inputs before the swap (TODO ?)
			REPORT(DETAILED, "Building close path dual mantissa subtraction box");

			newInstance("IntDualAddSub",
						getName() + "_DualSubClose",
						join("wIn=", wF + 3) + " opType=0",
						"X=>fracXClose1,Y=>fracYClose1",
						"XmY=>fracRClosexMy, YmX=>fracRCloseyMx");

			vhdl << tab << declare("fracSignClose") << " <= fracRClosexMy(" << wF + 2 << ");" << endl;
			vhdl << tab << declare("fracRClose1", wF + 2) << " <= fracRClosexMy(" << wF + 1
				 << " downto 0) when fracSignClose='0' else fracRCloseyMx(" << wF + 1 << " downto 0);" << endl;

			//TODO check the test if significand is all zero is useful.
			vhdl << tab << declare("resSign") << " <= '0' when selectClosePath='1' and fracRClose1 = (" << wF + 1
				 << " downto 0 => '0') else" << endl;
			// else sign(x) xor (close and sign(resclose))
			vhdl << tab << "          newX(" << wE + wF << ") xor (selectClosePath and "
				 << "fracSignClose);" << endl;

			// LZC + Shifting. The number of leading zeros are returned together with the shifted input
			REPORT(DEBUG, "Building close path LZC + shifter");

			lzocs = (LZOCShifterSticky *) newInstance("LZOCShifterSticky",
													  getName() + "_LZCShifter",
													  "countType=0" + join(" wIn=", wF + 2) + join(" wOut=", wF + 2) +
													  join(" wCount=", intlog2(wF + 2)),
													  "I=>fracRClose1",
													  "Count=>nZerosNew,O=>shiftedFrac"
			);
			// NORMALIZATION

			// shiftedFrac(0) is the round bit, shiftedFrac(1) is the parity bit,
			// shiftedFrac(wF) is the leading one, to be discarded
			// the rounding bit is computed:
			vhdl << tab << declare("roundClose0") << " <= shiftedFrac(0) and shiftedFrac(1);" << endl;
			// Is the result zero?
			vhdl << tab << declare("resultCloseIsZero0") << " <= '1' when nZerosNew"
				 << " = CONV_STD_LOGIC_VECTOR(" << (1 << lzocs->getCountWidth()) -
												   1 // Should be wF+2 but this is a bug of LZOCShifterSticky: for all zeroes it returns this value
				 << ", " << lzocs->getCountWidth()
				 << ") else '0';" << endl;

			// add two bits in order to absorb exceptions:
			// the second 0 will become a 1 in case of overflow,
			// the first 0 will become a 1 in case of underflow (negative biased exponent)
			vhdl << tab << declare("exponentResultClose", wE + 2) << " <= (\"00\" & "
				 << "newX(" << wE + wF - 1 << " downto " << wF << ")) "
				 << "- (CONV_STD_LOGIC_VECTOR(0," << wE - lzocs->getCountWidth() + 2 << ") & nZerosNew);"
				 << endl;


			// concatenate exponent with fractional part before rounding so the possible carry propagation automatically increments the exponent
			vhdl << tab << declare("resultBeforeRoundClose", wE + 1 + wF + 1) << " <= exponentResultClose(" << wE + 1
				 << " downto 0) & shiftedFrac(" << wF << " downto 1);" << endl;
			vhdl << tab << declare("roundClose") << " <= roundClose0;" << endl;
			vhdl << tab << declare("resultCloseIsZero") << " <= resultCloseIsZero0;" << endl;
		}




		//=========================================================================|
		//                              far path                                   |
		//=========================================================================|


		vhdl<< endl << "-- Far Path --" << endl;

		//add implicit 1 for frac1.
		vhdl<<tab<< declare("fracNewY",wF+1) << " <= '1' & newY("<<wF-1<<" downto 0);"<<endl;

		// shift right the significand of new Y with as many positions as the exponent difference suggests (alignment)
		REPORT(DEBUG, "Building far path right shifter");
		{
			ostringstream args;
			string inputs, outputs;
			args << "wIn=" << wF+1 << " maxShift=" << wF+3 << " dir=1";
			inputs = "X=>fracNewY, S=>shiftVal";
			outputs = "R=>shiftedFracY";
			newInstance("Shifter", "RightShifterComponent", args.str(), inputs, outputs);
		}
		// compute sticky bit as the or of the shifted out bits during the alignment //
		vhdl<<tab<< declare("sticky") << " <= '0' when (shiftedFracY("<<wF<<" downto 0)=CONV_STD_LOGIC_VECTOR(0,"<<wF+1<<")) else '1';"<<endl;

		//pad fraction of Y [sign][shifted frac having inplicit 1][guard bits]
		vhdl<<tab<< declare("fracYfar", wF+4) << " <= \"0\" & shiftedFracY("<<2*wF+3<<" downto "<<wF+1<<");"<<endl;

		// depending on the signs of the operands, perform addition or substraction
		// the result will be: a + (b xor operation) + operation, where operation=0=addition and operation=1=substraction
		// the operation selector is the xor between the signs of the operands
		// perform xor
		vhdl<<tab<< declare("EffSubVector", wF+4) << " <= ("<<wF+3<<" downto 0 => EffSub);"<<endl;
		vhdl<<tab<<declare("fracYfarXorOp", wF+4) << " <= fracYfar xor EffSubVector;"<<endl;
		//pad fraction of X [sign][inplicit 1][fracX][guard bits]
		vhdl<<tab<< declare("fracXfar", wF+4) << " <= \"01\" & (newX("<<wF-1<<" downto 0)) & \"00\";"<<endl;
		vhdl<<tab<< declare("cInAddFar") << " <= EffSub and not sticky;"<< endl;

		// perform carry in addition
		REPORT(DETAILED, "Building far path adder");
		newInstance("IntAdder", getName()+"_fracAddFar",
								join("wIn=", wF+4),
								"X=>fracXfar,Y=>fracYfarXorOp,Cin=>cInAddFar", "R=>fracResultfar0");


		// fracAddFar = new IntAdder(this, target,wF+4);
		// fracAddFar->changeName(getName()+"_fracAddFar");
		// inPortMap  (fracAddFar, "X", "fracXfar");
		// inPortMap  (fracAddFar, "Y", "fracYfarXorOp");
		// inPortMap  (fracAddFar, "Cin", "cInAddFar");
		// outPortMap (fracAddFar, "R","fracResultfar0");
		// vhdl << instance(fracAddFar, "fracAdderFar");

		vhdl<< tab << "-- 2-bit normalisation" <<endl;
		vhdl<< tab << declare("fracResultFarNormStage", wF+4) << " <= fracResultfar0;"<<endl;

		// NORMALIZATION
		// The leading one may be at position wF+3, wF+2 or wF+1
		//
		vhdl<<tab<< declare("fracLeadingBits", 2) << " <= fracResultFarNormStage("<<wF+3<<" downto "<<wF+2<<") ;" << endl;

		vhdl<<tab<< declare("fracResultFar1",wF) << " <=" << endl ;
		vhdl<<tab<<tab<< "     fracResultFarNormStage("<<wF+0<<" downto 1)  when fracLeadingBits = \"00\" "<<endl;
		vhdl<<tab<<tab<< "else fracResultFarNormStage("<<wF+1<<" downto 2)  when fracLeadingBits = \"01\" "<<endl;
		vhdl<<tab<<tab<< "else fracResultFarNormStage("<<wF+2<<" downto 3);"<<endl;

		vhdl<<tab<< declare("fracResultRoundBit") << " <=" << endl ;
		vhdl<<tab<<tab<< "     fracResultFarNormStage(0) 	 when fracLeadingBits = \"00\" "<<endl;
		vhdl<<tab<<tab<< "else fracResultFarNormStage(1)    when fracLeadingBits = \"01\" "<<endl;
		vhdl<<tab<<tab<< "else fracResultFarNormStage(2) ;"<<endl;

		vhdl<<tab<< declare("fracResultStickyBit") << " <=" << endl ;
		vhdl<<tab<<tab<< "     sticky 	 when fracLeadingBits = \"00\" "<<endl;
		vhdl<<tab<<tab<< "else fracResultFarNormStage(0) or  sticky   when fracLeadingBits = \"01\" "<<endl;
		vhdl<<tab<<tab<< "else fracResultFarNormStage(1) or fracResultFarNormStage(0) or sticky;"<<endl;
		// round bit
		vhdl<<tab<< declare("roundFar1") <<" <= fracResultRoundBit and (fracResultStickyBit or fracResultFar1(0));"<<endl;

		//select operation mode. This depends on wether or not the exponent must be adjusted after normalization
		vhdl<<tab<<declare("expOperationSel",2) << " <= \"11\" when fracLeadingBits = \"00\" -- add -1 to exponent"<<endl;
		vhdl<<tab<<"            else   \"00\" when fracLeadingBits = \"01\" -- add 0 "<<endl;
		vhdl<<tab<<"            else   \"01\";                              -- add 1"<<endl;

		//the second operand depends on the operation selector
		vhdl<<tab<<declare("exponentUpdate",wE+2) << " <= ("<<wE+1<<" downto 1 => expOperationSel(1)) & expOperationSel(0);"<<endl;

		// the result exponent before normalization and rounding is = to the exponent of the first operand //
		vhdl<<tab<<declare("exponentResultfar0",wE+2) << "<=\"00\" & (newX("<<wF+wE-1<<" downto "<<wF<<"));"<<endl;

		vhdl<<tab<<declare("exponentResultFar1",wE+2) << " <= exponentResultfar0 + exponentUpdate;" << endl;

		// End of normalization stage
		vhdl<<tab<<declare("resultBeforeRoundFar",wE+1 + wF+1) << " <= "
			 << "exponentResultFar1 & fracResultFar1;" << endl;
		vhdl<<tab<< declare("roundFar") << " <= roundFar1;" << endl;





		//=========================================================================|
		//                              Synchronization                            |
		//=========================================================================|
		if(onlyPositiveIO)
		{
			vhdl<<tab<< declare("resultBeforeRound", wE+1 + wF+1) << " <= resultBeforeRoundFar;"<<endl;
			vhdl<<tab<< declare("round") << " <= roundFar;"<<endl;

			vhdl<<tab<< declare("zeroFromClose") << " <= '0';" <<endl; //this could be optimized
		}
		else
		{
			vhdl<<endl<<"-- Synchronization of both paths --"<<endl;

			double muxDelay= getTarget()->lutDelay(); // estimated delay so far (one mux)
			// select between the results of the close or far path as the result of the operation
			vhdl<<tab<< "with selectClosePath select"<<endl;
			vhdl<<tab<< declare(muxDelay, "resultBeforeRound", wE+1 + wF+1)
				<< " <= resultBeforeRoundClose when '1',"<<endl;
			vhdl<<tab<< "                     resultBeforeRoundFar   when others;"<<endl;
			vhdl<<tab<< "with selectClosePath select"<<endl;
			vhdl<<tab<< declare(muxDelay, "round") << " <= roundClose when '1',"<<endl;
			vhdl<<tab<< "         roundFar   when others;"<<endl;

			vhdl<<tab<< declare("zeroFromClose") << " <= selectClosePath and resultCloseIsZero;" <<endl;
		}


		vhdl<< endl << "-- Rounding --" << endl;

		REPORT(DETAILED, "Building final round adder");
		// finalRoundAdd will add the mantissa concatenated with exponent, two bits reserved for possible under/overflow

		vhdl<<tab<< declare("zeroInput",2+wE+wF) << " <= " << zg(2+wE+wF)<< ";" <<endl;
		
		newInstance("IntAdder",
								getName()+"_finalRoundAdd",
								join("wIn=", wE + wF + 2),
								"X=>resultBeforeRound,Y=>zeroInput,Cin=>round", "R=>resultRounded");

		// ostringstream zero;
		// zero<<"("<<1+wE+wF<<" downto 0 => '0') ";

		// finalRoundAdd = new IntAdder(this, target, wE + wF + 2);
		// finalRoundAdd->changeName(getName()+"_finalRoundAdd");

		// inPortMap   (finalRoundAdd, "X", "resultBeforeRound");
		// inPortMapCst(finalRoundAdd, "Y", zero.str() );
		// inPortMap   (finalRoundAdd, "Cin", "round");
		// outPortMap  (finalRoundAdd, "R","resultRounded");
		// vhdl << instance(finalRoundAdd, "finalRoundAdder");


		// We neglect the delay of the rest
		vhdl<<tab<<declare("syncEffSub") << " <= EffSub;"<<endl;

		//X
		vhdl<<tab<<declare("syncX",3+wE+wF) << " <= newX;"<<endl;

		//signY
		vhdl<<tab<<declare("syncSignY") << " <= pipeSignY;"<<endl;

		// resSign comes from closer
		vhdl<<tab<<declare("syncResSign") << " <= resSign;"<<endl;

		// compute the exception bits of the result considering the possible underflow and overflow
		vhdl<<tab<< declare("UnderflowOverflow",2) << " <= resultRounded"<<range( wE+1+wF, wE+wF)<<";"<<endl;

		vhdl<<tab<< "with UnderflowOverflow select"<<endl;
		vhdl<<tab<< declare(getTarget()->lutDelay(), "resultNoExn",wE+wF+3)
				<< "("<<wE+wF+2<<" downto "<<wE+wF+1<<") <=   (not zeroFromClose) & \"0\" when \"01\", -- overflow"<<endl;
		vhdl<<tab<< "                              \"00\" when \"10\" | \"11\",  -- underflow"<<endl;
		vhdl<<tab<< "                              \"0\" &  not zeroFromClose  when others; -- normal "<<endl;

		vhdl<<tab<< "resultNoExn("<<wE+wF<<" downto 0) <= syncResSign & resultRounded("<<wE+wF-1<<" downto 0);"<<endl;

		vhdl<<tab<< declare("syncExnXY", 4) << " <= sdExnXY;"<<endl;
		vhdl<<tab<< "-- Exception bits of the result" << endl;
		vhdl<<tab<< "with syncExnXY select -- remember that ExnX > ExnY "<<endl;
		vhdl<<tab<<tab<< declare(getTarget()->lutDelay(), "exnR",2)
				<<" <= resultNoExn("<<wE+wF+2<<" downto "<<wE+wF+1<<") when \"0101\","<<endl;
		vhdl<<tab<<tab<< "        \"1\" & syncEffSub          when \"1010\","<<endl;
		vhdl<<tab<<tab<< "        \"11\"                      when \"1110\","<<endl;
		vhdl<<tab<<tab<< "        syncExnXY(3 downto 2)     when others;"<<endl;
		vhdl<<tab<< "-- Sign bit of the result" << endl;
		vhdl<<tab<< "with syncExnXY select"<<endl;
		vhdl<<tab<<tab<<declare(getTarget()->lutDelay(), "sgnR")
				<< " <= resultNoExn("<<wE+wF<<")         when \"0101\","<<endl;
		vhdl<<tab<< "           syncX("<<wE+wF<<") and syncSignY when \"0000\","<<endl;
		vhdl<<tab<< "           syncX("<<wE+wF<<")               when others;"<<endl;

		vhdl<<tab<< "-- Exponent and significand of the result" << endl;
		vhdl<<tab<< "with syncExnXY select  "<<endl;
		vhdl<<tab<<tab<< declare(getTarget()->lutDelay(), "expsigR", wE+wF)
				<< " <= resultNoExn("<<wE+wF-1<<" downto 0)   when \"0101\" ,"<<endl;
		vhdl<<tab<<tab<< "           syncX("<<wE+wF-1<<" downto  0)        when others; -- 0100, or at least one NaN or one infty "<<endl;

		// assign result
		vhdl<<tab<< "R <= exnR & sgnR & expsigR;"<<endl;

	}

	FPAddDualPath::~FPAddDualPath() {
	}










	void FPAddDualPath::emulate(TestCase * tc)
	{
		// use the generic one defined in FPAdd
		FPAdd::emulate(tc, wE, wF, sub);
	}





	void FPAddDualPath::buildStandardTestCases(TestCaseList* tcl){
		// use the generic one defined in FPAdd
		FPAdd::buildStandardTestCases(this, wE, wF, tcl, onlyPositiveIO);
	}

 

	TestCase* FPAddDualPath::buildRandomTestCase(int i){
		// use the generic one defined in FPAdd
		return FPAdd::buildRandomTestCase(this, i, wE, wF, sub, onlyPositiveIO);
	}


}
