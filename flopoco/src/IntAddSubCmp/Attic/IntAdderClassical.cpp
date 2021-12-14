/*
An integer adder for FloPoCo

It may be pipelined to arbitrary frequency.
Also useful to derive the carry-propagate delays for the subclasses of Target

Authors:  Bogdan Pasca, Florent de Dinechin

This file is part of the FloPoCo project
developed by the Arenaire team at Ecole Normale Superieure de Lyon

Initial software.
Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
2008-2010.
  All rights reserved.
*/

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "../Operator.hpp"
#include "IntAdderClassical.hpp"

using namespace std;

namespace flopoco {

	IntAdderClassical::IntAdderClassical ( Target* target, int wIn_, int optimizeType, bool srl) :
		IntAdder(),	wIn(wIn_)
	{
		srcFileName="IntAdderClassical";
		ostringstream name;

		setCopyrightString ( "Bogdan Pasca, Florent de Dinechin (2008-2010)" );

		if(getTarget()->isPipelined())
			name << "IntAdderClassical_" << wIn<<"_f"<<getTarget()->frequencyMHz()<<"_uid"<<getNewUId();
		else
			name << "IntAdderClassical_" << wIn<<"_comb"<<"_uid"<<getNewUId();
		setNameWithFreqAndUID( name.str() );

		inputsGotRegistered = false;
		objectivePeriod	    = 1.0 / getTarget()->frequency();

		

	}


	/**************************************************************************/
	IntAdderClassical::~IntAdderClassical() {
	}


}


