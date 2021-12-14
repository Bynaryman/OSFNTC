/*

  Systolic Array Kernel w/o FF for data alignment and denorm of input and norm of outputs
  Basically arranges in a topology PEs with links.
  Author: Ledoux Louis

 */

#ifndef SYSTOLIC_ARRAY_KERNEL_HPP
#define SYSTOLIC_ARRAY_KERNEL_HPP
#include <vector>

#include "Operator.hpp"
#include "SA/PE_S3.hpp"
#include "SA/S3FDP.hpp"

namespace flopoco{

	class SystolicArrayKernel : public Operator
	{
	public:

		/**
		 * @brief A SystolicArrayKernel constructor
		 * @param[in] {Target*} target : the target device
		 * @param[in] {int} N : width of the systolic array
		 * @param[in] {int} M : height of the systolic array
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
		SystolicArrayKernel(OperatorPtr parentOp, Target* target,
				int N,
				int M,
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
		 * SystolicArrayKernel destructor
		 */
		~SystolicArrayKernel();

		string buildVHDLSignalDeclarations();

		/**
		 * @param tc a TestCase partially filled with input values
		 */
		void emulate(TestCase * tc);

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		static void registerFactory();

	protected:
		int m_N;  // width of the Systolic Array
		int m_M;  // height of the Systolic Array
		int m_scale_width;
		int m_fraction_width;
		int m_bias;
		int m_nb_bits_ovf;
		int m_msb_summand;
		int m_lsb_summand;
		int m_chunk_size;
		float m_dspOccupationThreshold;
		bool m_has_HSSD;

	};
}

#endif  // SYSTOLIC_ARRAY_KERNEL_HPP
