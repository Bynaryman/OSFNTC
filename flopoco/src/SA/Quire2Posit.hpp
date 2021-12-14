/*

  Normalazing Unit to eventually put after PDFDP
  Ripple the carry if partial CSA
  Correctly round the fixed point exact accumulation to packed posit format

  Author: Ledoux Louis

 */

#ifndef QUIRE2POSIT_HPP
#define QUIRE2POSIT_HPP

#include "Operator.hpp"

namespace flopoco{

	class Quire2Posit : public Operator
	{
	public:

		/**
		 * @brief The Quire2Posit<n,es> constructor
		 * @param[in] {Target*} target : the target device
		 * @param[in] {int} n : the word width of the posit
		 * @param[in] {int} es : the tunable exponent size
		 * @param[in] {int=n-1} nb_bits_ovf : The number of bits after the actual MSB weight of summands
		 * @param[in] {int=-1} LSBQ : The weight of the LSB of the customizable Quire
		 **/
		Quire2Posit(OperatorPtr parentOp, Target* target, int n, int es, bool has_carry=false, int nb_bits_ovf=-1, int LSBQ=-1);

		/**
		 * Quire2Posit destructor
		 */
		~Quire2Posit();

		/**
		 * Emulate the operator using MPFR.
		 * @param tc a TestCase partially filled with input values
		 */
		void emulate(TestCase * tc);

		TestCase* buildRandomTestCase(int i);

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		static void registerFactory();

	protected:

		int m_n;  // the posit word size
		int m_es;  // the tunable exponent size
		bool m_has_carry;
		int m_nb_bits_ovf;  // the number of bits to add binades to prevent overflow
		int m_LSBQ;  // the tunable accuracy of accumulator
		int m_MSB_summand;  // the maximum weight of incoming summand
		int m_MSBQ;  // the maximum weight of the Quire
		int m_BIAS; // the posit exponent bias
		int m_max_absolute_dynamic_range;
		int m_wQ;
	};
}

#endif  // QUIRE2POSIT_HPP
