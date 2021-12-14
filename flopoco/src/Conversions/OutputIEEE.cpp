/*
  Conversion from  FloPoCo format to IEEE-like compact floating-point format

  This file is part of the FloPoCo project developed by the Arenaire
  team at Ecole Normale Superieure de Lyon
  
  Author : Fabrizio Ferrandi ferrandi@elet.polimi.it

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, 2009. All right reserved.

*/



#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <string.h>

#include <gmp.h>
#include <mpfr.h>

#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"
#include "OutputIEEE.hpp"

using namespace std;

namespace flopoco{

#define DEBUGVHDL 0

	OutputIEEE::OutputIEEE(OperatorPtr parentOp, Target* target, int wEI, int wFI, int wEO, int wFO, bool onlyPositiveZeroes) :
			Operator(parentOp, target), wEI(wEI), wFI(wFI), wEO(wEO), wFO(wFO), onlyPositiveZeroes(onlyPositiveZeroes)  {

		setCopyrightString("F. Ferrandi  (2009-2012)");

		ostringstream name;

		name<<"OutputIEEE_"<<wEI<<"_"<<wFI<<"_to_"<<wEO<<"_"<<wFO;

		uniqueName_ = name.str();

		// -------- Parameter set up -----------------

		addFPInput ("X", wEI, wFI);
		addOutput("R", wEO+wFO+1);

		vhdl << tab << declare("fracX", wFI) << "  <= X" << range(wFI-1, 0) << ";" << endl;
		vhdl << tab << declare("exnX",2) << "  <= X" << range(wEI+wFI+2, wEI+wFI+1) << ";" << endl;

		if(wEI==wEO){
			vhdl << tab << declare("expX", wEI) << "  <= X" << range(wEI+wFI-1, wFI) << ";" << endl;
		} else if(wEI<wEO) {

			/* Now, we have to adjust the exponent to a wider word size and a new bias.
			 * For that, we have to compute Enew = Eold + biasCorr with biasCorr = biasO - biasI where biasI and bias= are the input and output biases (biasI = (1<<(wEI-1))-1, biasO = (1<<(wEO-1))-1)
			 * This could be done by this line:
			 *   vhdl << tab << declare(getTarget()->adderDelay(wEO),"expX", wEO) << "  <= (" << zg(wEO-wEI) << " & X" << range(wEI+wFI-1, wFI) << ") + conv_std_logic_vector(" << biasCorr << "," << wEO << ");" << endl;
			 * Instead, as the wEI-1 LSBs are always zero and all the upper bits are one in biasCorr, we can utilize this to perform this constant addition by simple negations:
	        */
			vhdl << tab << declare(getTarget()->lutDelay(), "expX", wEO) << "  <= X" << of(wEI+wFI-1);
			for(int i=0; i < wEO-wEI; i++)
			{
				vhdl << " & not X" << of(wEI+wFI-1);
			}
			vhdl << " & X" << range(wEI+wFI-2, wFI) << ";" << endl;
		}

		if(onlyPositiveZeroes)
			vhdl << tab << declare(getTarget()->lutDelay(), "sX") << "  <= X(" << wEI+wFI << ") when (exnX = \"01\" or exnX = \"10\") else '0';" << endl;
		else
			vhdl << tab << declare(getTarget()->lutDelay(), "sX") << "  <= X(" << wEI+wFI << ") when (exnX = \"01\" or exnX = \"10\" or exnX = \"00\") else '0';" << endl;
		vhdl << tab << declare(getTarget()->lutDelay(), "expZero") << "  <= '1' when expX = " << rangeAssign(wEI-1,0, "'0'") << " else '0';" << endl;

		if((wEI==wEO) || (wEI < wEO)){ //TODO Subnormal inputs need to be normalized in case wEI < wEO
			vhdl << tab << "-- since we have one more exponent value than IEEE (field 0...0, value emin-1)," << endl
				 << tab << "-- we can represent subnormal numbers whose mantissa field begins with a 1" << endl;

			if(wFO>=wFI){
				vhdl << tab << declare(getTarget()->lutDelay(), "fracR",wFO) << " <= " << endl
					 << tab << tab << zg(wFO) << " when (exnX = \"00\") else" << endl //zero
					 << tab << tab << "'1' & fracX" << range(wFI-1, wFO>wFI?0:1) << " & " << zg(wFO-wFI-(wFO>wFI?1:0)) << " when (expZero = '1' and exnX = \"01\") else" << endl  //subnormal, no rounding is performed here when wFO==wFI
					 << tab << tab << "fracX " << " & " << zg(wFO-wFI) << " when (exnX = \"01\") else " << endl //normal number
					 << tab << tab << zg(wFO-1) << " & exnX(0);" << endl; //+/- infty or NaN
				vhdl << tab << declare(getTarget()->lutDelay(), "expR",wEO) << " <=  " << endl
					 << tab << tab << rangeAssign(wEO-1,0, "'0'") << " when (exnX = \"00\") else" << endl
					 << tab << tab << "expX when (exnX = \"01\") else " << endl
					 << tab << tab << rangeAssign(wEO-1,0, "'1'") << ";" << endl;

			}
			else { // wFI > wFO, wEI==wEO
				vhdl << tab << declare(getTarget()->lutDelay(), "sfracX",wFI) << " <= '1' & fracX" << range(wFI-1,1) << " when (expZero = '1' and exnX = \"01\") else fracX;" << endl;
				vhdl << tab << "-- wFO < wFI, need to round fraction" << endl;
				vhdl << tab << declare("resultLSB") << " <= sfracX("<< wFI-wFO <<");" << endl;
				vhdl << tab << declare("roundBit") << " <= sfracX("<< wFI-wFO-1 <<");" << endl;
				// need to define a sticky bit
				vhdl << tab << declare(getTarget()->lutDelay(), "sticky") << " <= ";
				if(wFI-wFO>1){
					vhdl<< " '0' when sfracX" << range(wFI-wFO-2, 0) <<" = CONV_STD_LOGIC_VECTOR(0," << wFI-wFO-2 <<") else '1';"<<endl;
				}
				else {
					vhdl << "'0';" << endl;
				} // end of sticky computation
				vhdl << tab << declare(getTarget()->lutDelay(), "round") << " <= roundBit and (sticky or resultLSB);"<<endl;

				vhdl << tab << "-- The following addition will not overflow since FloPoCo format has one more exponent value" <<endl;
				vhdl << tab << declare(getTarget()->adderDelay(wEO+wFO), "expfracR0", wEO+wFO) << " <= (expX & sfracX" << range(wFI-1, wFI-wFO) << ")  +  (CONV_STD_LOGIC_VECTOR(0," << wEO+wFO-1 <<") & round);"<<endl;

				vhdl << tab << declare(getTarget()->lutDelay(), "fracR",wFO) << " <= " << endl
					 << tab << tab << rangeAssign(wFO-1,0, "'0'") << " when (exnX = \"00\") else" << endl
					 << tab << tab << "expfracR0" << range(wFO-1, 0) << " when (exnX = \"01\") else " << endl
					 << tab << tab << rangeAssign(wFO-1,1, "'0'") << " & exnX(0);" << endl;

				vhdl << tab << declare(getTarget()->lutDelay(), "expR",wEO) << " <=  " << endl
					 << tab << tab << rangeAssign(wEO-1,0, "'0'") << " when (exnX = \"00\") else" << endl
					 << tab << tab << "expfracR0" << range(wFO+wEO-1, wFO) << " when (exnX = \"01\") else " << endl
					 << tab << tab << rangeAssign(wEO-1,0, "'1'") << ";" << endl;
			}

		}
		else {
			throw  string("OutputIEEE not yet implemented for wEI>wEO, send us a mail if you need it");
		}

		vhdl << tab << "R <= sX & expR & fracR; " << endl;

	}

	OutputIEEE::~OutputIEEE() {
	}






	void OutputIEEE::emulate(TestCase * tc)
	{
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");

		/* Compute correct value */
		FPNumber fpx(wEI, wFI, svX);
		mpfr_t x, r;
		mpfr_init2(x, 1+wFI);
		mpfr_init2(r, 1+wFO);
		fpx.getMPFR(x);

		mpfr_set(r, x, GMP_RNDN); ///TODO probably not enough
		FPNumber  fpr(wEO, wFO, r);

		/* Set outputs */
		mpz_class svr= fpr.getSignalValue();
		// Remove FloPoCo encoding
		mpz_class negative  = mpz_class(1)<<(wEO+wFO);
		if((((mpz_class(1)<<(wEO+wFO+2)) & svr) != 0) && (((mpz_class(1)<<(wEO+wFO+1)) & svr) != 0))
		{
			svr = (((mpz_class(1)<<wEO)-1) << wFO) + 1; //NaN
		}
		else if((mpz_class(1)<<(wEO+wFO+2) & svr) != 0)
		{
			svr = (svr & negative) + (((mpz_class(1)<<wEO)-1) << wFO); //+/- infty
		}
		else if((mpz_class(1)<<(wEO+wFO+1) & svr) != 0)
		{
			mpz_class exp = svr & ((mpz_class(1)<<(wFO+wEO))-(mpz_class(1)<<(wFO)));
			if(exp == 0)
			{
				//subnormal:
				svr = (svr & negative) + (mpz_class(1)<<(wFO-1)) + ((svr & ((mpz_class(1)<<wFO)-1)) >> 1); //no rounding is performed here (as in implementation)
			} else {
				//normal floating point number:
				svr = svr & ((mpz_class(1)<<(1+wFO+wEO))-1); //normal number
			}
		}
		else if(onlyPositiveZeroes)
		{
			svr = svr & ((mpz_class(1)<<(wFO+wEO))-1); //zero, sign bit forced to zero
		}
		else
		{
			svr = svr & ((mpz_class(1)<<(1+wFO+wEO))-1); //zero, sign bit identical to input
		}

		tc->addExpectedOutput("R", svr);

		mpfr_clears(x, r, NULL);
	}



	OperatorPtr OutputIEEE::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int wEIn, wFIn, wEOut, wFOut;
		bool onlyPositiveZeroes;
		UserInterface::parseStrictlyPositiveInt(args, "wEIn", &wEIn);
		UserInterface::parseStrictlyPositiveInt(args, "wFIn", &wFIn);
		UserInterface::parseStrictlyPositiveInt(args, "wEOut", &wEOut);
		UserInterface::parseStrictlyPositiveInt(args, "wFOut", &wFOut);
		UserInterface::parseBoolean(args, "onlyPositiveZeroes", &onlyPositiveZeroes);
		return new OutputIEEE(parentOp, target, wEIn, wFIn, wEOut, wFOut, onlyPositiveZeroes);
	}

	void OutputIEEE::registerFactory(){
		UserInterface::add("OutputIEEE", // name
						   "Conversion from FloPoCo to IEEE-754-like floating-point formats.",
						   "Conversions",
						   "", // seeAlso
						   "wEIn(int): input exponent size in bits; \
                        wFIn(int): input mantissa size in bits;\
                        wEOut(int): output exponent size in bits; \
                        wFOut(int): output mantissa size in bits;\
                        onlyPositiveZeroes(bool)=false: when true, normalize +0 and -0 to +0",
						   "", // htmldoc
						   OutputIEEE::parseArguments
		) ;
	}
}

