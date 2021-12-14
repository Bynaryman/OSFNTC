#ifndef Xilinx_MUXF7_H
#define Xilinx_MUXF7_H

#include "Xilinx_Primitive.hpp"

namespace flopoco {
    class Xilinx_MUXF7_base : public Xilinx_Primitive {
      public:
		Xilinx_MUXF7_base(Operator *parentOp, Target *target );
        ~Xilinx_MUXF7_base() {};

        void base_init();
    };

    class Xilinx_MUXF7 : public Xilinx_MUXF7_base {
      public:
		Xilinx_MUXF7(Operator *parentOp, Target *target );
        ~Xilinx_MUXF7() {};
    };

    class Xilinx_MUXF7_L : public Xilinx_MUXF7_base {
      public:
		Xilinx_MUXF7_L(Operator *parentOp, Target *target );
        ~Xilinx_MUXF7_L() {};
    };

    class Xilinx_MUXF7_D : public Xilinx_MUXF7_base {
      public:
		Xilinx_MUXF7_D(Operator *parentOp, Target *target );
        ~Xilinx_MUXF7_D() {};
    };
}//namespace

#endif
