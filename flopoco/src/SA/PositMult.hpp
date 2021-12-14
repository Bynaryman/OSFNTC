/*
  Posit Multiplier for FloPoCo

  Author: Ledoux Louis

 */

#ifndef POSITMULT_HPP
#define POSITMULT_HPP
#include <vector>

#include "Operator.hpp"
#include "IntMult/IntMultiplier.hpp"
#include "IntAddSubCmp/IntAdder.hpp"

namespace flopoco{

	class PositMult : public Operator
	{
	public:

		/**
		 * @brief The PositMult<n,es> constructor
		 * @param[in] {Target*} target : the target device
		 * @param[in] {int} n : the word width of the posit
		 * @param[in] {int} es : the tunable exponent size
		 * @param[in] {float} dspOccupationThreshold : the ratio of DSP to be used in the mantissa product
		 **/
		PositMult(OperatorPtr parentOp, Target* target, int n, int es, double dspOccupationThreshold = 0.0);

		/**
		 * PositMult destructor
		 */
		~PositMult();

		/**
		 * Emulate the operator using MPFR.
		 * @param tc a TestCase partially filled with input values
		 */
		void emulate(TestCase * tc);

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		static void registerFactory();

	protected:

		int m_n;  // the posit word size
		int m_es;  // the tunable exponent size
		float m_dspOccupationThreshold;  // threshold of relative occupation ratio of a DSP multiplier to be used or not

	};
}

#endif
