#ifdef HAVE_PAGLIB

#include <algorithm>
#include <iostream>
#include "adder_cost.hpp"

namespace IntConstMultShiftAdd_TYPES {
int getNodeCost(
		vector<int> word_sizes, 
		vector<int> truncations,
		vector<int> shifts,
		vector<bool> is_neg
		//int out_word_size
	) 
{
	int out_word_size = 0;
	size_t nb_inputs = word_sizes.size();
	if (nb_inputs < 2) {
		return 0;
	}
	vector<int> known_zeros(nb_inputs);
	vector<size_t> order(nb_inputs);
	for (size_t i = 0 ; i < nb_inputs ; i++) {
		int cur_shift = max(shifts[i], 0);
		known_zeros[i] = truncations[i] + cur_shift;
		order[i] = i;
		if (word_sizes[i] + shifts[i] > out_word_size) {
			out_word_size = word_sizes[i] + shifts[i];
		}
	}

	int negShifts = max(-1 * shifts[0], 0);

	auto comp = [known_zeros](const size_t & a, const size_t& b)->bool{
		return known_zeros[a] < known_zeros[b];
	};

	std::sort(order.begin(), order.end(), comp);
	int min_kz = known_zeros[order[0]];
	int second_min_kz = known_zeros[order[1]];
	int copy_as_is = (is_neg[order[0]]) ? min_kz : second_min_kz;

	return out_word_size + negShifts - copy_as_is;
}

int getGraphAdderCost(
		PAGSuite::adder_graph_t& adder_graph,
		int inputWordSize,
		bool unsignedInput,
		string truncations
	)
{
	TruncationRegister truncReg(truncations);
	return getGraphAdderCost(adder_graph, inputWordSize, unsignedInput, truncReg);
}

int getGraphAdderCost(
		PAGSuite::adder_graph_t &adder_graph, 
		int inputWordSize,
		bool unsignedInput, 
		TruncationRegister const &truncReg
	)
{
	int totalCost = 0;
	for (auto nodePtr : adder_graph.nodes_list) {
		if (PAGSuite::is_a<PAGSuite::adder_subtractor_node_t>(*nodePtr)) {
//			int64_t factor = nodePtr->output_factor[0][0];
//			cout << "Number of adders for factor " << factor << " : ";
			PAGSuite::adder_subtractor_node_t* t =
					(PAGSuite::adder_subtractor_node_t*) nodePtr;
			int nodeOpSize = computeWordSize(t, inputWordSize);
			vector<int> truncationsVal = truncReg.getTruncationFor(
					t->output_factor[0][0],
					t->stage
			);
			vector<int> shiftsVal;
			for (auto shift : t->input_shifts) {
				shiftsVal.push_back(shift);
			}
			vector<bool> isNegVal(t->input_is_negative);
			vector<int> wordSizesVal;

			for (auto prevPtr : t->inputs) {
				wordSizesVal.push_back(
						computeWordSize(
								prevPtr,
								inputWordSize
						)
				);
			}
			if (unsignedInput) {
				vector<int> groupWordSize;
				auto groups = splitNonOverlap(
						wordSizesVal,
						truncationsVal,
						shiftsVal,
						isNegVal,
						nodeOpSize,
						groupWordSize
				);
				size_t nb_groups = groups.size();
				for (size_t i = 0 ; i < nb_groups ; ++i) {
					auto cur_group = groups[i];
					vector<int> wordSizesGroup;
					vector<int> truncationsGroup;
					vector<int> shiftsGroup;
					vector<bool> isNegGroup;
					for (auto idx : cur_group) {
						wordSizesGroup.push_back(wordSizesVal[idx]);
						truncationsGroup.push_back(truncationsVal[idx]);
						shiftsGroup.push_back(wordSizesVal[idx]);
						isNegGroup.push_back(wordSizesVal[idx]);
						totalCost += getNodeCost(
								wordSizesGroup,
								truncationsGroup,
								shiftsGroup,
								isNegGroup
						);
					}
				}
			} else {
				auto tmp = getNodeCost(
						wordSizesVal,
						truncationsVal,
						shiftsVal,
						isNegVal
				);
//				cout << tmp << endl;
				totalCost += tmp;
			}
		}
	}
	return totalCost;
}

vector<vector<size_t>> splitNonOverlap(
			vector<int> word_sizes, 
			vector<int> truncations,
			vector<int> shifts,
			vector<bool> is_neg,
			int  out_word_size,
			vector<int>& groupWordSize
		)
{
	size_t nb_input = word_sizes.size();
	vector<int> kz(nb_input);
	vector<int> left_border(nb_input);
	vector<size_t> order(nb_input);

	for (size_t i = 0 ; i < nb_input ; ++i) {
		int cur_shift = max(0, shifts[i]);		
		kz[i] = truncations[i] + cur_shift;
		order[i] = i;
		left_border[i] = (is_neg[i]) ? out_word_size : word_sizes[i] + cur_shift;
	}

	auto comp = [kz](size_t const & a, size_t const & b)->bool{
		return kz[a] < kz[b];
	};

	vector<vector<size_t>> ret;
	vector<size_t> curSet;
	int  leftbound = left_border[order[0]];
	for (size_t i = 0 ; i < nb_input ; ++i) {
		size_t cur_idx = order[i];			
		int group_left_bound = leftbound;
		if (curSet.size() > 0) {
			group_left_bound += intlog2(curSet.size());
		}
		if (kz[cur_idx] < group_left_bound) {
			leftbound = max(leftbound, left_border[cur_idx]);
		} else {
			groupWordSize.push_back(group_left_bound);
			ret.push_back(curSet);
			curSet.empty();
			leftbound = left_border[cur_idx];
		}
	}
	ret.push_back(curSet);
	return ret;
}

}

#endif //HAVE_PAGLIB
