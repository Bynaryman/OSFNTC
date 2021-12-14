/*
  S3FDP(<Sign,Significand,Scale> Fused Dot Product) for FloPoCo

  Author: Ledoux Louis

 */

#ifndef S3FDP_HPP
#define S3FDP_HPP
#include <vector>

#include "Operator.hpp"
#include "IntMult/IntMultiplier.hpp"
#include "IntAddSubCmp/IntAdder.hpp"

namespace flopoco{

	class S3FDP : public Operator
	{
	public:

		/**
		 * @brief The S3FDP constructor
		 * @param[in] {Target*} target : the target device
		 * @param[in] {int} scale_width : the bitwidth of incoming scales. (This also encodes the dynamic range since it is base 2)
		 * @param[in] {int} fraction_width : the bitwidth of the fraction in the significand (implicit+fraction : 1 + wF)
		 * @param[in] {int} bias : The scales biases of S3 inputs.
		 * @param[in] {int=n-1} nb_bits_ovf : The number of bits prepended before the actual MSB weight of summands
		 * @param[in] {int= 2*(2^(scale_width-1)-1)} msb_summand : The expected biggest product value weight
		 * @param[in] {int=-2*(2^(scale_width-1)-1)} lsb_summand : The expected smallest product value wieght
		 * @param[in] {int=-1} chunk_size : The user-suggested size of a chunk in the partial CSA. Computed by flopoco if not provided.
		 * @param[in] {float=0.0} dspOccupationThreshold : the ratio of DSP to be used in the mantissa product
		 * @param[in] {bool=true} has_HSSD : indicates if the PE contains the half speed sink down chain
		 **/
		S3FDP(OperatorPtr parentOp, Target* target,
				int scale_width,
				int fraction_width,
				int bias,
				int nb_bits_ovf=-1,
				int msb_summand=-1,
				int lsb_summand=-1,
				int chunk_size=-1,
				double dspOccupationThreshold = 0.0,
				bool has_HSSD=true);

		/**
		 * S3FDP destructor
		 */
		~S3FDP();

		/**
		 * Emulate the operator using MPFR.
		 * @param tc a TestCase partially filled with input values
		 */
		void emulate(TestCase * tc);

		// return the number of carry bits
		int get_wC();

		bool hasCOutput();

		TestCase* buildRandomTestCase(int i);

		int get_wLAICPT2();

		int getPipelineDepth();


		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		static void registerFactory();

	protected:

		int m_scale_width;
		int m_fraction_width;
		int m_bias;
		int m_nb_bits_ovf;
		int m_msb_summand;
		int m_lsb_summand;
		int m_chunk_size;
		float m_dspOccupationThreshold;
		bool m_has_HSSD;

		Signal::ResetType m_reset_type;
		std::list<int> m_ftz_clock_positions; // for testbench
		int m_nb_chunk;
		int m_wLAICPT2;
		int m_last_chunk_size;
	};
}

#endif  // S3FDP_HPP
