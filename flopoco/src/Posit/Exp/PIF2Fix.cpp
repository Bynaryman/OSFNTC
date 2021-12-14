// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "PIF2Fix.hpp"

#include "ShiftersEtc/Shifters.hpp"
#include "TestBenches/PositNumber.hpp"
#include "TestBenches/IEEENumber.hpp"

using namespace std;
namespace flopoco {

  void PIF2Fix::computePIFWidths(int const widthP, int const wES, int* wE, int* wF) {
		*wE = intlog2(widthP) + 1 + wES;
		*wF = widthP - (wES + 3);
	}

  PIF2Fix::PIF2Fix(Target* target, Operator* parentOp, int widthP, int wES):Operator(parentOp, target), widthP_(widthP), wES_(wES) {

		// definition of the source file name, used for info and error reporting using REPORT 
		srcFileName="PIF2Posit";

		// definition of the name of the operator
		ostringstream name;
		name << "PIF2Fix_" << widthP << "_" << wES;
		setNameWithFreqAndUID(name.str());
		// Copyright 
		setCopyrightString("test");

    
        	// SET UP THE IO SIGNALS
		int g = 1; // comme ça quand on le change ça ira mieux
		computePIFWidths(widthP, wES, &wE_, &wF_);
		// declaring inputs
		int widthI = wE_ + wF_ + 5;
		int widthO = wE_ + wF_ + g + 1;
		addInput ("I" , widthI);
		// declaring output
		addOutput("O" , widthO);
		cout << "in : " << widthI << " and out : " << widthO << endl;

		
		addFullComment("Start of vhdl generation"); // this will be a large, centered comment in the VHDL


		// basic message
		REPORT(INFO,"Declaration of PIF2Fix \n");

		// more detailed message
		REPORT(DETAILED, "this operator has received two parameters " << widthP << " and " << wES);
  
		// debug message for developper
		REPORT(DEBUG,"debug of PIF2Fix");


		
		
		vhdl << declare(.0, "is_NAR", 1, false) << "<= I" << of(widthI - 1) << 
			";" << endl;
		
		vhdl << declare(.0, "s", 1, false) << "<= I" << of(widthI - 2) << 
			";" << endl;

		vhdl << declare(.0, "biased_exponent", wE_) << "<= I" << range(widthI - 3, wF_ + 3) << 
			";" << endl;
		
		vhdl << declare(.0, "fraction", wF_ + 1) << "<= I" << range(wF_+2, 2) << 
		  ";" << endl; // on prend pas le guard et sticky mais le bit implicite y est
		
		// on s'occupe de la partie exposant
		int bias = (((widthP - 2)<< wES) + 1);
		int e0 = bias - (wF_ + g);

		
		vhdl << declare(0., "shiftVal", wE_ + 2) << "<= (\"00\" & biased_exponent) - " << e0 << ";" << endl;
		vhdl << declare(0., "resultWillBeOne", 1, false) << " <= shiftVal" << of(wE_ + 1)<< ";" << endl; // c'est le cas où on aurait voulu shifter encore plus, donc nombre trop petit, c'est négatif tout ça

		//donc en gros on shift à gauche, en extension de signe, et voilà on est content

		int maxshift= wE_ + g - 1;
		int maxshiftsize = intlog2(maxshift);
		
		vhdl << declare(0., "oufl", 1, false) << " <= not resultWillBeOne when shiftVal" << range(wE_, 0) << " >= " << maxshift << " else '0';" << endl; //là on vérifie so exposant trop grand

		// on prend que ce qui est important dans shiftval, et on aura juste un truc random si ça overflow mais comme c'est déjà pris en compte on s'en fout
		vhdl << declare(0., "shiftValIn", maxshiftsize) << "<= shiftVal" << range(maxshiftsize-1, 0) << ";" << endl;

	
		ostringstream param, inmap, outmap;
		param << "wIn=" << wF_ + 1;
		param << " maxShift=" << maxshift;
		param << " wOut=" << widthO;
		param << " dir=" << Shifter::Left;
		param << " inputPadBit=true";

		inmap << "X=>fraction,S=>shiftValIn,padBit=>s";

		outmap << "R=>fix";

		newInstance("Shifter", "matissa_shift", param.str(), inmap.str(), outmap.str());

		
		//donc on a 3 valeurs importantes, is_NAR du début, qu'on propagera, resultWillBeOne, et oufl (+ fix si on a pas eu de pb) on les garde dans le coin mais on les sort pas (on va faire du copier-coller dans l'exponentielle plus tard)

		/* débug, tout va bien pas de pb
		  vhdl << declare(0., "problem", 1, false) << " <= resultWillBeOne or oufl;" << endl;
		  vhdl << declare(0., "upper_bound", 1, false) << " <= '1' when shiftVal = "<< (maxshift+1)<<" else '0';" << endl; */
		vhdl << "O <= s & fix;" << endl; // c'est pas super compréhensible qu'il faille ajouter un bit, c'est bizarre

		
		addFullComment("End of vhdl generation"); // this will be a large, centered comment in the VHDL
	};

	
	void PIF2Fix::emulate(TestCase * tc) {
	  mpz_class si = tc->getInputValue("I");
	  mpfr_t val;
	  mpfr_init2(val, wE_ + wF_ + 2);
	  PositNumber posit(wE_+ wF_ +2, wE_, val);
	  tc->addExpectedOutput("O", posit.getSignalValue());
	  mpfr_clear(val);

	  //ça renvoit un truc random mais au moins de la bonne taille, on peut regarder
	}


	OperatorPtr PIF2Fix::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int width, es;
		UserInterface::parseInt(args, "width", &width);
		UserInterface::parseInt(args, "wES", &es);
		return new PIF2Fix(target, parentOp, width, es);
	}
	
	void PIF2Fix::registerFactory() {
		UserInterface::add("PIF2Fix", // name
				   "Converts Posit Intermediate Format to the FixPoint format used in the exponential", // description, string
				   "Conversions", // category, from the list defined in UserInterface.cpp
				   "", //seeAlso
				   "width(int): The size of the posit; \
                        wES(int): The exponent size (for the posit)",
				   // More documentation for the HTML pages. If you want to link to your blog, it is here.
				   "",
				   PIF2Fix::parseArguments
				   ) ;
	}
  
}//namespace
