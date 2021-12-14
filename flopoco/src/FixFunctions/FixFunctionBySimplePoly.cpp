/*
  Polynomial Function Evaluator for FloPoCo
	This version uses a single polynomial for the full domain, so no table. To use typically after a range reduction.
  Authors: Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2018.
  All rights reserved.

  */

/* 

Error analysis: see FixFunctionByPiecewisePoly.cpp


*/

#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <string.h>

#include <gmp.h>
#include <mpfr.h>

#include <gmpxx.h>
#include "../utils.hpp"


#include "FixFunctionBySimplePoly.hpp"
#include "FixHornerEvaluator.hpp"

using namespace std;

namespace flopoco{

#define DEBUGVHDL 0


	FixFunctionBySimplePoly::FixFunctionBySimplePoly(OperatorPtr parentOp, Target* target, string func, bool signedIn, int lsbIn, int msbOut, int lsbOut, bool finalRounding_):
		Operator(parentOp, target), finalRounding(finalRounding_){

		f = new FixFunction(func, signedIn, lsbIn, msbOut, lsbOut);

		srcFileName="FixFunctionBySimplePoly";

		if(finalRounding==false){
			THROWERROR("FinalRounding=false not implemented yet" );
		}

		ostringstream name;
		name<<"FixFunctionBySimplePoly_";
		setNameWithFreqAndUID(name.str());

		setCopyrightString("Florent de Dinechin (2014)");
		addHeaderComment("-- Evaluator for " +  f-> getDescription() + "\n");
		addInput("X"  , -lsbIn + (signedIn?1:0));
		int outputSize = msbOut-lsbOut+1;
		addOutput("Y" ,outputSize , 2);
		useNumericStd();

		if(f->signedIn)
			vhdl << tab << declareFixPoint("Xs", true, 0, lsbIn) << " <= signed(X);" << endl;
		else
			vhdl << tab << declareFixPoint("Xs", true, 0, lsbIn) << " <= signed('0' & X);  -- sign extension of X" << endl;

		// Polynomial approximation
		double targetApproxError = exp2(lsbOut-2);
		poly = new BasicPolyApprox(f, targetApproxError, -1);
		double approxErrorBound = poly->approxErrorBound;

		int degree = poly->degree;
		if(msbOut < poly->coeff[0]->MSB) {
			REPORT(INFO, "user-provided msbO smaller that the MSB of the constant coefficient, I am worried it won't work");
		}
		vhdl << tab << "-- With the following polynomial, approx error bound is " << approxErrorBound << " ("<< log2(approxErrorBound) << " bits)" << endl;

		// Adding the round bit to the degree-0 coeff
		int oldLSB0 = poly->coeff[0]->LSB;
		poly->coeff[0]->addRoundBit(lsbOut-1);
		// The following is probably dead code. It was a fix for cases
		// where BasicPolyApprox found LSB=lsbOut, hence the round bit is outside of MSB[0]..LSB
		// In this case recompute the poly, it would give better approx error 
		if(oldLSB0 != poly->coeff[0]->LSB) {
			// deliberately at info level, I want to see if it happens
			REPORT(INFO, "   addRoundBit has changed the LSB to " << poly->coeff[0]->LSB << ", recomputing the coefficients");
			for(int i=0; i<=degree; i++) {
				REPORT(DEBUG, poly->coeff[i]->report());
			}
			poly = new BasicPolyApprox(f->fS, degree, poly->coeff[0]->LSB, signedIn);
		}


		for(int i=0; i<=degree; i++) {
			coeffMSB.push_back(poly->coeff[i]->MSB);
			coeffLSB.push_back(poly->coeff[i]->LSB);
			coeffSize.push_back(poly->coeff[i]->MSB - poly->coeff[i]->LSB +1);
		}

		for(int i=degree; i>=0; i--) {
			FixConstant* ai = poly->coeff[i];
			//			REPORT(DEBUG, " a" << i << " = " << ai->getBitVector() << "  " << printMPFR(ai->fpValue)  );
			//vhdl << tab << "-- " << join("A",i) <<  ": " << ai->report() << endl;
			vhdl << tab << declareFixPoint(join("A",i), true, coeffMSB[i], coeffLSB[i])
					 << " <= " << ai->getBitVector(0 /*both quotes*/)
					 << ";  --" << ai->report();
			if(i==0)
				vhdl << "  ... includes the final round bit at weight " << lsbOut-1;
			vhdl << endl;
		}


		// In principle we should compute the rounding error budget and pass it to FixHornerEval
		// REPORT(INFO, "Now building the Horner evaluator for rounding error budget "<< roundingErrorBudget);
		
		// This is the same order as newwInstance() would do, but does not require to write a factory for this Operator
		schedule();
		inPortMap("X", "Xs");
		for(int i=0; i<=degree; i++) {
			inPortMap(join("A",i), join("A",i));
		}
		outPortMap("R", "Ys");
		OperatorPtr h = new  FixHornerEvaluator(this, target, 
																						lsbIn,
																						msbOut,
																						lsbOut,
																						degree, 
																						coeffMSB, 
																						poly->coeff[0]->LSB // it is the smaller LSB
																						);
		vhdl << instance(h, "horner", false);
		
		vhdl << tab << "Y <= " << "std_logic_vector(Ys);" << endl;
	}



	FixFunctionBySimplePoly::~FixFunctionBySimplePoly() {
		delete f;
	}



	void FixFunctionBySimplePoly::emulate(TestCase* tc){
		f->emulate(tc);
	}

	void FixFunctionBySimplePoly::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;
		int lsbIn = f->lsbIn;
		bool signedIn = f->signedIn;
		// Testing the extremal cases
		tc = new TestCase(this);
		tc->addInput("X", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addInput("X", (mpz_class(1)<<(-lsbIn) ) -1);
		tc -> addComment("largest positive value, corresponding to 1");
		emulate(tc);
		tcl->add(tc);

		if(signedIn) {
			tc = new TestCase(this);
			tc->addInput("X", (mpz_class(1)<<(-lsbIn) ));
			tc -> addComment("Smallest two's complement value, corresponding to -1");
			emulate(tc);
			tcl->add(tc);
		}
	}





	TestList FixFunctionBySimplePoly::unitTest(int index)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		
		if(index==-1) 
		{ // The unit tests
			// A few regression tests
			// ./flopoco verbose=2 FixFunctionBySimplePoly plainvhdl=true f="sin(x)" lsbIn=-16 msbOut=4 lsbout=-16 TestBench n=-2
			paramList.push_back(make_pair("f","\"sin(x)\""));
			paramList.push_back(make_pair("plainVHDL","true"));
			paramList.push_back(make_pair("lsbIn","-16"));
			paramList.push_back(make_pair("lsbOut","-16"));
			paramList.push_back(make_pair("msbOut","4"));
			paramList.push_back(make_pair("signedIn","true"));
			testStateList.push_back(paramList);
			paramList.clear();
			
			paramList.push_back(make_pair("f","\"exp(x)\""));
			paramList.push_back(make_pair("plainVHDL","true"));
			paramList.push_back(make_pair("lsbIn","-16"));
			paramList.push_back(make_pair("lsbOut","-16"));
			paramList.push_back(make_pair("msbOut","4"));
			paramList.push_back(make_pair("signedIn","true"));
			testStateList.push_back(paramList);
			paramList.clear();
			

#if 0

			for(int wF=5; wF<53; wF+=1) {
				for(int dualPath = 0; dualPath <2; dualPath++)	{
					for(int sub = 0; sub <2; sub++)	{
						int wE = 6+(wF/10);
						while(wE>wF)
							wE -= 2;
					
						paramList.push_back(make_pair("wF",to_string(wF)));
						paramList.push_back(make_pair("wE",to_string(wE)));
						paramList.push_back(make_pair("sub",to_string(sub)));
						paramList.push_back(make_pair("dualPath",to_string(dualPath)));
						testStateList.push_back(paramList);
						paramList.clear();
					}
					
				}
			}
#endif

		}
		else     
		{
				// finite number of random test computed out of index
		}	

		return testStateList;
	}




	
	OperatorPtr FixFunctionBySimplePoly::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args)
	{
		string f;
		bool signedIn;
		int lsbIn, msbOut, lsbOut;

		UserInterface::parseString(args, "f", &f);
		UserInterface::parseBoolean(args, "signedIn", &signedIn);
		UserInterface::parseInt(args, "lsbIn", &lsbIn);
		UserInterface::parseInt(args, "msbOut", &msbOut);
		UserInterface::parseInt(args, "lsbOut", &lsbOut);

		return new FixFunctionBySimplePoly(parentOp, target, f, signedIn, lsbIn, msbOut, lsbOut);
	}



	
	void FixFunctionBySimplePoly::registerFactory()
	{
		UserInterface::add("FixFunctionBySimplePoly",
						   "Evaluator of function f on [0,1) or [-1,1), using a single polynomial with Horner scheme",
						   "FunctionApproximation",
						   "",
						   "f(string): function to be evaluated between double-quotes, for instance \"exp(x*x)\";\
signedIn(bool): if true the function input range is [-1,1), if false it is [0,1);\
lsbIn(int): weight of input LSB, for instance -8 for an 8-bit input;\
msbOut(int): weight of output MSB;\
lsbOut(int): weight of output LSB;\
",
						   "This operator uses a table for coefficients, and Horner evaluation with truncated multipliers sized just right.<br>For more details, see <a href=\"bib/flopoco.html#DinJolPas2010-poly\">this article</a>.",
											 FixFunctionBySimplePoly::parseArguments,
											 FixFunctionBySimplePoly::unitTest

							);
	}

}
