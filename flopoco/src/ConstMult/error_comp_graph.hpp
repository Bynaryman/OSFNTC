#ifndef ERROR_COMP_GRAPH_HPP
#define ERROR_COMP_GRAPH_HPP

#ifdef HAVE_PAGLIB

#include <iostream>
#include <map>
#include <set>

#include <pagsuite/adder_graph.h>
#include "IntConstMultShiftAddTypes.hpp"
#include "gmp.h"
#include "gmpxx.h"

using namespace std;
using namespace PAGSuite;

namespace IntConstMultShiftAdd_TYPES {
	struct ErrorStorage {
			mpz_class positive_error;
			mpz_class negative_error;

			ErrorStorage shift(int shift);

			ErrorStorage():positive_error{0}, negative_error{0}{}

			ErrorStorage& operator+=(ErrorStorage const & rhs);
			void swap();
	};

	void print_aligned_word_graph(
			adder_graph_t & adder_graph,
			TruncationRegister truncationReg,
			int input_word_size,
			ostream & output_stream
	);

	void print_aligned_word_graph(
			adder_graph_t & adder_graph,
			string truncations,
			int input_word_size,
			ostream & output_stream
	);

	void df_fill_error(
			adder_graph_base_node_t* base_node,
			TruncationRegister const & truncReg,
			set<adder_graph_base_node_t*>& visited,
			map<adder_graph_base_node_t*, ErrorStorage>& errors,
			map<adder_graph_base_node_t*, int>& propagated_zeros
		);

	void df_accumulate_error(
			adder_graph_base_node_t* base_node,
			TruncationRegister const & truncReg,
			set<adder_graph_base_node_t*>& visited,
			map<adder_graph_base_node_t*, ErrorStorage>& errors,
			map<adder_graph_base_node_t*, int>& propagated_zeros,
			ErrorStorage& tot_error
		);

	ErrorStorage getAccumulatedErrorFor(
			output_node_t* output_node,
			TruncationRegister const & truncReg
		);

	void print_aligned_word_node(
		adder_graph_base_node_t* node,
		TruncationRegister const & truncationReg,
		int right_shift,
		int total_word_size,
		int input_word_size,
		int  truncation,
		ostream& output_stream
		);

	ErrorStorage get_node_error_ulp(
		adder_graph_base_node_t* node,
		TruncationRegister const & truncReg
	);
}

#endif //HAVE_PAGLIB

#endif
