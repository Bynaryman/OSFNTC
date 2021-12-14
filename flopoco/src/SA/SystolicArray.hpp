/*

  Systolic Array
  Author: Ledoux Louis

 */

#ifndef SYSTOLIC_ARRAY_HPP
#define SYSTOLIC_ARRAY_HPP
#include <vector>

#include "Operator.hpp"
#include <boost/algorithm/string/join.hpp>

namespace flopoco{

	class SystolicArray : public Operator
	{
	public:

		/**
		 * @brief A SystolicArrays constructor
		 * @param[in] {Target*} target : the target device
		 * @param[in] {int} N : width of the systolic array
		 * @param[in] {int} M : height of the systolic array
		 * @param[in] {vector<string>} arithmetic_in : The arithmetic used in the array as a list of parameters
		 * @param[in] {int=n-1} nb_bits_ovf : The number of bits prepended before the actual MSB weight of summands
		 * @param[in] {int= 2*(2^(scale_width-1)-1)} msb_summand : The expected biggest product value weight
		 * @param[in] {int=-2*(2^(scale_width-1)-1)} lsb_summand : The expected smallest product value wieght
		 * @param[in] {int=-1} chunk_size : The user-suggested size of a chunk in the partial CSA. Computed by flopoco if not provided.
		 * @param[in] {vector<string>} arithmetic_out : The arithmetic used after the array to return to the external world as a list of parameters
		 * @param[in] {float=0.0} dspOccupationThreshold : the ratio of DSP to be used in the mantissa product
		 * @param[in] {bool=true} has_HSSD : indicates if the PE contains the half speed sink down chain
	     **/
		SystolicArray(OperatorPtr parentOp, Target* target,
			int N,
			int M,
			vector<string>* arithmetic_in,
			vector<string>* arithmetic_out,
			int nb_bits_ovf=-1,
			int msb_summand=-1,
			int lsb_summand=-1,
			int chunk_size=-1,
			double dspOccupationThreshold = 0.0,
			bool has_HSSD=true);

		/**
		 * destructor
		 */
		~SystolicArray();

		string buildVHDLSignalDeclarations();

		void buildStandardTestCases(TestCaseList* tcl);

		TestCase* buildRandomTestCase(int i);

		/**
		 * @param tc a TestCase partially filled with input values
		 */
		void emulate(TestCase * tc);

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		static void registerFactory();

	protected:
		int m_N;  // width of the Systolic Array
		int m_M;  // height of the Systolic Array
		// pre SA section
		int m_dense_in;  // bitwidth at SA border
		int m_s3_in;  // bitwidth at SAK border
		int m_scale_width_in;
		int m_fraction_width_in;
		int m_bias_in;
		bool m_subnormals_in;
		// LAICPT2 section
		int m_nb_bits_ovf;
		int m_msb_summand;
		int m_lsb_summand;
		int m_chunk_size;
		// post SA section
		int m_dense_out;  // bitwidth at SA border
		int m_scale_width_out;
		int m_fraction_width_out;
		int m_bias_out;
		bool m_subnormals_out;
		bool m_exact_return;
		// global param section
		float m_dspOccupationThreshold;
		bool m_has_HSSD;

	};
}

#endif  // SYSTOLIC_ARRAY_HPP
