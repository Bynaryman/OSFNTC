#ifndef Xilinx_GenericAddSub_slice_H
#define Xilinx_GenericAddSub_slice_H

#include "Operator.hpp"
#include "utils.hpp"
#include "Xilinx_LUT6.hpp"

namespace flopoco {

	// new operator class declaration
    class Xilinx_GenericAddSub_slice : public Operator {

      public:
		// constructor, defined there with two parameters (default value 0 for each)
        Xilinx_GenericAddSub_slice(Operator* parentOp, Target *target, int wIn, bool initial, bool fixed = false, bool dss = false,const std::string& prefix="" );

		// destructor
        ~Xilinx_GenericAddSub_slice() {}

		void build_normal(Operator* parentOp, Target *target, int wIn, bool initial );

		void build_fixed_sign(Operator* parentOp, Target *target, int wIn, bool initial );

		void build_with_dss(Operator* parentOp, Target *target, int wIn, bool initial );

        void getLUT_std() {
        }
        void getLUT_init() {
        }

        string getLUT_dss_init();
        string getLUT_dss_sec();
        string getLUT_dss_std();
	};


}//namespace

#endif
