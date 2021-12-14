/*
Permuters for FloPoCo

Authors: Antonin Dudermel

This file is part of the FloPoCo project
developed by the Arenaire team at Ecole Normale Superieure de Lyon

Initial software.
Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
2008-2010.
  All rights reserved.
*/

#include <vector>
#include <sstream>
#include <mpfr.h>

#include "../utils.hpp"
#include "Permuter.hpp"

using namespace std;
namespace flopoco{
	Permuter::Permuter(OperatorPtr parentOp, Target* target, int wIn,
	                   int nbInputs, fint perm):
		
		Operator (parentOp, target), wIn_ (wIn), perm_(perm),
		nbInputs_(nbInputs){

		// Set up the IO signals
		for(int i=0; i<nbInputs; i++){
			addInput(join("I_",i), wIn, true);
			addOutput(join("O_",i), wIn, true);
		}
		for(int i=0; i<nbInputs; i++){
			vhdl << tab << declare(join("Link_", i)) << " <= I_" << i;
			vhdl << tab << "O_" << perm(i) << "Link_" << i;
		}
	}

	Permuter::~Permuter(){}
	
	void Permuter::emulate(TestCase* tc){
		vector<mpz_class> svIn(nbInputs);
		for(int i=0; i<nbInputs; i++){
			svIn[i] = tc->getInputValue(join("In_",i));
		}
	}
}
