/*
  PositFunction object for FloPoCo

  Authors: Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Aric team at Ecole Normale Superieure de Lyon
	then by the Socrate team at INSA de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,

  All rights reserved.

*/

#include "PositFunction.hpp"
#include "../../TestBenches/PositNumber.hpp"
#include <sstream>
#define LARGE_PREC 1000
namespace flopoco{

	PositFunction::PositFunction(string sollyaString_, int width_, int wES_):
	  sollyaString(sollyaString_), width(width_), wES(wES_), wOut(width), wIn(width)
	{
	        int useed = 1<< wES;
	        int maxpos = 1<< ((width -1)*useed);
		int minneg = - 1 << ((width-2)*useed);
	        ostringstream range;
		range << "[" << minneg <<";"<< maxpos << "]";
	        rangeS = sollya_lib_parse_string(range.str().c_str());
		ostringstream completeDescription;
		completeDescription << sollyaString_;
		completeDescription << " for width=" << width << ", wES=" << wES;
		description = completeDescription.str();
		// cout << description;

		// Now do the parsing in Sollya
		fS= sollya_lib_parse_string(sollyaString_.c_str());

		/* If  parse error throw an exception */
		if (sollya_lib_obj_is_error(fS))
			throw("PositFunction: Unable to parse input function.");
	}




	PositFunction::PositFunction(sollya_obj_t fS_):
		fS(fS_)
	{
	  	int useed = 1<< wES;
	        int maxpos = 1<< ((width -1)*useed);
		int minneg = - 1 << ((width-2)*useed);
	        ostringstream range;
		range << "[" << minneg <<";"<< maxpos << "]";
	        rangeS = sollya_lib_parse_string(range.str().c_str());
	}




	PositFunction::~PositFunction()
	{
	  sollya_lib_clear_obj(fS);
	  sollya_lib_clear_obj(rangeS);
	}

	string PositFunction::getDescription() const
	{
		return description;
	}

  void PositFunction::eval(mpz_class x,mpz_class &r) const
	{
	        mpfr_t X, R;
	        sollya_lib_set_prec(sollya_lib_constant_from_int(LARGE_PREC));
	        PositNumber positx(width, wES, x);
		mpfr_init2(X, LARGE_PREC);
		mpfr_init2(R, LARGE_PREC);
		positx.getMPFR(X);
	    
	        try {
		  sollya_lib_evaluate_function_at_point(R, fS, X, NULL);
		  PositNumber positres(width, wES, R);
		  r = positres.getSignalValue();
		}
		catch (...) { //si y'a un problème on cherche pas on renvoit nar
		  r = mpz_class(1<<(width-1));
		}
		mpfr_clears(X, R, NULL);
	}



	void PositFunction::emulate(TestCase * tc){
			mpz_class x = tc->getInputValue("X");
			mpz_class r;
			eval(x,r);
			// cerr << "x=" << x << " -> " << rNorD << " " << ru << endl; // for debugging
			tc->addExpectedOutput("Y", r);
	}
} //namespace
