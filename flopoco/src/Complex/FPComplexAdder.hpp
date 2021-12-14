
#ifndef COMPLEXADDER_HPP
#define COMPLEXADDER_HPP
#include <vector>
#include <sstream>

#include "../Operator.hpp"
#include "../FPMult.hpp"
#include "../FPAddSinglePath.hpp"

namespace flopoco{

	class ComplexAdder : public Operator
	{
	public:
		ComplexAdder(Target* target, int wE, int wF);
		~ComplexAdder();
		
		void emulate(TestCase * tc);
		
		void buildStandardTestCases(TestCaseList* tcl);
		
		TestCase* buildRandomTestCase(int i);

		int wE, wF;

	};
}
#endif
