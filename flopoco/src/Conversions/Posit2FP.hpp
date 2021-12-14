#ifndef POSIT2FP_HPP
#define POSIT2FP_HPP

#include "Operator.hpp"

namespace flopoco {
	class Posit2FP : public Operator
	{
	public:
		/**
		 * @brief Constructor
		 * @param[in]	target 	the target device
		 * @param[in]	widthI	the total width of the input posit
		 * @param[in]	esI		the exponent field size of the input prosit
		 */
		Posit2FP(Operator* parentOp, Target* target, int widthI, int esI);

		void emulate(TestCase* tc);
		static void computeFloatWidths(int const widthI, int const eS, int* wE, int* wF);
		
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target* target, vector<string> &args);
		static void registerFactory();

	private:
		int wE_;
		int wF_;
		int widthI_;
		int esI_;
	};
}

#endif
