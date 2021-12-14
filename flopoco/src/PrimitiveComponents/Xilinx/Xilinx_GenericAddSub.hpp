#ifndef Xilinx_GenericAddSub_H
#define Xilinx_GenericAddSub_H

#include "Operator.hpp"
#include "utils.hpp"

namespace flopoco {

	// new operator class declaration
	class Xilinx_GenericAddSub : public Operator {
		const int wIn_;
		const int signs_;
		const bool dss_;
	public:
		// definition of some function for the operator

		// constructor, defined there with two parameters (default value 0 for each)
		Xilinx_GenericAddSub(Operator* parentOp, Target *target, int wIn = 10, bool dss = false );
		Xilinx_GenericAddSub(Operator* parentOp, Target *target, int wIn = 10, int fixed_signs = -1 );

		void build_normal( Target *target, int wIn );

		void build_with_dss( Target *target, int wIn );

		void build_with_fixed_sign( Target *target, int wIn, int fixed_signs );

		// destructor
		~Xilinx_GenericAddSub();
		
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args);

		static void registerFactory();
		// Operator interface
	
		virtual void emulate(TestCase *tc);
		
	};


}//namespace

#endif
