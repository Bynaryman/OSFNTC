/*
 * A floating-point faithful multiplier by a real constant, using a variation of the KCM method

 This file is part of the FloPoCo project developed by the Arenaire
 team at Ecole Normale Superieure de Lyon

 Author : Bogdan Pasca, Bogdan.Pasca@ens-lyon.org

 Initial software.
 Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
 2008-2010.
  All rights reserved.
*/


#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <sollya.h>
#include "../utils.hpp"
#include "../Operator.hpp"

#include "FPRealKCM.hpp"
#include "FixRealKCM.hpp"

using namespace std;

namespace flopoco{

	FPRealKCM::FPRealKCM(Target* target, int wE, int wF,  string constant, map<string, double> inputDelays) :
		Operator(target, inputDelays), wE(wE), wF(wF), constant(constant)
	{
		srcFileName="FPRealKCM";

		/* Convert the input string into a sollya evaluation tree */
		sollya_obj_t node;
		node = sollya_lib_parse_string(constant.c_str());
		/* If  parse error throw an exception */
		if (sollya_lib_obj_is_error(node))
			{
				ostringstream error;
				error << srcFileName << ": Unable to parse string "<< constant << " as a numeric constant" <<endl;
				throw error.str();
			}

		mpfr_init2(mpC, 10000);
		sollya_lib_get_constant(mpC, node);

		if(mpfr_cmp_si(mpC, 0) <= 0)
			throw string("FPRealKCM: only strictly positive constants are supported yet");

		 	

		REPORT(DEBUG, "Constant evaluates to " << mpfr_get_d(mpC, GMP_RNDN));
		REPORT(DEBUG, "Constant exponent is " << mpfr_get_exp(mpC) );

		// build the name
		ostringstream name;
		name <<"FPRealKCM_" << vhdlize(wE)  << "_" << vhdlize(wF) << "_" << vhdlize(constant);
		setNameWithFreqAndUID(name.str());

		int iExp = mpfr_get_exp(mpC);// - 1;

		addFPInput("X", wE, wF);
		addFPOutput("R", wE, wF, 2); //faithful result


		vhdl << tab << declare("exc",2) << "<= X"<<range(wE+wF+2, wE+wF+1)<<";"<<endl;
		vhdl << tab << declare("sign") << "<= X"<<of(wE+wF)<<";"<<endl;

		vhdl << tab << declare("fracX",wF+1) << " <= \"1\" & X"<<range(wF-1,0)<<";"<<endl;
		vhdl << tab << declare("eX",wE) << " <= X"<<range(wE+wF-1, wF)<<";"<<endl;
		
		if(mpfr_cmp_ui_2exp(mpC, 1, iExp - 1) == 0)
		{
			cout << "Power of two " << endl;
			vhdl << tab << declare("nf", wF) << " <= fracX " << range(wF-1, 0) << ";" << endl ;
			vhdl << tab << declare("norm") << " <= '0' ;" << endl ;
		}
		else
		{
			FixRealKCM *frkcm = new FixRealKCM( 
					target, 
					false,
					0,
					-wF,
					-wF+iExp-1, 
					constant
				);

		inPortMap(frkcm, "X", "fracX");
		outPortMap(frkcm, "R", "fracMultRes");
		vhdl << tab << instance( frkcm, "ConstMultKCM") << endl;
		syncCycleFromSignal("fracMultRes");
		setCriticalPath(frkcm->getOutputDelay("R"));

		//get number of bits of output
		//normalize
		vhdl << tab << declare("norm") << " <= fracMultRes"<<of(wF+1)<<";"<<endl;

		manageCriticalPath(getTarget()->localWireDelay() + getTarget()->adderDelay(wE+2));
		vhdl << tab << declare("nf",wF) << " <= fracMultRes"<<range(wF-1,0)<<" when norm='0' else fracMultRes"<<range(wF,1)<<";"<<endl;
		}

		//update exponent
		vhdl << tab << declare("expOp1",wE+2) << " <= CONV_STD_LOGIC_VECTOR("<<iExp-1<<","<<wE+2<<");"<<endl;
		vhdl << tab << declare("finalExp",wE+2) << " <= (\"00\" & eX) + expOp1 + norm;"<<endl;

		manageCriticalPath(getTarget()->localWireDelay() + getTarget()->lutDelay());
		vhdl << tab << "with finalExp"<<range(wE+1,wE)<<" select "<<endl;
		vhdl << tab << declare("excUpdated", 2) << " <= exc when \"00\","<<endl;
		vhdl << tab << tab << "\"00\" when \"10\"|\"11\","<<endl;
		vhdl << tab << tab << "\"10\" when \"01\","<<endl;
		vhdl << tab << tab << " exc when others;"<<endl;

		manageCriticalPath(getTarget()->localWireDelay() + getTarget()->lutDelay());
		vhdl << tab << "with exc select "<<endl;
		vhdl << tab << declare("excUpdated2", 2) << " <= exc when \"00\"|\"10\"|\"11\","<<endl;
		vhdl << tab << tab << " excUpdated when \"01\","<<endl;
		vhdl << tab << tab << " exc when others;"<<endl;

		vhdl << tab << "R <= excUpdated2 & sign & finalExp"<<range(wE-1,0)<<" & nf;"<<endl;
		getOutDelayMap()["R"] = getCriticalPath();
	}

	FPRealKCM::~FPRealKCM() {
		// TODO
	}


	// To have MPFR work in fix point, we perform the multiplication in very large precision using RN,
	// and the RU and RD are done only when converting to an int at the end.
	void FPRealKCM::emulate(TestCase* tc){
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");

		/* Compute correct value */
		FPNumber fpx(wE, wF);
		fpx = svX;
		mpfr_t x, c, ru, rd;
		mpfr_init2(x, 1+wF);
		mpfr_init2(c, 1+wF);
		mpfr_init2(ru, 1+wF);
		mpfr_init2(rd, 1+wF);
		mpfr_set(c, mpC, GMP_RNDN);
		fpx.getMPFR(x);
		mpfr_mul(ru, x, c, GMP_RNDU);
		mpfr_mul(rd, x, c, GMP_RNDD);

		// Set outputs
		FPNumber  fprd(wE, wF, rd);
		mpz_class svRd = fprd.getSignalValue();
		tc->addExpectedOutput("R", svRd);

		FPNumber  fpru(wE, wF, ru);
		mpz_class svRu = fpru.getSignalValue();
		tc->addExpectedOutput("R", svRu);

		// clean up
		mpfr_clears(x, c, rd, ru, NULL);
	}

	// void FPRealKCM::buildStandardTestCases(TestCaseList* tcl){

	// }

	//Interface related methods
	OperatorPtr FPRealKCM::parser(Target *target, vector<string> &args)
	{
		int wE, wF;
		string constant;
		UserInterface::parseStrictlyPositiveInt(args, "wE", &wE);
		UserInterface::parseStrictlyPositiveInt(args, "wF", &wF);
		UserInterface::parseString(args, "constant", &constant);
		return new FPRealKCM(target, wE, wF, constant);
	}

	void FPRealKCM::registerFactory(void)
	{
		UserInterface::add(
					"FPRealKCM",
					"Table based real multiplier for floating points input. ",
					"ConstMultDiv",
					"",
					"wE(int): exponent width;"
					"wF(int): significand width;"
					"constant(string): constant given in arbitrary-precision decimal, or as a Sollya expression, e.g \"log(2)\"",
					"KCM is a table-based method well suited to LUT-based FPGAs. It is due to Ken Chapman who published it in 1994.",
					FPRealKCM::parser
				);
	}


}




