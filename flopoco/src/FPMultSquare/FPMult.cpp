/*
  Floating Point Multiplier for FloPoCo

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author: Bogdan Pasca

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.

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
#include "FPMult.hpp"
#include "TestBenches/FPNumber.hpp"

using namespace std;

namespace flopoco{

	FPMult::FPMult(OperatorPtr parentOp, Target* target, int wEX, int wFX, int wEY, int wFY, int wER, int wFR,
	                           bool norm, bool correctlyRounded, float dspOccupationThreshold) :
		Operator(parentOp, target), wEX_(wEX), wFX_(wFX), wEY_(wEY), wFY_(wFY), wER_(wER), wFR_(wFR), normalized_(norm), correctlyRounded_(correctlyRounded), dspOccupationThreshold(dspOccupationThreshold)  {

		ostringstream name;
		name << "FPMult_"<<wEX_<<"_"<<wFX_<<"_"<<wEY_<<"_"<<wFY_<<"_"<<wER_<<"_"<<wFR_<<"_uid"<<getNewUId();
		setNameWithFreqAndUID(name.str());
		setCopyrightString("Bogdan Pasca, Florent de Dinechin 2008-2011");


		addFPInput ("X", wEX_, wFX_);
		addFPInput ("Y", wEY_, wFY_);
		if(normalized_)
			addFPOutput ("R"   , wER , wFR);
		else{
			wFR_ = 2 + wFX_ + wFY_;
			addOutput  ("ResultExponent"   , wER_    );
			addOutput ("ResultSignificand", wFR_    );
			addOutput ("ResultException"  , 2      );
			addOutput("ResultSign"       );
		}

		/* Sign Handling -- no need to count it in the critical path */
		vhdl << tab << declare(getTarget()->lutDelay(), "sign") << " <= X" << of(wEX_+wFX) << " xor Y" << of(wEY_+wFY) << ";" << endl;

		/* Exponent Handling */
		vhdl << tab << declare("expX", wEX_) << " <= X"<< range(wEX_ + wFX_ -1, wFX_) << ";" << endl;
		vhdl << tab << declare("expY", wEY_) << " <= Y"<< range(wEY_ + wFY_ -1, wFY_) << ";" << endl;

		//Add exponents and substract bias
		vhdl << tab << declare(getTarget()->adderDelay(wEX_+2), "expSumPreSub", wEX_+2) << " <= (\"00\" & expX) + (\"00\" & expY);" << endl;
		vhdl << tab << declare("bias", wEX_+2) << " <= CONV_STD_LOGIC_VECTOR(" << intpow2(wER-1)-1 << ","<<wEX_+2<<");"<< endl;

		vhdl << tab << declare(getTarget()->adderDelay(wEX_+2), "expSum",wEX+2) << " <= expSumPreSub - bias;" << endl;

		/* Significand Handling */
		vhdl << tab << declare("sigX",1 + wFX_) << " <= \"1\" & X" << range(wFX_-1,0) << ";" << endl;
		vhdl << tab << declare("sigY",1 + wFY_) << " <= \"1\" & Y" << range(wFY_-1,0) << ";" << endl;


		int sigProdSize;
		int g=3; // number of guard bits needed in case of faithful rounding
		if(correctlyRounded_)
			sigProdSize = wFX_+1 + wFY_+1;
		else
			// faithful rounding will be computed by IntMultiplier
			// but we still  have to re-round behind
			sigProdSize = wFR_+g;

		newInstance("IntMultiplier", "SignificandMultiplication", "wX="+to_string(wFX_+1)+" wY="+to_string(wFY_+1)+" wOut="+to_string(sigProdSize)+ " dspThreshold="+to_string(dspOccupationThreshold),"X=>sigX,Y=>sigY", "R=>sigProd");

		/* Exception Handling, assumed to be faster than both exponent and significand computations */
		vhdl << tab << declare("excSel",4) <<" <= X"<<range(wEX_ + wFX_ +2, wEX_ + wFX_ + 1) << " & Y"<<range(wEY_ + wFY_ +2, wEY_ + wFY_ +1) << ";" << endl;

		vhdl << tab << "with excSel select " << endl;
		vhdl << tab << declare(getTarget()->lutDelay(), "exc",2) << " <= \"00\" when  \"0000\" | \"0001\" | \"0100\", " << endl;
		vhdl << tab << "       \"01\" when \"0101\","<<endl;
		vhdl << tab << "       \"10\" when \"0110\" | \"1001\" | \"1010\" ,"<<endl;
		vhdl << tab << "       \"11\" when others;"<<endl;

		if (normalized_){
		/******************************************************************/

			vhdl << tab<< declare("norm") << " <= sigProd" << of(sigProdSize -1) << ";"<<endl;

			vhdl << tab<< "-- exponent update"<<endl;
			vhdl << tab<< declare("expPostNorm", wEX_+2) << " <= expSum + (" << zg(wEX_+1,0) << " & norm);"<<endl;

			//  exponent update is in parallel to the mantissa shift, so get back there

			//check is rounding is needed
			if (1+wFR_ >= wFX_+wFY_+2) {
				/* => no rounding needed - possible padding;
				   in this case correctlyRounded_ is irrelevant: result is exact  */
				vhdl << tab << declare(getTarget()->lutDelay(), "resSig", wFR_) << " <= sigProd" << range(wFX_+wFY_,0) << " & " <<   zg(1+wFR_ - (wFX_+wFY_+2) , 0)<<" when norm='1' else"<<endl;
				vhdl << tab <<"                      sigProd" << range(wFX_+wFY_-1,0) << " & " << zg(1+wFR_ - (wFX_+wFY_+2) + 1 , 0) << ";"<<endl;

				vhdl << tab <<"with expPostNorm" << range(wER_+1, wER_) << " select"<<endl;
				vhdl << tab << declare("excPostNorm",2) << " <=  \"01\"  when  \"00\","<<endl;
				vhdl << tab <<"                            \"10\"             when \"01\", "<<endl;
				vhdl << tab <<"                            \"00\"             when \"11\"|\"10\","<<endl;
				vhdl << tab <<"                            \"11\"             when others;"<<endl;

				vhdl << tab << "with exc select " << endl;
				vhdl << tab << declare("finalExc",2) << " <= exc when  \"11\"|\"10\"|\"00\"," <<endl;
				vhdl << tab << "                    excPostNorm when others; " << endl;

				vhdl << tab << "R <= finalExc & sign & expPostNorm" << range(wER_-1, 0) << " & resSig;"<<endl;
			}
			else{
				vhdl << tab<< "-- significand normalization shift"<<endl;
				vhdl << tab << declare(getTarget()->lutDelay(), "sigProdExt", sigProdSize) << " <= sigProd" << range(sigProdSize-2, 0) << " & " << zg(1,0) <<" when norm='1' else"<<endl;
				vhdl << tab << "                      sigProd" << range(sigProdSize-3, 0) << " & " << zg(2,0) << ";"<<endl;

				vhdl << tab << declare("expSig", 2 + wER_ + wFR_) << " <= expPostNorm & sigProdExt" << range(sigProdSize-1,  sigProdSize-wFR_) << ";" << endl;

				if(correctlyRounded_) {
					vhdl << tab << declare("sticky") << " <= sigProdExt" << of(wFX_+wFY + 1 - wFR) << ";" << endl;

					vhdl << tab << declare(getTarget()->eqConstComparatorDelay(sigProdSize-1 - wFR), "guard") << " <= '0' when sigProdExt" << range(wFX_+wFY + 1 - wFR - 1,0) << "=" << zg(wFX_+wFY + 1 - wFR - 1 +1,0) <<" else '1';" << endl;

					vhdl << tab << declare(getTarget()->lutDelay(), "round") << " <= sticky and ( (guard and not(sigProdExt" << of(wFX_+wFY + 1 - wFR+1) <<")) or ("
					     << "sigProdExt" << of(wFX_+wFY + 1 - wFR+1) << " ))  ;" << endl;
				}
				else{
					vhdl << tab << declare("round") << " <= '1' ;" << endl;
				}

				newInstance("IntAdder", "RoundingAdder", "wIn="+to_string(2 + wER_ + wFR_),"X=>expSig,Cin=>round", "R=>expSigPostRound", "Y=>" + zg(2 + wER_ + wFR_,0));


				vhdl << tab <<"with expSigPostRound" << range(wER_+wFR_+1, wER_+wFR_) << " select"<<endl;
				vhdl << tab << declare("excPostNorm",2) << " <=  \"01\"  when  \"00\","<<endl;
				vhdl << tab <<"                            \"10\"             when \"01\", "<<endl;
				vhdl << tab <<"                            \"00\"             when \"11\"|\"10\","<<endl;
				vhdl << tab <<"                            \"11\"             when others;"<<endl;

				vhdl << tab << "with exc select " << endl;
				vhdl << tab << declare("finalExc",2) << " <= exc when  \"11\"|\"10\"|\"00\"," <<endl;
				vhdl << tab << "                    excPostNorm when others; " << endl;
				vhdl << tab << "R <= finalExc & sign & expSigPostRound" << range(wER_+wFR_-1, 0)<<";"<<endl;

			}
		}else{ //the non-normalized version for FPDotProduct
			// TODO: modern pipeline framework
			// TODO: manage the faithful case
				vhdl << tab <<"with expSum" << range(wER_+1, wER_) << " select"<<endl;
				vhdl << tab << declare("excPostProc",2) << " <=  \"01\"  when  \"00\","<<endl;
				vhdl << tab <<"                            \"10\"             when \"01\", "<<endl;
				vhdl << tab <<"                            \"00\"             when \"11\"|\"10\","<<endl;
				vhdl << tab <<"                            \"11\"             when others;"<<endl;

				vhdl << tab << "with exc select " << endl;
				vhdl << tab << declare("finalExc",2) << " <= exc when  \"11\"|\"10\"|\"00\"," <<endl;
				vhdl << tab << "                    excPostProc when others; " << endl;

				vhdl << tab << "ResultExponent <= expSum" << range(wER_-1, 0) << ";" << endl;
				vhdl << tab << "ResultSignificand <= sigProd;" << endl;
				vhdl << tab << "ResultException <= finalExc;" << endl;
				vhdl << tab << "ResultSign <= sign;" << endl;
		}
	} // end constructor

	FPMult::~FPMult() {
	}



	// TODO the unnormalized case is not emulated
	void FPMult::emulate(TestCase * tc)
	{
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");
		mpz_class svY = tc->getInputValue("Y");

		/* Compute correct value */
		FPNumber fpx(wEX_, wFX_), fpy(wEY_, wFY_);
		fpx = svX;
		fpy = svY;
		mpfr_t x, y, r;
		mpfr_init2(x, 1+wFX_);
		mpfr_init2(y, 1+wFY_);
		mpfr_init2(r, 1+wFR_);
		fpx.getMPFR(x);
		fpy.getMPFR(y);
		if(correctlyRounded_){
			mpfr_mul(r, x, y, GMP_RNDN);
			// Set outputs
			FPNumber  fpr(wER_, wFR_, r);
			mpz_class svR = fpr.getSignalValue();
			tc->addExpectedOutput("R", svR);
		}
		else{
			// round down
			mpfr_mul(r, x, y, GMP_RNDD);
			FPNumber  fprd(wER_, wFR_, r);
			mpz_class svRd = fprd.getSignalValue();
			tc->addExpectedOutput("R", svRd);
			// round up
			mpfr_mul(r, x, y, GMP_RNDU);
			FPNumber  fpru(wER_, wFR_, r);
			mpz_class svRu = fpru.getSignalValue();
			tc->addExpectedOutput("R", svRu);
		}
		// clean up
		mpfr_clears(x, y, r, NULL);
	}

	OperatorPtr FPMult::parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args){
		int wE, wF;
		bool correctlyRounded;
		double dspOccupationThreshold=0.0;

		UserInterface::parseStrictlyPositiveInt(args, "wE", &wE);
		UserInterface::parseStrictlyPositiveInt(args, "wF", &wF);
		UserInterface::parseBoolean(args, "correctlyRounded", &correctlyRounded);
		UserInterface::parseFloat(args, "dspThreshold", &dspOccupationThreshold);

		return new FPMult(parentOp, target, wE, wF, wE, wF, wE, wF, true, correctlyRounded, dspOccupationThreshold); //currently, user interface only supports same data formats for all inputs and output
	}

	void FPMult::registerFactory(){
		UserInterface::add("FPMult", // name
                           "A floating-point multiplier. The actual FloPoCo component supports different input and output sizes, but this is not available from the command line.",
                           "BasicFloatingPoint", // categories
                           "",
                           "wE(int): exponent size in bits; \
                           wF(int): input's mantissa size in bits;  \
                           wFout(int)=0: output's mantissa size in bits (if 0 or ommitted, will be equal to wFIn); \
						   correctlyRounded(bool)=true: Use correct rounding, if false use faithful rounding;\
						   dspThreshold(real)=0.0: threshold of relative occupation ratio of a DSP multiplier to be used or not", // This string will be parsed
                           "",
                           FPMult::parseArguments
                           ) ;

	}
}
