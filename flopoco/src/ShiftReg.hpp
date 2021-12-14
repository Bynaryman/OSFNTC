#ifndef SHIFTREG_HPP
#define SHIFTREG_HPP

#include "Operator.hpp"
#include "utils.hpp"

namespace flopoco{ 

	
	class ShiftReg : public Operator {
	  
		public:
			/* Costructor : w is the input and output size; n is the number of taps */
		ShiftReg(OperatorPtr parentOp, Target* target, int w, int n, Signal::ResetType resetType=Signal::noReset); 

			/* Destructor */
			~ShiftReg();


			// Below all the functions needed to test the operator
			/* the emulate function is used to simulate in software the operator
			  in order to compare this result with those outputed by the vhdl opertator */
			void emulate(TestCase * tc);

			/* function used to create Standard testCase defined by the developper */
			void buildStandardTestCases(TestCaseList* tcl);


			/** Factory method that parses arguments and calls the constructor */
			static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

			/** Factory register method */
			static void registerFactory();

	  	private:
			int w; // input and output size
			int n; // number of tap in ShiftReg
			Signal::ResetType resetType; // do we want a reset
	};

}

#endif
