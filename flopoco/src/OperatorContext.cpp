/*
  This class models the context of an operator in FloPoCo.

  Author : Florent de Dinechin

  This file is part of the FloPoCo project

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA-Lyon
  2008-2017.
  All rights reserved.

 */


#include "OperatorContext.hpp"

namespace flopoco{

	OperatorContext::OperatorContext(OperatorPtr parentOp_, Target* target_, double frequency_):
		parentOp(parentOp_), target(target_), frequency(frequency_) 	{}
	

	Target* OperatorContext::getTarget() {
		return target;
	}
	
	OperatorPtr OperatorContext::getParentOp() {
		return parentOp;
	}

	double OperatorContext::getFrequency() {
		return frequency;
	}
}
