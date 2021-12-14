/*

  LAICPT2_to_arith
  Operator which converts the output of the long
  accumulator in 2s complement to the desired FP format
  Author: Ledoux Louis

 */
#ifndef LAICPT2_TO_ARITH_HPP
#define LAICPT2_TO_ARITH_HPP

#include "Operator.hpp"


namespace flopoco{

	class LAICPT2_to_arith : public Operator
	{
	public:

		/**
		 * @brief A LAICPT2_to_arith constructor
		 * @param[in] {Target*} target : the target device
		 * TODO(lledoux): complete doc
	        **/
		LAICPT2_to_arith(OperatorPtr parentOp, Target* target,
				// input part
				int nb_bits_ovf,
				int msb_summand,
				int lsb_summand,
				int nb_chunks,
				// output part
				vector<string>* arithmetic_in,
				vector<string>* arithmetic_out);


		/** Destructor */
		~LAICPT2_to_arith();

		void emulate(TestCase * tc);

		void buildStandardTestCases(TestCaseList* tcl);

		TestCase* buildRandomTestCase(int i);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */
		static void registerFactory();


	protected:
		// input part
		int m_nb_bits_ovf;
		int m_msb_summand;
		int m_lsb_summand;
		int m_nb_chunks;
		int m_chunk_size;
		int m_last_chunk_size;
		// output part
		int m_scale_width_in;
		int m_fraction_width_in;
		int m_bias_in;
		bool m_subnormals_in;
		int m_dense_in;
		bool m_exact_return;
		int m_scale_width_out;
		int m_fraction_width_out;
		int m_bias_out;
		bool m_subnormals_out;
		int m_dense_out;
		int m_es_out;
		bool m_is_posit_like;
		vector<string>* m_arithmetic_in;
		vector<string>* m_arithmetic_out;

	};
}
#endif
