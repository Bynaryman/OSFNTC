// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "Posit2PIF.hpp"

#include "ShiftersEtc/LZOCShifterSticky.hpp"
#include "TestBenches/PositNumber.hpp"
#include "TestBenches/IEEENumber.hpp"

using namespace std;
namespace flopoco {

  void Posit2PIF::computePIFWidths(int const widthI, int const wES, int* wE, int* wF) {
		*wE = intlog2(widthI) + 1 + wES;
		*wF = widthI - (wES + 3);
	}

  Posit2PIF::Posit2PIF(Target* target, Operator* parentOp, int widthI, int wES):Operator(parentOp, target), widthI_(widthI), wES_(wES) {

		// definition of the source file name, used for info and error reporting using REPORT 
		srcFileName="Posit2PIF";

		// definition of the name of the operator
		ostringstream name;
		name << "Posit2PIF_" << widthI << "_" << wES;
		setNameWithFreqAndUID(name.str());
		// Copyright 
		setCopyrightString("test");

    
		if (widthI < 3) {
			throw std::string("Posit2PIF Constructor : width is too small, should be greater than two");
		}
		int freeWidth = widthI - 1;
		if (wES >= freeWidth) {
			//Avoid posits without even one bit of precision
			throw std::string("Posit2PIF Constructor : invalid value of wES");
		}


        	// SET UP THE IO SIGNALS
		

		// declaring inputs
		addInput ("I" , widthI);
		// declaring output
		computePIFWidths(widthI, wES, &wE_, &wF_);
		addOutput("O" , wE_ + wF_ + 5);



		
		addFullComment("Start of vhdl generation"); // this will be a large, centered comment in the VHDL


		// basic message
		REPORT(INFO,"Declaration of Posit2PIF \n");

		// more detailed message
		REPORT(DETAILED, "this operator has received two parameters " << widthI << " and " << wES);
  
		// debug message for developper
		REPORT(DEBUG,"debug of Posit2PIF");


		/* vhdl is the stream which receives all the vhdl code, some special functions are
		   available to smooth variable declaration and use ... 
		   -> when first using a variable (Eg: T), declare("T",64) will create a vhdl
		   definition of the variable : signal T and includes it it the header of the architecture definition of the operator

		   Each code transmited to vhdl will be parsed and the variables previously declared in a previous cycle will be delayed automatically by a pipelined register.
		*/
		
	
		
		vhdl << declare(.0, "s", 1, false) << "<= I" << of(widthI - 1) << 
			";" << endl;
		vhdl << declare(.0, "count_type", 1, false) << "<= I" << of(widthI - 2) << 
			";" << endl;

		vhdl << declare(.0, "remainder", widthI - 2) << "<= I" << range(widthI - 3, 0) << 
			";" << endl;

		vhdl << declare(target->logicDelay(1), "not_s", 1, false) << "<= not s;" << endl;

		vhdl << declare(target->adderDelay(widthI-2), "zero_NAR", 1, false) << " <= " <<
		  "not count_type when remainder=\"" << string(widthI -2, '0') << "\" else '0';" << endl;
		
		vhdl << declare(target->logicDelay(2), "is_NAR", 1, false) << "<= zero_NAR and s;" << endl;

		vhdl << declare(target->logicDelay(2), "is_not_zero", 1, false) << "<= not(zero_NAR and not_s);" << endl;

		vhdl << declare(target->logicDelay(2), "implicit_bit", 1, false) << "<= is_not_zero and not_s;" << endl;

		vhdl << declare(target->logicDelay(2), "neg_count", 1, false) << "<= not (s xor count_type);" << endl;

		ostringstream param, inmap, outmap;
		int wCount = intlog2(widthI) - 1; //comme ça le shifter ne pense pas qu'il peut shifter un nombre absurde de bits

		param << "wIn=" << widthI - 2;
		param << " wOut=" << widthI - 2;
		param << " wCount=" << wCount; 

		inmap << "I=>remainder,OZb=>count_type";

		outmap << "Count=>lzCount,O=>usefulBits";

		newInstance("LZOCShifterSticky", "lzoc", param.str(), inmap.str(), outmap.str());

		vhdl << "with neg_count select " << declare(0., "extended_neg_count", wCount+2) << " <= "  << endl <<
		  tab << "\"" << string(wCount+2, '0') << "\" when '0', " << endl <<
		  tab << "\"" << string(wCount+2, '1') << "\" when '1', "<< endl <<
		  tab << "\"" << string(wCount+2, '-') << "\" when others;" << endl; 

		
		vhdl << declare(target->logicDelay(wCount+2), "comp2_range_count", wCount+2) << "<= extended_neg_count xor (\"00\" & lzCount);" << endl;

		vhdl << declare(0., "fraction", wF_) << "<= usefulBits" << range( wF_ - 1, 0) <<";" << endl;

		if (wES>0) {
		  vhdl << declare(0., "partialExponent", wES) << "<= usefulBits" << range( widthI - 4, wF_) << ";" << endl;

		  vhdl << "with s select " << declare(target->logicDelay(wES), "us_partialExponent", wES) << "<= " << endl <<
		    tab << "partialExponent when '0'," << endl <<
		    tab << "not partialExponent when '1'," << endl <<
		    tab << "\"" << string(wES, '-') << "\" when others;" << endl; 

		  vhdl << declare(0., "exponent", wE_) << "<= comp2_range_count & us_partialExponent;" << endl;
		}
		else {
		  vhdl << declare(0., "exponent", wE_) << "<= comp2_range_count;" << endl;
		}

		vhdl << declare(target->adderDelay(wE_), "biased_exponent", wE_) << "<= exponent + " << (((widthI - 2)<< wES) + 1) << ";" << endl;

		vhdl << "with is_not_zero select  " << declare(target->logicDelay(1), "extended_is_not_zero", wE_) << " <= " << endl <<
		  tab << "\"" << string(wE_, '0') << "\" when '0', " << endl <<
		  tab << "\"" << string(wE_, '1') << "\" when '1', "<< endl <<
		  tab << "\"" << string(wE_, '-') << "\" when others;" << endl; 


		vhdl << declare(target->logicDelay(wE_), "final_biased_exponent", wE_) << "<= extended_is_not_zero and biased_exponent;" << endl;

		vhdl << declare(0., "round", 1, false) << "<= '0';"<<endl;
		vhdl << declare(0., "sticky", 1, false) << "<= '0';"<<endl;
		vhdl << "O <= is_NAR & s & final_biased_exponent & implicit_bit & fraction & round & sticky;" << endl; 	

	     		
		addFullComment("End of vhdl generation"); // this will be a large, centered comment in the VHDL
	};

	
	void Posit2PIF::emulate(TestCase * tc) {
	}


	OperatorPtr Posit2PIF::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int width, es;
		UserInterface::parseInt(args, "width", &width);
		UserInterface::parseInt(args, "wES", &es); 
		return new Posit2PIF(target, parentOp, width, es);
	}
	
	void Posit2PIF::registerFactory() {
		UserInterface::add("Posit2PIF", // name
				   "Converts Posits to Posit Intermediate Format", // description, string
				   "Conversions", // category, from the list defined in UserInterface.cpp
				   "", //seeAlso
				   "width(int): The input size; \
                        wES(int): The exponent size",
				   // More documentation for the HTML pages. If you want to link to your blog, it is here.
				   "",
				   Posit2PIF::parseArguments
				   ) ;
	}
  
}//namespace
