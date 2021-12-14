#ifndef Xilinx_PRIMITIVE_H
#define Xilinx_PRIMITIVE_H

#include "../Primitive.hpp"
#include <map>

namespace flopoco {
    class Xilinx_Primitive : public Primitive {
        std::map<std::string, std::string> generics_;
      public:

        // constructor, defined there with two parameters (default value 0 for each)
        Xilinx_Primitive(Operator *parentOp, Target *target );

        // destructor
        ~Xilinx_Primitive();

        static void checkTargetCompatibility( Target *target );
    };
}//namespace

#endif
