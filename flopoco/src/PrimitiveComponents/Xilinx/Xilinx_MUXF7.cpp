// general c++ library for manipulating streams
#include <iostream>
#include <sstream>
#include <stdexcept>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "Xilinx_MUXF7.hpp"

using namespace std;
namespace flopoco {
	Xilinx_MUXF7_base::Xilinx_MUXF7_base(Operator *parentOp, Target *target ) : Xilinx_Primitive( parentOp,target ) {}

	Xilinx_MUXF7::Xilinx_MUXF7(Operator *parentOp, Target *target ) : Xilinx_MUXF7_base( parentOp,target ) {
        setName( "MUXF7" );
        addOutput( "o" );
        base_init();
    }

	Xilinx_MUXF7_L::Xilinx_MUXF7_L(Operator *parentOp, Target *target ) : Xilinx_MUXF7_base( parentOp,target ) {
        setName( "MUXF7_L" );
        addOutput( "lo" );
        base_init();
    }

	Xilinx_MUXF7_D::Xilinx_MUXF7_D(Operator *parentOp, Target *target ) : Xilinx_MUXF7_base( parentOp,target ) {
        setName( "MUXF7_D" );
        addOutput( "o" );
        addOutput( "lo" );
        base_init();
    }

    void Xilinx_MUXF7_base::base_init() {
        srcFileName = "Xilinx_MUXF7";
        addInput( "i0" );
        addInput( "i1" );
        addInput( "s" );
    }
}//namespace
