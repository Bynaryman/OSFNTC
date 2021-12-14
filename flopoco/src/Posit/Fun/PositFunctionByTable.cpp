/*
  Function Table for FloPoCo

  Authors: Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,

  All rights reserved.

  */

#include "../../utils.hpp"
#include "PositFunctionByTable.hpp"


#define LARGE_REC 1000

using namespace std;

namespace flopoco{

	PositFunctionByTable::PositFunctionByTable(OperatorPtr parentOp_, Target* target_, string func_, int width_, int wES_):
		Table(parentOp_, target_)
	{
		srcFileName="PositFunctionByTable";
		ostringstream name;
		name<<"PositFunctionByTable";
		setNameWithFreqAndUID(name.str());
		setCopyrightString("Florent de Dinechin (2010-2018)");

		f = new PositFunction(func_, width_, wES_);
		addHeaderComment("-- Evaluator for " +  f-> getDescription() + "\n");
		wIn = f->wIn;
		wOut = f->wOut;
		if(wIn>32) {
		  THROWERROR("width limited to 32 (a table with 1O^10 entries should be enough for anybody). Do you really want me to write a source file of "
				   << wOut * (mpz_class(1) << wIn) << " bytes?");
		}
		vector<mpz_class> v;
		for(int i=0; i<(1<<wIn); i++) {
			mpz_class rn;
			f->eval(mpz_class(i), rn);
			v.push_back(rn);
			//REPORT(FULL, "f("<< i << ") = " << function(i) );
		};
		Table::init(v, join("f", getNewUId()), wIn, wOut);
	}


	
	PositFunctionByTable::~PositFunctionByTable() {
		delete f;
	}



	void PositFunctionByTable::emulate(TestCase* tc){
		f->emulate(tc);
	}

	OperatorPtr PositFunctionByTable::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args)
	{
		int width, wES;
		string f;
		UserInterface::parseString(args, "f", &f);
		UserInterface::parseInt(args, "width", &width);
		UserInterface::parseInt(args, "wES", &wES);
		return new PositFunctionByTable(parentOp, target, f, width, wES);
	}

	void PositFunctionByTable::registerFactory()
	{
		UserInterface::add("PositFunctionByTable", // name
				   "Evaluator of function f using a table.",
				   "Posit",
				   "",
				   "f(string): function to be evaluated between double-quotes, for instance \"exp(x*x)\";\
                                    width(int): size of the Posit;	\
                                    wES(int): size of the Posit's exponent.",
				   "This operator uses a table to store function values.",
				   PositFunctionByTable::parseArguments
				   ) ;
	}
}
