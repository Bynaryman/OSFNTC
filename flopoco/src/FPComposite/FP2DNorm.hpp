#include "Operator.hpp"
#include "utils.hpp"

/*  All flopoco operators and utility functions are declared within
  the flopoco namespace.
    You have to use flopoco:: or using namespace flopoco in order to access these
  functions.
*/
namespace flopoco {


	// new operator class declaration
	class FP2DNorm : public Operator {
	public:
		int wE; 
		int wF;


	public:
		// definition of some function for the operator    

		// constructor, defined there with two parameters
		FP2DNorm(Target* target,int wE = 8, int wF=23);

		// destructor
		~FP2DNorm() {};


		// Below all the functions needed to test the operator
		/* the emulate function is used to simulate in software the operator
		   in order to compare this result with those outputed by the vhdl opertator */
		void emulate(TestCase * tc);

		/* function used to create Standard testCase defined by the developper */
		// void buildStandardTestCases(TestCaseList* tcl);


		/* function used to bias the (uniform by default) random test generator
		   See FPExp.cpp for an example */
		// TestCase* buildRandomTestCase(int i);
	};
}
