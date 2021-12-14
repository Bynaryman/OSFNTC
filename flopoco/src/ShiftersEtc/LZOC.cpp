/*
  A leading zero/one counter for FloPoCo

  Author : Florent de Dinechin, Bogdan Pasca

   This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2011.
  All rights reserved.

*/

#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"
#include "LZOC.hpp"

using namespace std;


namespace flopoco{

	LZOC::LZOC(OperatorPtr parentOp, Target* target, int wIn_, int countType_) :
		Operator( parentOp, target), wIn(wIn_), countType(countType_) {

		srcFileName = "LZOC";
		setCopyrightString("Florent de Dinechin, Bogdan Pasca (2007)");

		ostringstream name;
		if(countType==0)
			name <<"LZC_"<<wIn;
		else if(countType==1)
			name <<"LOC_"<<wIn;
		else if(countType==-1)
			name <<"LZOC_"<<wIn;
		else
			THROWERROR("countType should be 0, 1 or -1; got " << countType);

		setNameWithFreqAndUID(name.str());

		// -------- Parameter set up -----------------
		wOut = intlog2(wIn);
		p2wOut = 1<<wOut; // no need for GMP here

		
		addInput ("I", wIn);
		if(countType==-1)
			addInput ("OZB");
		addOutput("O", wOut);
		
		if(countType==-1)
			vhdl << tab << declare("sozb") <<" <= OZB;" << endl;

		addComment("pad input to the next power of two minus 1", tab);
		vhdl << tab << declare(join("level", wOut), intpow2(wOut)-1) << " <= I";
		if (intpow2(wOut)-1>wIn){
			vhdl << " & ";
			if(countType==0)
				vhdl << og(intpow2(wOut)-1-wIn);
			else if(countType==1)
				vhdl << zg(intpow2(wOut)-1-wIn);
			else if(countType==-1)
				vhdl << rangeAssign(intpow2(wOut)-wIn-2, 0, "not sozb");
		}
		vhdl <<	";"<<endl;
		//each operation is formed of a comparison followed by a multiplexing
		
		addComment("Main iteration for large inputs", tab);
		string currentDigitName, previousLevelName, nextLevelName;
		int i=wOut-1;
		/* Invariants: at iteration i,
			 we define bit i of the output,
			 we test the 2^i leading bits of the leveli vector,
			 we build the next level's vector of size 2^i-1
		 */
		//while (i>=0){
		while (intpow2(i+1) > target->lutInputs()+1){			
			currentDigitName = join("digit", i);
			previousLevelName = join("level", i+1);
			nextLevelName = join("level", i);
			double levelDelay;
			if(countType==-1)
				levelDelay = target->eqComparatorDelay(intpow2(i));
			else
				levelDelay = target->eqConstComparatorDelay(intpow2(i));
				
			//			double levelDelay = intlog(mpz_class(getTarget()->lutInputs()), intpow2(i-1)) * getTarget()->lutDelay();
			vhdl << tab <<declare(levelDelay, currentDigitName) << "<= '1' when " << previousLevelName << range(intpow2(i+1)-2, intpow2(i)-1) << " = ";
			if(countType==0)
				vhdl << zg(intpow2(i));
			else if(countType==1)
				vhdl << og(intpow2(i));
			else if(countType==-1)
				vhdl << rangeAssign(intpow2(i)-1,0, "sozb") ;
			vhdl << " else '0';"<<endl;

			if (i>0){
				vhdl << tab << declare(target->logicDelay(2),  nextLevelName, intpow2(i)-1) << "<= "
						 << previousLevelName << range(intpow2(i)-2,0)  << " when " << currentDigitName << "='1' "
						 <<"else " << previousLevelName << range(intpow2(i+1)-2, intpow2(i)) << ";"<<endl;
			}
			i--;
		}
 
		i++;

		addComment("Finish counting with one LUT", tab);

		// In principle the two following equations are packed within the same LUT by logic tools, so we only add the delay to the first
		// to avoid inserting a register level between them
		double finalDelay=target->logicDelay(5);
		if(countType==-1) {
			vhdl << tab << declare(finalDelay, "z", intpow2(i)-1) << " <= " << nextLevelName << " when ozb='0' else (not "<< nextLevelName << ");" << endl;
			nextLevelName="z";
			finalDelay=0.0;
		}
		if(countType==1) {
			vhdl << tab << declare(finalDelay, "z", intpow2(i)-1) << " <= not "<< nextLevelName << ";" << endl;
			nextLevelName="z";
			finalDelay=0.0;
		}

		vhdl << tab << "with " << nextLevelName << " select " << declare(finalDelay, "lowBits", i) << " <= " << endl;
		for (int j=0; j<intpow2(i); j++) {
			vhdl << tab << tab << "\"" << unsignedBinary(intpow2(i)-1-intlog2(j), i) <<"\" when \"" << unsignedBinary(j, intpow2(i)-1) << "\"," << endl;
		}
		vhdl << tab << tab << zg(i) << " when others;" << endl;
		//		vhdl << tab << tab << rangeAssign(i,0,"'-'") << " when others;" << endl; // seems to slow everything down and consume space 

		vhdl << tab << declare("outHighBits", wOut-i) << " <= ";
		for (int j=wOut-1;j>=i;j--){
			vhdl << join( "digit", j);
			if (j>i)
				vhdl << " & ";
		}
		vhdl << ";" <<endl;

		vhdl << tab << "O <= outHighBits & lowBits ;" << endl;
	}

	LZOC::~LZOC() {}




	void LZOC::emulate(TestCase* tc)
	{
		mpz_class si   = tc->getInputValue("I");
		mpz_class sozb = tc->getInputValue("OZB");
		mpz_class so;

		int j;
		int bit = (sozb == 0) ? 0 : 1;
		for (j = 0; j < wIn; j++)
			{
				if (mpz_tstbit(si.get_mpz_t(), wIn - j - 1) != bit)
					break;
			}

		so = j;
		tc->addExpectedOutput("O", so);
	}


	OperatorPtr LZOC::parseArguments(OperatorPtr parentOp, Target *target, std::vector<std::string> &args) {
		int wIn,countType;
		UserInterface::parseStrictlyPositiveInt(args, "wIn", &wIn);
		UserInterface::parseInt(args, "countType", &countType);
		return new LZOC(parentOp, target, wIn, countType);
	}



	
	void LZOC::registerFactory(){
		UserInterface::add("LZOC", // name
											 "A leading zero or one counter. The output size is computed.",
											 "ShiftersLZOCs", // category
											 "",
											 "wIn(int): input size in bits;\
                        countType(int)=-1:  0 means count zeroes, 1 means count ones, -1 means add an input that defines what to count", // This string will be parsed
											 "", // no particular extra doc needed
											 LZOC::parseArguments
											 ) ;
		
	}
}
