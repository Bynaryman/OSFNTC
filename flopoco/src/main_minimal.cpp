/*
  A minimal example on how use libFloPoCo to generate operators for you.

  In the FloPoCo distribution it is built into executable fpadder_example

  To compile it independently, do something like this (from Flopoco root dir): 
  
	g++ -c -I./src src/main_minimal.cpp 
  g++ -L. -pg -Wall main_minimal.o  -o fpadder_example -lFloPoCo /usr/local/lib/libsollya.so -lmpfr  -lgmpxx -lgmp -lmpfi -lxml2 



  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon
  
  Author : Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,  
  2011.
  All rights reserved.

*/

#include "UserInterface.hpp"
#include "Targets/Virtex6.hpp"
#include "FPAddSub/FPAddSinglePath.hpp"
using namespace flopoco;



int main(int argc, char* argv[] )
{

	Target* target = new Virtex6();

	target -> setFrequency(200e6); // 200MHz
	int wE = 9;
	int wF = 33;

	try{
 
		UserInterface::initialize();
		
		Operator*	op = new FPAddSinglePath(nullptr, target, wE, wF);
		// if we want pipeline we need to add the two following lines
		op->schedule(); 
		op->applySchedule();
		
		ofstream file;
		file.open("FPAdd.vhdl", ios::out);
		
		op->outputVHDLToFile(file);
		
		file.close();
	
	
		cerr << endl<<"Final report:"<<endl;
		op->outputFinalReport(cout, 0);
	}
	catch(string &e){
		cerr << "FloPoCo raised the following exception: " << e << endl;
	}
	return 0;
}



