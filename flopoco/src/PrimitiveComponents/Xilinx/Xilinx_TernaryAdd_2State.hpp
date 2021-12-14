#ifndef Xilinx_TernaryAdd_2State_H
#define Xilinx_TernaryAdd_2State_H

#include "Operator.hpp"
#include "utils.hpp"

#include "Xilinx_LUT_compute.h"


namespace flopoco {
    class Xilinx_TernaryAdd_2State : public Operator {
        int wIn_;
        short bitmask_,bitmask2_;
      public:
        short mapping[3];
        short state_type;

        Xilinx_TernaryAdd_2State(Operator *parentOp, Target *target,const int &wIn,const short &bitmask,const short &bitmask2 = -1 );
	    ~Xilinx_TernaryAdd_2State();

        void insertCarryInit();
        void computeState();
        string computeLUT();

	    static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args);

	    static void registerFactory();
	    
        // Operator interface
	    
	    virtual void emulate(TestCase *tc);
		static TestList unitTest(int index);
    };
}//namespace

#endif
