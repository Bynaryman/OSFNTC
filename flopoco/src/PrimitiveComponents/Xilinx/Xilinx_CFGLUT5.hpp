#ifndef Xilinx_CFGLUT5_H
#define Xilinx_CFGLUT5_H

#include "Xilinx_Primitive.hpp"
#include "../Primitive.hpp"

namespace flopoco {
	// new operator class declaration
    class Xilinx_CFGLUT5 : public Primitive {
	public:
		/* operatorInfo is a user defined parameter (not a part of Operator class) for
		   stocking information about the operator. The user is able to defined any number of parameter in this class, as soon as it does not affect Operator parameters undeliberatly*/



	public:
		// definition of some function for the operator    

		// constructor, defined there with two parameters (default value 0 for each)
        Xilinx_CFGLUT5(Operator *parentOp, Target* target);

		// destructor
		~Xilinx_CFGLUT5() {};


		// Below all the functions needed to test the operator
		/* the emulate function is used to simulate in software the operator
		   in order to compare this result with those outputed by the vhdl opertator */
		void emulate(TestCase * tc);

		/* function used to create Standard testCase defined by the developper */
		void buildStandardTestCases(TestCaseList* tcl);


		/* function used to bias the (uniform by default) random test generator
		   See FPExp.cpp for an example */
		// TestCase* buildRandomTestCase(int i);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(Target *target , vector<string> &args);
		
		/** Factory register method */ 
		static void registerFactory();


	};


}//namespace

#endif
