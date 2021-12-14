#ifndef ADDER_COST_HPP
#define ADDER_COST_HPP

#include <vector>
#include <pagsuite/adder_graph.h>
#include "IntConstMultShiftAddTypes.hpp"

namespace IntConstMultShiftAdd_TYPES {
	int getNodeCost(
			vector<int> word_sizes, 
			vector<int> truncations,
			vector<int> shifts,
			vector<bool> is_neg
		);

	vector<vector<size_t> > splitNonOverlap(
			vector<int> word_sizes, 
			vector<int> truncations,
			vector<int> shifts,
			vector<bool> is_neg,
			int  out_word_size,
			vector<int>& groupWordSize
		);

	int getGraphAdderCost(
			PAGSuite::adder_graph_t & adder_graph,
			int inputWordSize,
			bool unsignedInput,
			TruncationRegister const &truncReg
	);

	int getGraphAdderCost(
		PAGSuite::adder_graph_t & adder_graph,			
		int inputWordSize,
		bool unsignedInput,
		string truncations = ""
	);
}
#endif
