#ifndef IEEE_TO_S3_HPP
#define IEEE_TO_S3_HPP

#include "Operator.hpp"

namespace flopoco {

	class IEEE_to_S3 : public Operator {

	public:
		IEEE_to_S3(Target* target, Operator* parentOp, const int exponent_width, const int mantissa_width);

		// ~IEEE_to_S3();

		void emulate(TestCase * tc);

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
		static void registerFactory();

	private:
		int m_exponent_width;
		int m_mantissa_width;
	};

}

#endif  // IEEE_TO_S3_HPP
