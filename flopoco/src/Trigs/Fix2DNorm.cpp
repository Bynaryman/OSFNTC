
#include <iostream>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"

#include "Fix2DNorm.hpp"
#include "FixFunctions/GenericTable.hpp"
// #include "FixFunctions/BipartiteTable.hpp"
// #include "FixFunctions/FixFunctionByPiecewisePoly.hpp"


using namespace std;
namespace flopoco{



	Fix2DNorm::Fix2DNorm(Target* target_, int msb_, int lsb_) :
		Operator(target_), msb(msb_), lsb(lsb_)
	{
		srcFileName="Fix2DNorm";
		setCopyrightString ( "Florent de Dinechin (2015-...)" );
		//		useNumericStd_Unsigned();

		ostringstream name;
		name << "Fix2DNorm_" << msb << "_" << lsb;
		setNameWithFreqAndUID( name.str() );

		addFixInput ("X",  false, msb, lsb);
		addFixInput ("Y",  false, msb, lsb);
		addFixOutput ("R",  false, msb, lsb, 2);
	};


	Fix2DNorm::~Fix2DNorm(){
	};



	
	OperatorPtr Fix2DNorm::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {		
		int msb,lsb, method;
		UserInterface::parseInt(args, "msb", &msb);
		UserInterface::parseInt(args, "lsb", &lsb);
		UserInterface::parseInt(args, "method", &method);
		//select the method
		return new Fix2DNorm(target, msb, lsb);
			
	}

	void Fix2DNorm::registerFactory(){
		UserInterface::add("Fix2DNorm", // name
											 "Computes sqrt(x*x+y*y)",
											 "CompositeFixPoint",
											 "", // seeAlso
											 "msb(int): weight of the MSB of both inputs and outputs; lsb(int): weight of the LSB of both inputs and outputs; \
                        method(int)=-1: technique to use, -1 selects a sensible default",
											 "",
											 Fix2DNorm::parseArguments
											 ) ;
		
	}



}

