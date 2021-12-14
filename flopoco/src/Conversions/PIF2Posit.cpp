// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "PIF2Posit.hpp"

#include "ShiftersEtc/Shifters.hpp"
#include "TestBenches/PositNumber.hpp"
#include "TestBenches/IEEENumber.hpp"

using namespace std;
namespace flopoco {

  void PIF2Posit::computePIFWidths(int const widthO, int const wES, int* wE, int* wF) {
		*wE = intlog2(widthO) + 1 + wES;
		*wF = widthO - (wES + 3);
	}

  PIF2Posit::PIF2Posit(Target* target, Operator* parentOp, int widthO, int wES):Operator(parentOp, target), widthO_(widthO), wES_(wES) {

		// definition of the source file name, used for info and error reporting using REPORT 
		srcFileName="PIF2Posit";

		// definition of the name of the operator
		ostringstream name;
		name << "PIF2Posit_" << widthO << "_" << wES;
		setNameWithFreqAndUID(name.str());
		// Copyright 
		setCopyrightString("test");

    
        	// SET UP THE IO SIGNALS
		
		computePIFWidths(widthO, wES, &wE_, &wF_);
		// declaring inputs
		int widthI = wE_ + wF_ + 5;
		addInput ("I" , widthI);
		// declaring output
		addOutput("O" , widthO);



		
		addFullComment("Start of vhdl generation"); // this will be a large, centered comment in the VHDL


		// basic message
		REPORT(INFO,"Declaration of PIF2Posit \n");

		// more detailed message
		REPORT(DETAILED, "this operator has received two parameters " << widthI << " and " << wES);
  
		// debug message for developper
		REPORT(DEBUG,"debug of PIF2Posit");


		/* vhdl is the stream which receives all the vhdl code, some special functions are
		   available to smooth variable declaration and use ... 
		   -> when first using a variable (Eg: T), declare("T",64) will create a vhdl
		   definition of the variable : signal T and includes it it the header of the architecture definition of the operator

		   Each code transmited to vhdl will be parsed and the variables previously declared in a previous cycle will be delayed automatically by a pipelined register.
		*/
		
		
		
		vhdl << declare(.0, "is_NAR", 1, false) << "<= I" << of(widthI - 1) << 
			";" << endl;
		
		vhdl << declare(.0, "s", 1, false) << "<= I" << of(widthI - 2) << 
			";" << endl;

		vhdl << declare(.0, "biased_exponent", wE_) << "<= I" << range(widthI - 3, wF_ + 3) << 
			";" << endl;
		
		vhdl << declare(.0, "fraction", wF_ + 1) << "<= I" << range(wF_+1, 1) << 
			";" << endl;

		vhdl << declare(target->logicDelay(widthI),"is_zero", 1, false) << "<= '1' when I" << range(widthI-1, 0) << " = \"" << string(widthI,'0') << "\" else '0';" << endl;
		
		// on s'occupe de la partie exposant
		vhdl << declare(target->adderDelay(wE_), "exponent", wE_) << "<= biased_exponent - " << (((widthO - 2)<< wES) + 1) << ";" << endl;

		if (wES >0) {
		  vhdl << declare(0., "partial_exponent", wES) << "<= exponent" << range(wES-1, 0) << ";" << endl;

		  vhdl << "with s select " << declare(target->logicDelay(wES), "partial_exponent_us", wES) << " <= " << endl <<
		    tab << "partial_exponent when '0'," << endl <<
		    tab << "not partial_exponent when '1'," << endl <<
		    tab << "\"" << string(wES, '-') << "\" when others;" << endl;
		}
		
		// on s'occupe de la partie regime
		//de combien il faut décaler
		int wCount = intlog2(widthO);
		vhdl << declare(0., "bin_regime", wCount) << "<= exponent" << range(wE_ -2, wES) << ";" << endl;
		vhdl << declare(.0, "first_regime", 1, false) << "<= exponent" << of(wE_-1) << ";" << endl;

		vhdl << "with first_regime select " << declare(target->logicDelay(wCount), "regime", wCount) << " <= "  << endl <<
		  tab << "bin_regime when '0', " << endl <<
		  tab << "not bin_regime when '1', "<< endl <<
		  tab << "\"" << string(wCount, '-') << "\" when others;" << endl; 

		//par quoi on commence (exp négatif ou positif)
		vhdl << declare(target->logicDelay(2), "pad", 1, false) << "<= not(first_regime xor s);" << endl;

	       	vhdl << "with pad select " << declare(target->logicDelay(1), "start_regime", 2) << " <= "  << endl <<
		  tab << "\"01\" when '0', " << endl <<
		  tab << "\"10\" when '1', "<< endl <<
		  tab << "\"--\" when others;" << endl;

		if (wES>0) {
		  vhdl << declare(0., "input_shifter", widthO) << "<= start_regime & partial_exponent_us & fraction;" << endl;
		}
		else {
		   vhdl << declare(0., "input_shifter", widthO) << "<= start_regime & fraction;" << endl;
		}
		
		ostringstream param, inmap, outmap;
		param << "wIn=" << widthO;
		param << " maxShift=" << widthO;
		param << " wOut=" << widthO;
		param << " dir=" << Shifter::Right;
		param << " computeSticky=true";
		param << " inputPadBit=true";

		inmap << "X=>input_shifter,S=>regime,padBit=>pad";

		outmap << "R=>extended_posit,Sticky=>pre_sticky";

		cout << param.str() <<endl;

		newInstance("Shifter", "rshift", param.str(), inmap.str(), outmap.str());

		
		//rounding up
		
		vhdl << declare(0.,"truncated_posit", widthO - 1) << "<= extended_posit" << range(widthO-1, 1) << ";" << endl;
		vhdl << declare(0., "lsb", 1, false) << "<= extended_posit" << of(1) << ";" << endl;
		vhdl << declare(0., "guard", 1, false) << "<= extended_posit" << of(0) << ";" << endl;
		vhdl << declare(0., "sticky", 1, false) << "<= I" << of(0) << " or pre_sticky;" << endl;

		vhdl << declare(target->logicDelay(3),"round_bit", 1, false) << "<= guard and (sticky or lsb);" << endl;
		
		vhdl << declare(target->adderDelay(widthO-1), "rounded_reg_exp_frac", widthO-1) << "<= truncated_posit + round_bit;" << endl;
		vhdl << declare(0., "rounded_posit", widthO) << "<= s & rounded_reg_exp_frac;" << endl;

		vhdl << declare(target->logicDelay(1), "rounded_posit_zero", widthO) << "<= rounded_posit when is_zero= '0' else \"" << string(widthO, '0') << "\";" << endl;
		vhdl << "O <= rounded_posit_zero when is_NAR = '0' else \"1" << string(widthO-1, '0') << "\";" << endl;

		addFullComment("End of vhdl generation"); // this will be a large, centered comment in the VHDL
	};

	
	void PIF2Posit::emulate(TestCase * tc) {
	}


	OperatorPtr PIF2Posit::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int width, es;
		UserInterface::parseInt(args, "width", &width);
		UserInterface::parseInt(args, "wES", &es);
		return new PIF2Posit(target, parentOp, width, es);
	}
	
	void PIF2Posit::registerFactory() {
		UserInterface::add("PIF2Posit", // name
				   "Converts Posit Intermediate Format to Posits", // description, string
				   "Conversions", // category, from the list defined in UserInterface.cpp
				   "", //seeAlso
				   "width(int): The size of the posit; \
                        wES(int): The exponent size (for the posit)",
				   // More documentation for the HTML pages. If you want to link to your blog, it is here.
				   "",
				   PIF2Posit::parseArguments
				   ) ;
	}
  
}//namespace
