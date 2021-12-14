#ifndef Xilinx_FDCE_H
#define Xilinx_FDCE_H

#include "Xilinx_Primitive.hpp"

namespace flopoco {

    // new operator class declaration
    class Xilinx_FDCE : public Xilinx_Primitive {
      public:
        // constructor, defined there with two parameters (default value 0 for each)
		Xilinx_FDCE(Operator *parentOp, Target *target );

        // destructor
        ~Xilinx_FDCE() {};
    };
}//namespace

#endif
