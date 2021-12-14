#ifndef __FPSumOf3Squares_HPP
#define __FPSumOf3Squares_HPP
#include <vector>
#include <sstream>

#include "Operator.hpp"
#include "Shifters.hpp"
#include "LZOC.hpp"
#include "LZOCShifterSticky.hpp"
#include "IntAdder.hpp"
#include "IntMultiAdder.hpp"
#include "IntMultiplier.hpp"
#include "IntSquarer.hpp"
#include "FPMult.hpp"
#include "FPAddSinglePath.hpp"
#include "TestBenches/FPNumber.hpp"
#include "utils.hpp"

namespace flopoco{

	class FPSumOf3Squares : public Operator
	{
	public:
		FPSumOf3Squares(Target* target, int wE, int wF, int optimize);
		~FPSumOf3Squares();

		void emulate(TestCase * tc);

		TestCase* buildRandomTestCase(int i);

		int wE, wF;

	};
}
#endif
