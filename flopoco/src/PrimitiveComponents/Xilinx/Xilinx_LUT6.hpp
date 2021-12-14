#ifndef Xilinx_LUT6_H
#define Xilinx_LUT6_H

#include "Xilinx_Primitive.hpp"

#include "Xilinx_LUT_compute.h"

namespace flopoco {
    enum LUT6_VARIANT {
        LUT6_GENERAL_OUT,
        LUT6_TWO_OUT,
        LUT6_LOCAL_OUT,
        LUT6_GENERAL_AND_LOCAL_OUT
    };
    // new operator class declaration
    class Xilinx_LUT6_base : public Xilinx_Primitive {
      public:
		Xilinx_LUT6_base(Operator* parentOp, Target *target );
    };

    class Xilinx_LUT6 : public Xilinx_LUT6_base {
      public:
		Xilinx_LUT6(Operator* parentOp, Target *target );
    };

    class Xilinx_LUT6_2 : public Xilinx_LUT6_base {
      public:
		Xilinx_LUT6_2(Operator* parentOp, Target *target );
    };

    class Xilinx_LUT6_L : public Xilinx_LUT6_base {
      public:
		Xilinx_LUT6_L(Operator* parentOp, Target *target );
    };

    class Xilinx_LUT6_D : public Xilinx_LUT6_base {
      public:
		Xilinx_LUT6_D(Operator* parentOp, Target *target );
    };
}//namespace

#endif
