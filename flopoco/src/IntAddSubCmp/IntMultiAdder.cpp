/*
 * An adder of n integers, using a bit heap
 *
 * This file is part of the FloPoCo project developed by the Arenaire
 * team at Ecole Normale Superieure de Lyon
 * 
 * Authors : Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr
 *
 * Initial software.
 * Copyright © ENS-Lyon, INRIA, CNRS, UCBL, 
 * 2008-2011.
 * All rights reserved.
 */



#include "../Operator.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "../utils.hpp"
#include "IntMultiAdder.hpp"

using namespace std;


namespace flopoco{




	
	//standalone operator
	IntMultiAdder::IntMultiAdder(OperatorPtr parentOp, Target* target, unsigned int wIn_, unsigned int n_, bool signedIn_, unsigned int wOut_):
		Operator(parentOp, target),
		wIn(wIn_),
		n(n_),
		signedIn(signedIn_)
	{
		srcFileName="IntMultiAdder";
		setCopyrightString ( "Florent de Dinechin (2008-2016)" );
		ostringstream name;
		name << "IntMultiAdder_"  << (signedIn?"S":"U") << wIn << "_" << n;
		setNameWithFreqAndUID(name.str());

		BitHeap* bh;
		if(wOut_==0){ //compute it
			wOut = wIn+intlog2(n);
		}
		else{
			wOut=wOut_;
		}
		if(wIn < 1+(signedIn?1:0)) {
			THROWERROR("wIn too small");
		}
		if(wIn==1 && wOut==1) {
			THROWERROR("There are more efficient ways of computing a parity... and this one turns out not to work at the moment.");
		}

		if(wIn>wOut) {
			THROWERROR("wIn>wOut: I could build such a silly operator but I don't want to.");
		}

		REPORT(INFO, "wOut=" << wOut);
		addOutput("R", wOut);
		bh = new BitHeap(this, wOut); 

		for (unsigned int i=0; i<n; i++) {
			string xi=join("X",i);
			string ixi=join("iX",i);
			addInput  (join("X",i)  , wIn, true);
			vhdl << tab << declareFixPoint(ixi, signedIn, wIn-1, 0) << " <= " << (signedIn?"":"un")<< "signed(" << xi << ");" << endl; // simplest way to get signedness in the signal
			bh->addSignal(ixi);
		}


		//compress the bitheap and produce the result
		bh->startCompression();

		// Retrieve the bits we want from the bit heap
		vhdl << tab << declare("OutRes", wOut) << " <= " << 
			bh->getSumName() << ";" << endl; // This range is useful in case there was an overflow?

		vhdl << tab << "R <= OutRes;" << endl;
	}




	
	void IntMultiAdder::emulate(TestCase* tc)
	{
		// get the inputs from the TestCase
		mpz_class result=0;
		for(unsigned int i=0; i<n; i++) {
			mpz_class xi = tc->getInputValue ( join("X",i) );
			if(signedIn && (xi>>(wIn-1)==1) ) {
				xi -= mpz_class(1)<<wIn; // two's complement
			}
			result += xi;
		}
			// Don't allow overflow: the output is modulo 2^wIn
		result = (result+(mpz_class(1)<<wOut)) & ((mpz_class(1)<<wOut)-1);

		// complete the TestCase with this expected output
		tc->addExpectedOutput ( "R", result );
	}





	
	TestList IntMultiAdder::unitTest(int index)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		if(index==-1) 	{ // The unit tests
			for (int n=2; n<=8; n++) {
				for(int s=0; s<2; s++) {
					for(int wIn=1+(s?1:0); wIn<8; wIn++) { // 1+s because no 2's complement on 1 bit...
						paramList.push_back(make_pair("wIn",to_string(wIn)));
						paramList.push_back(make_pair("n",to_string(n)));
						paramList.push_back(make_pair("signedIn",to_string(s)));
						paramList.push_back(make_pair("TestBench n=",to_string(1000)));
						testStateList.push_back(paramList);

						// same with wOut=wIn
						if (wIn>1) {
							paramList.push_back(make_pair("wOut",to_string(wIn)));
							testStateList.push_back(paramList);
						}
						paramList.clear();

					}
				}
			}
		}
		return testStateList;
	}

	OperatorPtr IntMultiAdder::parseArguments(OperatorPtr parentOp, Target* target, std::vector<std::string> &args)
	{
		int wIn, n, wOut;
		bool signedIn;
		UserInterface::parseInt(args, "wIn", &wIn);
		UserInterface::parseInt(args, "n", &n);
		UserInterface::parseBoolean(args, "signedIn", &signedIn);
		UserInterface::parseInt(args, "wOut", &wOut);
		return new IntMultiAdder(parentOp, target, wIn, n, signedIn, wOut);
	}

	void IntMultiAdder::registerFactory()
	{
		UserInterface::add(
				"IntMultiAdder",
				"A component adding n integers, bitheap based. If wIn=1 it is also a population count",
				"BasicInteger",
				"",
				"signedIn(bool): 0=unsigned, 1=signed; \
			  n(int): number of inputs to add;\
			  wIn(int): input size in bits;\
			  wOut(int)=0: output size in bits -- if 0, wOut is computed to be large enough to represent the result;",
				"",
				IntMultiAdder::parseArguments,
				IntMultiAdder::unitTest
		);
	}

	
}





