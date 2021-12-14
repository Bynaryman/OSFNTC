#ifndef POSIT2PD_HPP
#define POSIT2PD_HPP

#include "Operator.hpp"

namespace flopoco {

	class Posit2PD : public Operator {

	public:
		Posit2PD(Target* target, Operator* parentOp, const int posit_width, const int posit_es);

		// ~Posit2PD();

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

#endif  // POSIT2PD_HPP
