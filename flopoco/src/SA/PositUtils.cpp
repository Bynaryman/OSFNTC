#include "SA/PositUtils.hpp"

namespace flopoco {

	void get_pd_components_width(const int posit_width, const int posit_es, int &scale_width, int &fraction_width) {
		//scale_width = ceil(log2(posit_width-2)) + 1 + posit_es;
		scale_width = intlog2(posit_width-2) + 1 + posit_es;
		fraction_width = posit_width - (posit_es + 3);
	}

	int get_pd_size(const int posit_width, const int posit_es, const PD_TYPE pd) {
		int scale_width = intlog2(posit_width-2) + 1 + posit_es;
		//int scale_width = ceil(log2(posit_width-2)) + 1 + posit_es;
		int fraction_width = posit_width - (posit_es + 3);
		switch(pd) {
			case POSIT_MEMORY:
				return scale_width + fraction_width + 3;
				break;
			case POSIT_MULT:
				return (scale_width+1) + (2*(fraction_width+1)) + 3;
				break;
			default:
				return -1;
				break;
		}
	}

	int get_BIAS(const int posit_width, const int posit_es, int p1) {
		return ((posit_width-2)<<posit_es) + p1;
	}

	void parse_arithmetic(vector<string> * arithmetic, int &scale_width, int &significand_width, int &bias, bool &subnormals, int &dense) {
		if ((arithmetic->at(0)).compare("posit") == 0) {
			int es = 0;
			int n_dense  = stoi(arithmetic->at(1));
			if (arithmetic->size() >= 3) {  // es value has been provided
				es = stoi(arithmetic->at(2));
			}
			else {  // no es value provided, guessing the new posit draft with es always two
				es = 2;
			}
			get_pd_components_width(n_dense, es, scale_width, significand_width);
			bias = get_BIAS(n_dense, es, 0);
			subnormals = false;
			dense = n_dense;
		}
		else if (arithmetic->at(0).compare("ieee") == 0) {
			scale_width = stoi(arithmetic->at(1));
			significand_width = stoi(arithmetic->at(2));
			bias = (1<<(scale_width-1))-1;
			subnormals = true;
			dense = scale_width + significand_width + 1;
		}
		else if (arithmetic->at(0).compare("tfp") == 0) {
			scale_width = stoi(arithmetic->at(1));
			significand_width = stoi(arithmetic->at(2));
			bias = (1<<(scale_width-1))-1;
			subnormals = false;
			dense = scale_width + significand_width + 1;
		}
		else if (arithmetic->at(0).compare("bfloat16") == 0) {
			scale_width = 8;
			significand_width = 7;
			bias = (1<<(scale_width-1))-1;  // 127
			subnormals = false;
			dense = scale_width + significand_width + 1;
		}

	}
}
