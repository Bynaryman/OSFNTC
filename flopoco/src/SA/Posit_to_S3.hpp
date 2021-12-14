#ifndef POSIT_TO_S3_HPP
#define POSIT_TO_S3_HPP

#include "Operator.hpp"

namespace flopoco {

	class Posit_to_S3 : public Operator {

	public:
		Posit_to_S3(Target* target, Operator* parentOp, const int posit_width, const int posit_es);

		// ~Posit_to_S3();

		void emulate(TestCase * tc);

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
		static void registerFactory();

	private:
		int m_posit_width;
		int m_posit_es;
		int m_scale_width;
		int m_fraction_width;
	};

}

#endif  // POSIT_TO_S3_HPP
