#ifdef HAVE_PAGLIB

#include "error_comp_graph.hpp"

#include<set>
#include <gmpxx.h>
#include <numeric>

using namespace PAGSuite;

namespace IntConstMultShiftAdd_TYPES {

	void print_aligned_word_node(
			adder_graph_base_node_t* node,
			TruncationRegister const & truncationReg,
			int right_shift,
			int total_word_size,
			int input_word_size,
			int truncation,
			ostream & output_stream
		)
	{
		if (is_a<output_node_t>(*node) || is_a<register_node_t>(*node)) {
			register_node_t& t = *((register_node_t*) node);
			print_aligned_word_node(
					t.input,
					truncationReg,
					right_shift	+ t.input_shift,
					total_word_size,
					input_word_size,
					truncation,
					output_stream
				);
			return;
		}

		int wordsize = computeWordSize(node->output_factor, input_word_size);
		int start = total_word_size - (wordsize + right_shift);
		int64_t output_factor = node->output_factor[0][0];

		output_stream << string(start, ' ');
		output_stream << string(wordsize - truncation, 'x');
		output_stream << string(truncation, '-');
		output_stream << string(1 + total_word_size - (wordsize + start), ' ');
		output_stream << output_factor << "X" << endl;
		if (is_a<adder_subtractor_node_t>(*node)) {
			output_stream << string(start, ' ' ) << "(" << endl;
			adder_subtractor_node_t & t = *(adder_subtractor_node_t*) node;
			size_t nb_inputs = t.inputs.size();
			auto truncVal = truncationReg.getTruncationFor(output_factor, t.stage);
			for (size_t i = 0 ; i < nb_inputs ; ++i) {
				print_aligned_word_node(
						t.inputs[i],
						truncationReg,
						right_shift + t.input_shifts[i],
						total_word_size,
						input_word_size,
						truncVal[i],
						output_stream
					);
			}
			output_stream << string(start, ' ' ) << ")" << endl;
		} 
	}

	void print_aligned_word_graph(
			PAGSuite::adder_graph_t &adder_graph,
			TruncationRegister truncationReg, int input_word_size,
			ostream &output_stream)
	{
		set<input_node_t*> inputs;
		set<output_node_t*> outputs;

		int max_word_size = 0;
		for (auto nodePtr : adder_graph.nodes_list) {
			int word_size = computeWordSize(nodePtr->output_factor, input_word_size);
			if (word_size > max_word_size) {
				max_word_size = word_size;
			}
			if (is_a<output_node_t>(*nodePtr)) {
				outputs.insert((output_node_t*) nodePtr);
			} else if  (is_a<input_node_t>(*nodePtr)) {
				inputs.insert((input_node_t*) nodePtr);
			}
		}

		for (auto outNodePtr : outputs) {
			output_stream << "Decomposition for " <<
						  outNodePtr->output_factor[0][0] << ":" << endl << endl ;
			print_aligned_word_node(
					outNodePtr,
					truncationReg,
					0,
					max_word_size + 1,
					input_word_size,
					0,
					output_stream
			);
			output_stream << endl;
		}
	}

	void print_aligned_word_graph(
			adder_graph_t & adder_graph,
			string truncations,
			int input_word_size,
			ostream & output_stream
		)
	{
		TruncationRegister truncationReg(truncations);
		print_aligned_word_graph(adder_graph,truncationReg,input_word_size,output_stream);
	}

	void df_fill_error(
			adder_graph_base_node_t* base_node,
			TruncationRegister const & truncReg,
			set<adder_graph_base_node_t*>& visited,
			map<adder_graph_base_node_t*, ErrorStorage>& errors,
			map<adder_graph_base_node_t*, int>& propagated_zeros
		)
	{
		if (visited.count(base_node) > 0) {
			return;
		}
		if (is_a<input_node_t>(*base_node)) {
			errors.insert(make_pair(base_node, ErrorStorage{}));
			propagated_zeros.insert(make_pair(base_node, 0));
			visited.insert(base_node);
			return;
		}
		
		if (is_a<register_node_t>(*base_node) || is_a<output_node_t>(*base_node)) {
			register_node_t* t = (register_node_t*) base_node;
			if (visited.count(t->input) < 1) {
				df_fill_error(t->input, truncReg, visited, errors, propagated_zeros);
			}
			errors.insert(make_pair(base_node, ErrorStorage{}));
			int input_zero = (propagated_zeros.find(t->input))->second;
			propagated_zeros.insert(make_pair(base_node, input_zero + t->input_shift));
			visited.insert(base_node);
			return;
		}

		if (not is_a<adder_subtractor_node_t>(*base_node)) {
			throw string("IntConstMultShiftAdd_TYPES::get_node_error_ulp : "
					"Don't know how to handle this type yet");
		}

		adder_subtractor_node_t* addsub = (adder_subtractor_node_t*) base_node;
		int min_kz = numeric_limits<int>::max();
		size_t i = 0;
		auto trunc = truncReg.getTruncationFor(
				addsub->output_factor[0][0],
				addsub->stage
			);

		ErrorStorage error;

		for (auto inputNodePtr : addsub->inputs) {
			if (visited.count(inputNodePtr) < 1) {
				df_fill_error(
						inputNodePtr, 
						truncReg, 
						visited, 
						errors, 
						propagated_zeros
					);
			}

			int kz_i = (propagated_zeros.find(inputNodePtr))->second;
			int truncation_i = trunc[i];
			int64_t shift_i = addsub->input_shifts[i];
			int real_loss = max(truncation_i - kz_i, 0);
			int extra_shift = truncation_i - real_loss;
			mpz_class loss_i = ((mpz_class{1} << real_loss) - 1) << (shift_i + extra_shift);
			if (addsub->input_is_negative[i]) {
				error.negative_error += loss_i;
			} else {
				error.positive_error += loss_i;
			}
			++i;

			int cur_kz = max(kz_i, truncation_i) + shift_i;
							
			if (cur_kz < min_kz) {
				min_kz = cur_kz;
			}
		}
		
		propagated_zeros.insert(make_pair(base_node, min_kz));
		errors.insert(make_pair(base_node, error));
		visited.insert(base_node);
	}

	void df_accumulate_error(
			adder_graph_base_node_t* base_node,
			TruncationRegister const & truncReg,
			set<adder_graph_base_node_t*>& visited,
			map<adder_graph_base_node_t*, ErrorStorage>& errors,
			map<adder_graph_base_node_t*, int>& propagated_zeros,
			ErrorStorage& tot_error
		)
	{
		if (visited.count(base_node) < 1) {
			df_fill_error(base_node, truncReg, visited, errors, propagated_zeros);
		}

		auto& cur_node_error = (errors.find(base_node))->second;
		tot_error += cur_node_error;

		if (is_a<register_node_t>(*base_node) || is_a<output_node_t>(*base_node)) {
			auto t = (register_node_t*) base_node;
			ErrorStorage sub_tot_error;
			df_accumulate_error(
					t->input, 
					truncReg, 
					visited, 
					errors,
					propagated_zeros, 
					sub_tot_error
				);
			sub_tot_error.shift(t->input_shift);			
			tot_error += sub_tot_error;
		} else if (is_a<adder_subtractor_node_t>(*base_node)) {
			auto t = (adder_subtractor_node_t*) base_node;
			size_t nb_inputs = t->inputs.size();
			ErrorStorage sub_tot;
			int neg_shift = (t->input_shifts[0] < 0) ? -1 * t->input_shifts[0] : 0;
			for (size_t i = 0 ; i < nb_inputs ; ++i) {
				ErrorStorage tmp;
				int  cur_shift = max(t->input_shifts[i], 0);
				df_accumulate_error(
						t->inputs[i],
						truncReg,
						visited,
						errors,
						propagated_zeros,
						tmp
					);
				tmp.shift(cur_shift);
				if (t->input_is_negative[i]) {
					tmp.swap();
				}
				sub_tot += tmp;
			}
			if (neg_shift > 0) {
				sub_tot.shift(-neg_shift);
			}
			
			tot_error += sub_tot;
		}
	}

	ErrorStorage getAccumulatedErrorFor(
			output_node_t* output_node,
			TruncationRegister const & truncReg
		)
	{
		ErrorStorage ret;
		set<adder_graph_base_node_t*> visited;
		map<adder_graph_base_node_t*, ErrorStorage> errors;
		map<adder_graph_base_node_t*, int> propagated_zeros;
		df_accumulate_error(
				output_node, 
				truncReg,
				visited,
				errors,
				propagated_zeros,
				ret
			);
		return ret;
	}

	ErrorStorage ErrorStorage::shift(int shift)
	{
		if (shift > 0) {
			positive_error <<= shift;
			negative_error <<= shift;
		} else {
			positive_error /= (mpz_class{1} << -shift);
			negative_error /= (mpz_class{1} << -shift);
		}
		 return *this;
	}

	ErrorStorage& ErrorStorage::operator+=(ErrorStorage const & rhs)
	{
		positive_error += rhs.positive_error;
		negative_error += rhs.negative_error;
		return *this;
	}

	void ErrorStorage::swap()
	{
		std::swap(positive_error, negative_error);
	}
}

#endif //HAVE_PAGLIB