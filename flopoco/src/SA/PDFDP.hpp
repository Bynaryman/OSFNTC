/*
  pdfdp(Posit Denormalized Fused Dot Product) for FloPoCo

  Author: Ledoux Louis

 */

#ifndef PDFDP_HPP
#define PDFDP_HPP
#include <vector>

#include "Operator.hpp"
#include "IntMult/IntMultiplier.hpp"
#include "IntAddSubCmp/IntAdder.hpp"

namespace flopoco{

	class PDFDP : public Operator
	{
	public:

		/**
		 * @brief The PDFDP<n,es> constructor
		 * @param[in] {Target*} target : the target device
		 * @param[in] {int} n : the word width of the posit
		 * @param[in] {int} es : the tunable exponent size
		 * @param[in] {int=n-1} nb_bits_ovf : The number of bits after the actual MSB weight of summands
		 * @param[in] {float=0.0} dspOccupationThreshold : the ratio of DSP to be used in the mantissa product
		 * @param[in] {int=-1} LSBQ : The weight of the LSB of the customizable Quire
		 * @param[in] {int=-1} chunk_size : The suggested size of a chunk in the partial CSA
		 **/
		PDFDP(OperatorPtr parentOp, Target* target, int n, int es, int nb_bits_ovf=-1, double dspOccupationThreshold = 0.0, int LSBQ=-1, int chunk_size=-1);

		/**
		 * PDFDP destructor
		 */
		~PDFDP();

		/**
		 * Emulate the operator using MPFR.
		 * @param tc a TestCase partially filled with input values
		 */
		void emulate(TestCase * tc);

		bool hasCOutput();

		TestCase* buildRandomTestCase(int i);

		int get_wQ();

		int getPipelineDepth();

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		static void registerFactory();

	protected:

		int m_n;  // the posit word size
		int m_es;  // the tunable exponent size
		int m_nb_bits_ovf;  // the number of bits to add binades to prevent overflow
		float m_dspOccupationThreshold;  // threshold of relative occupation ratio of a DSP multiplier to be used or not
		int m_LSBQ;  // the tunable accuracy of accumulator
		int m_chunk_size;  // the size in bits of a chunk for the partial carry save
		int m_MSB_summand;  // the maximum weight of incoming summand
		Signal::ResetType m_reset_type;
		int m_BIAS; // the posit exponent bias
		std::list<int> m_ftz_clock_positions; // for testbench
		int m_max_absolute_dynamic_range;
		int m_wQ;
		int m_nb_chunk;
	};
}

#endif  // PDFDP_HPP
