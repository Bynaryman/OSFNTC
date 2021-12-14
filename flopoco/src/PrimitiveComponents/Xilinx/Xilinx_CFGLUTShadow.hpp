#ifndef Xilinx_CFGLUTShadow_H
#define Xilinx_CFGLUTShadow_H

#include "Operator.hpp"

namespace flopoco {
	// new operator class declaration
    class Xilinx_CFGLUTShadow : public Operator {

	public:
		// definition of some function for the operator    

		// constructor, defined there with two parameters (default value 0 for each)
        Xilinx_CFGLUTShadow(Target* target, bool simpleInterface=true);

	};


}//namespace

#endif
