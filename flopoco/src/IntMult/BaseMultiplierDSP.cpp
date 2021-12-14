#include "BaseMultiplierDSP.hpp"
#include "../PrimitiveComponents/Xilinx/Xilinx_LUT6.hpp"
#include "../PrimitiveComponents/Xilinx/Xilinx_CARRY4.hpp"
#include "../PrimitiveComponents/Xilinx/Xilinx_LUT_compute.h"
#include "IntMult/DSPBlock.hpp"

namespace flopoco {

Operator* BaseMultiplierDSP::generateOperator(
		Operator *parentOp, 
		Target* target,
		Parametrization const &parameters) const
{
	//ToDo: no fliplr support anymore, find another solution!
	return new DSPBlock(
			parentOp, 
			target, 
            parameters.getMultXWordSize(),
			parameters.getMultYWordSize(),
			parameters.isSignedMultX(),
			parameters.isSignedMultY()
			); 
}

}   //end namespace flopoco

