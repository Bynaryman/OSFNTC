/*
  A post-normalization unit for the FloPoCo Long Accumulator

  Author : Bogdan Pasca

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2011.
  All rights reserved.

 */


#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <math.h>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <cstdlib>

#include "utils.hpp"
#include "Operator.hpp"
#include "LargeAccToFP.hpp"
#include "ShiftersEtc/Shifters.hpp"

using namespace std;

namespace flopoco{

	LargeAccToFP::LargeAccToFP(Target* target, int MSBA, int LSBA, int wEOut, int wFOut):
		Operator(target),
		LSBA_(LSBA), MSBA_(MSBA), wEOut_(wEOut), wFOut_(wFOut)
	{
		srcFileName = "LargeAccToFP";
		ownTarget_ = target;
		ostringstream name;
		setCopyrightString("Florent de Dinechin, Bogdan Pasca (2008-2014)");
		name <<"LargeAccToFP_"
			  <<(MSBA_>=0?"":"M")<<abs(MSBA_)<<"_"
			  <<(LSBA_>=0?"":"M")<<abs(LSBA_)<<"_"
			  <<wEOut_<<"_"<<wFOut_;
		setNameWithFreqAndUID(name.str());

		sizeAcc_ = MSBA - LSBA + 1;

		//inputs and outputs
		addInput    ("A", sizeAcc_);
		addInput    ("C", sizeAcc_);
		addInput("AccOverflow");
		addFPOutput ("R", wEOut_, wFOut_);

		vhdl << tab <<declare("signA") << " <= A" << of(sizeAcc_-1)<<";"<<endl;
		vhdl << tab <<declare("signC") << " <= C" << of(sizeAcc_-1)<<";"<<endl;

		vhdl << tab <<declare("AccOverflowFlag") << " <= AccOverflow;"<<endl;

		vhdl << tab << declare("extA", sizeAcc_+1) << " <= signA & A;"<<endl;
		vhdl << tab << declare("extC", sizeAcc_+1) << " <= signC & C;"<<endl;

		IntAdder *a = new IntAdder(this, target, sizeAcc_+1);

		inPortMap( a,   "X",   "extA");
		inPortMap( a,   "Y",   "extC");
		inPortMapCst(a, "Cin", "'0'");
		outPortMap( a,  "R",   "acc");
		vhdl << instance(a,    "CarryPropagation");

		syncCycleFromSignal("acc");
		setCriticalPath( a->getOutputDelay("R"));
		setSignalDelay( "acc", getCriticalPath());

		vhdl << tab << declare("resSign") << " <= acc" << of(sizeAcc_) << ";" << endl;

		/* count the number of zeros/ones in order to determine
		the value of the exponent */

		lzocShifterSticky_ = new LZOCShifterSticky(this, target, sizeAcc_+1,  wFOut_ + 1, intlog2(sizeAcc_+1), false, -1) ;
		countWidth_ = lzocShifterSticky_->getCountWidth();

		inPortMapCst ( lzocShifterSticky_, "I"    , "acc");
		inPortMap    ( lzocShifterSticky_, "OZb"  , "resSign");
		outPortMap   ( lzocShifterSticky_, "Count", "nZO");
		outPortMap   ( lzocShifterSticky_, "O"    , "resFrac");
		vhdl << tab << instance(lzocShifterSticky_, "InputLZOCShifter");

		syncCycleFromSignal("resFrac");
		setCriticalPath( lzocShifterSticky_->getOutputDelay("O") );
		setSignalDelay("resFrac",getCriticalPath());

		manageCriticalPath(getTarget()->localWireDelay() + getTarget()->adderDelay(countWidth_+1));
		vhdl << tab <<declare("unbiasedExp", countWidth_+1) << " <= CONV_STD_LOGIC_VECTOR("<<MSBA_+1<<","<<countWidth_+1<<") - (\"0\" & nZO);"<<endl;
		expBias_ =  intpow2(wEOut-1) - 1;
		if (countWidth_+1 < wEOut_){
			/* accumulator does not cover all exponent range */
			manageCriticalPath(getTarget()->localWireDelay() + getTarget()->adderDelay(wEOut_));
			vhdl << tab << declare("bias"     , wEOut_) << " <= CONV_STD_LOGIC_VECTOR("<<expBias_<<","<<wEOut_<<");"<<endl; //fixed value
			vhdl << tab << declare("expBiased", wEOut_) << " <= bias + ("<< rangeAssign(wEOut_-1,countWidth_+1, "unbiasedExp"+of(countWidth_))<<" & unbiasedExp);"<<endl;
			vhdl << tab << declare("excBits"  , 2)      << " <=\"01\";"<<endl;
		} else if (countWidth_+1 == wEOut_){
			manageCriticalPath(getTarget()->localWireDelay() + getTarget()->adderDelay(wEOut_));
			vhdl << tab << declare("bias"     ,wEOut_) << " <= CONV_STD_LOGIC_VECTOR("<<expBias_<<","<<wEOut_<<");"<<endl; //fixed value
			vhdl << tab << declare("expBiased",wEOut_) << " <= bias + unbiasedExp;"<<endl;
			vhdl << tab << declare("excBits"  ,2)      << " <=\"01\";"<<endl;
		}else{
			/* acc covers more range than destination output format */
			manageCriticalPath(getTarget()->localWireDelay() + getTarget()->adderDelay(countWidth_+1));
			vhdl << tab << declare("bias"   , countWidth_+1) << " <= CONV_STD_LOGIC_VECTOR("<<expBias_<<","<<countWidth_+1<<");"<<endl;
			vhdl << tab << declare("expExt" , countWidth_+1) << " <= bias + unbiasedExp;"<<endl;
			vhdl << tab << declare("signExpExt") << " <= expExt" << of(countWidth_)<<";"<<endl;

			if (countWidth_==wEOut_)
				vhdl << tab <<declare("expOverflow") <<" <= '0';"<<endl;
			else
				vhdl << tab <<declare("expOverflow") <<" <= '1' when ( expExt"<<range(countWidth_-1,wEOut_)<<">"<<zg(countWidth_-wEOut_)<<" and signExpExt='0') else '0';"<<endl;
			vhdl << tab <<declare("expUnderflow")<<" <= signExpExt;"<<endl;

			declare("excBits",2);
			vhdl << tab <<"excBits(1) <= expOverflow or expUnderflow or AccOverflow;"<<endl;
			vhdl << tab <<"excBits(0) <= not(expOverflow) and not(expUnderflow) and not AccOverflow;"<<endl;
			vhdl << tab <<declare("expBiased",wEOut_) << " <= expExt" << range(wEOut_-1,0)<<";"<<endl;
		}

		vhdl << tab <<declare("excRes",2) << " <= excBits;"<<endl;
		setSignalDelay("excRes", getCriticalPath());

		//get back to the fraction part
		setCycleFromSignal("resFrac");
		setCriticalPath( getSignalDelay("resFrac"));

		manageCriticalPath( getTarget()->localWireDelay() + getTarget()->lutDelay());
		vhdl << tab << declare("notResFrac",wFOut_+1) << " <= resFrac xor "<<rangeAssign(wFOut_,0,"resSign")<<";"<<endl;

		//convert fraction in sign-magnitude
		IntAdder *smFracAdder = new IntAdder(this, target,wFOut_ + 1 );

		inPortMap   (smFracAdder, "X",   "notResFrac");
		inPortMapCst(smFracAdder, "Y",   zg(wFOut+1,0));
		inPortMap   (smFracAdder, "Cin", "resSign");
		outPortMap  (smFracAdder, "R",   "resultFraction");

		vhdl << tab << instance(smFracAdder, "SignMagnitudeFracAdder");

		syncCycleFromSignal("resultFraction");
		setCriticalPath( smFracAdder->getOutputDelay("R"));
		syncCycleFromSignal("excRes",  getSignalDelay("excRes"));

		vhdl << tab << "R <= excRes & resSign & expBiased" << range(wEOut_-1,0)<<" & resultFraction" << range(wFOut_-1,0) <<";"<<endl;
	}

	LargeAccToFP::~LargeAccToFP() {
	}

	void LargeAccToFP::emulate(TestCase *tc)
	{
		/* Get I/O values */
		mpz_class svA           = tc->getInputValue("A");
		mpz_class svAccOverflow = tc->getInputValue("AccOverflow");
		mpz_class svC           = tc->getInputValue("C");


		mpz_class newAcc;
		newAcc = svA + svC;

		mpz_class tmpSUB = (mpz_class(1) << (MSBA_ - LSBA_+1))    ;
		mpz_class tmpCMP = (mpz_class(1) << (MSBA_ - LSBA_  )) - 1;

		if (newAcc > tmpCMP)
			newAcc = newAcc - tmpSUB;

		mpfr_t x;
		mpfr_init2(x, 10000); //init to infinite prec
		mpfr_set_z(x, newAcc.get_mpz_t(), GMP_RNDN);

		mpfr_t cst, tmp2;
		mpfr_init2(cst, 10000); //init to infinite prec
		mpfr_init2(tmp2, 10000); //init to infinite prec


		mpfr_set_ui(cst, 2 , GMP_RNDN);
		mpfr_set_si(tmp2, LSBA_ , GMP_RNDN);
		mpfr_pow(cst, cst, tmp2, GMP_RNDN);

		mpfr_mul(x, x, cst, GMP_RNDN);

		mpfr_t myFP;
		mpfr_init2(myFP, wFOut_+1);

		mpfr_set(myFP, x, GMP_RNDD);
		FPNumber  fpr(wEOut_, wFOut_, myFP);
		mpz_class svR = fpr.getSignalValue();
		tc->addExpectedOutput("R", svR);

		mpfr_set(myFP, x, GMP_RNDU);
		FPNumber  fpr2(wEOut_, wFOut_, myFP);
		mpz_class svR2 = fpr2.getSignalValue();
		tc->addExpectedOutput("R", svR2);



		// clean-up
		mpfr_clears(x, myFP, NULL);
	}

	void LargeAccToFP::buildStandardTestCases(TestCaseList* tcl){
	}

	TestCase* LargeAccToFP::buildRandomTestCase(int i){

		TestCase *tc;
		mpz_class A,C;

		C=mpz_class(0);
		int chunkSize, k;

		getTarget()->suggestSubaddSize(chunkSize, MSBA_-LSBA_+1);
		if (chunkSize >= MSBA_-LSBA_+1)
			k=1;
		else
			k= int(ceil( double(MSBA_-LSBA_+1)/double(chunkSize)));


		C= mpz_class(0);
		tc = new TestCase(this);

		A = getLargeRandom(MSBA_-LSBA_+1);

		tc->addInput("A", A);

		if (k==1)
			tc->addInput("C", mpz_class(0));
		else{
			for (int j=0;j<k-1;j++){
				C = C + (  getLargeRandom(1)<< (chunkSize*(j+1)) );
				tc->addInput("C", C);
			}


		}
		tc->addInput("AccOverflow",mpz_class(0));

		/* Get correct outputs */
		emulate(tc);
		return tc;
	}

	OperatorPtr LargeAccToFP::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int MSBA, LSBA, wE_out, wF_out;
		UserInterface::parseStrictlyPositiveInt(args, "wE_out", &wE_out); 
		UserInterface::parseStrictlyPositiveInt(args, "wF_out", &wF_out);
		UserInterface::parseInt(args, "MSBA", &MSBA);
		UserInterface::parseInt(args, "LSBA", &LSBA);
		return new LargeAccToFP(target, MSBA, LSBA, wE_out, wF_out);
	}

	void LargeAccToFP::registerFactory(){
		UserInterface::add("LargeAccToFP", // name
											 "Post-normalisation unit for FPLargeAcc.",
											 "CompositeFloatingPoint",
											 "FPLArgeAcc", // seeAlso
											 "wE_out(int): the width of the output exponent ; \
                        wF_out(int): the width of the output fractional part;  \
                        MSBA(int): the weight of the most significand bit of the accumulator; \
                        LSBA(int): the weight of the least significand bit of the accumulator",
											 "Converts the (fixed-point) output of FPLargeAcc or FPDotProduct (with the same parameters) into a floating-point number.  <br> For details on the technique used and an example of application, see <a href=\"bib/flopoco.html#DinechinPascaCret2008:FPT\">this article</a>",
											 LargeAccToFP::parseArguments
											 ) ;
		
	}
	
}
