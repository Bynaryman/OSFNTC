#ifndef Xilinx_n2m_Decoder_H
#define Xilinx_n2m_Decoder_H

#include "Operator.hpp"
#include "utils.hpp"

#include <vector>
#include <string>

namespace flopoco {

	// new operator class declaration
    class Xilinx_n2m_Decoder : public Operator {
      public:
		// constructor, defined there with two parameters (default value 0 for each)
        Xilinx_n2m_Decoder(Operator *parentOp, Target *target, string name, map<int, int> groups, int n = 0, int m = 0 );

        void check_groups( map<int, int> &groups, int &n, int &m );
		// destructor
        ~Xilinx_n2m_Decoder() {};

	};


}//namespace

#endif
