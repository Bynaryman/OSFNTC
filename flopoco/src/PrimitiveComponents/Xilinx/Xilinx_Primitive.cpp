// general c++ library for manipulating streams
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"
#include "Xilinx_Primitive.hpp"
#include "Target.hpp"

using namespace std;
namespace flopoco {


    Xilinx_Primitive::Xilinx_Primitive(Operator *parentOp, Target *target ) : Primitive( parentOp, target ) {
        checkTargetCompatibility(target);
		srcFileName = "Xilinx_Primitive";
    }

    Xilinx_Primitive::~Xilinx_Primitive() {}

    void Xilinx_Primitive::checkTargetCompatibility( Target *target ) {
		if( target->getVendor() != "Xilinx" || !(target->getID() == "Virtex6" || target->getID() == "Zynq7000" || target->getID() == "Kintex7") ) {
			throw std::runtime_error( "This component is only suitable for target Xilinx Virtex6, Zynq7000 and Kintex7." );
		}
    }
}//namespace
