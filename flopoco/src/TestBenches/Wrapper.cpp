/*
  A wrapper generator for FloPoCo.

  A wrapper is a VHDL entity that places registers before and after
  an operator, so that you can synthesize it and get delay and area,
  without the synthesis tools optimizing out your design because it
  is connected to nothing.

  Author: Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.
 */

#include <iostream>
#include <sstream>
#include "Operator.hpp"
#include "Wrapper.hpp"

namespace flopoco{

	Wrapper::Wrapper(Target* target, Operator *op):
		Operator(nullptr, target), op_(op)
	{
		string idext;

		setCopyrightString("Florent de Dinechin (2007-2019)");
		//the name of the Wrapped operator consists of the name of the operator to be
		//	wrapped followd by _Wrapper
		setNameWithFreqAndUID(op_->getName() + "_Wrapper");

		//this operator is a sequential one	even if Target is unpipelined
		setSequential();

		//copy the signals of the wrapped operator
		// this replaces addInputs and addOutputs
		for(int i=0; i<op->getIOListSize(); i++){
			Signal* s = op->getIOListSignal(i);

			if(s->type() == Signal::in){
				//copy the input
				addInput(s->getName(), s->width(), s->isBus());
				//connect the input to an intermediary signal
				idext = "i_" + s->getName();
#if O
				vhdl << tab << declare(getTarget()->ffDelay(), idext, s->width(), s->isBus()) << " <= " << delay(s->getName()) << ";" << endl;
#else
				addRegisteredSignalCopy(idext, s->getName());
#endif
				//connect the port of the wrapped operator
				inPortMap (s->getName(), idext);
			}else if(s->type() == Signal::out){
				//copy the output
				addOutput(s->getName(), s->width(), s->isBus());
				//connect the output
				idext = "o_" + s->getName();
				outPortMap (s->getName(), idext);
			}
			// Adding an attribute so that Vivado doesn't connect the IOs to IOBuff
			// addAttribute("buffer_type",  "string",  s->getName(), "none", true );
		}
		

		// The VHDL for the instance
		vhdl << instance(op, "test", false);

		// copy the outputs
		for(int i=0; i<op->getIOListSize(); i++){
			Signal* s = op->getIOListSignal(i);

			if(s->type() == Signal::out) {
				string idext = "o_" + s->getName();
#if O
				vhdl << tab << s->getName() << " <= " << delay(idext) << ";" << endl;
#else
				addRegisteredSignalCopy(s->getName()+"d", idext);
				vhdl << tab << s->getName() << " <= " << s->getName()+"d" << ";" << endl;
#endif
			}
		}
	}

	Wrapper::~Wrapper() {
	}

	OperatorPtr Wrapper::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		if(UserInterface::globalOpList.empty()){
			throw("ERROR: Wrapper has no operator to wrap (it should come after the operator it wraps)");
		}

		Operator* toWrap = UserInterface::globalOpList.back();
		UserInterface::globalOpList.pop_back();

		return new Wrapper(target, toWrap);
	}

	void Wrapper::registerFactory(){
			UserInterface::add("Wrapper", // name
								 "Wraps the preceding operator between registers (for frequency testing).",
								 "TestBenches",
								 "fixed-point function evaluator; fixed-point", // categories
								 "",
								 "",
								 Wrapper::parseArguments
								 ) ;
	}

}
