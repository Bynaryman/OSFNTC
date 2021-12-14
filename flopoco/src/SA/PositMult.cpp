/*
  Posit Multiplier for FloPoCo

  Author: Ledoux Louis

 */

#include "SA/PositUtils.hpp"
#include "PositMult.hpp"

#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "Operator.hpp"
#include "IntMult/IntMultiplier.hpp"
#include "IntAddSubCmp/IntAdder.hpp"

using namespace std;

namespace flopoco{

	PositMult::PositMult(
			OperatorPtr parentOp,
			Target* target,
			int n,
			int es,
			double dspOccupationThreshold) :
				Operator(parentOp, target),
				m_n(n),
				m_es(es),
				m_dspOccupationThreshold(dspOccupationThreshold) {

		ostringstream module_name;
		module_name << "posit_mult" << n << "_" << es;
		setNameWithFreqAndUID(module_name.str());
		setCopyrightString("Ledoux Louis - BSC / UPC");

		REPORT(DEBUG, "this operator received 3 parameters: " << m_n << " / " << m_es << " / " << m_dspOccupationThreshold);

		int mantissa_size, scale_size; get_pd_components_width(m_n, m_es, scale_size, mantissa_size);
		const int pd_size_normal = get_pd_size(m_n, m_es, POSIT_MEMORY);
		const int pd_size_mult = get_pd_size(m_n, m_es, POSIT_MULT);

		// IOs declaration
		addInput("pd_x", pd_size_normal);
		addInput("pd_y", pd_size_normal);
		addOutput("pd_r", pd_size_mult);


		// sign processing
		addFullComment("sign processing");
		vhdl << tab << declare(.0, "sign_X", 1, false, Signal::wire) << " <= pd_x" << of(pd_size_normal-1) << ";" << endl;
		vhdl << tab << declare(.0, "sign_Y", 1, false, Signal::wire) << " <= pd_y" << of(pd_size_normal-1) << ";" << endl;
		vhdl << tab << declare(target->logicDelay(2), "sign_R", 1, false, Signal::wire) << " <= sign_X xor sign_Y;" << endl;
		vhdl << endl;

		// mantissa product processing
		addFullComment("significand processing");
		vhdl << tab << declare(.0, "mantissa_X", 1 + mantissa_size, true, Signal::wire) << " <= \"1\" & pd_x" << range(mantissa_size-1,0) << ";" << endl;
		vhdl << tab << declare(.0, "mantissa_Y", 1 + mantissa_size, true, Signal::wire) << " <= \"1\" & pd_y" << range(mantissa_size-1,0) << ";" << endl;

		ostringstream parametric, inputs, outputs;
		parametric << "wX=" << mantissa_size + 1;
		parametric << " wY=" << mantissa_size + 1;
		parametric << " wOut=" << 2*(mantissa_size + 1);
		parametric << " dspThreshold=" << m_dspOccupationThreshold;
		inputs << "X=>mantissa_X,";
		inputs << "Y=>mantissa_Y";
		outputs << "R=>significand_product";
		newInstance("IntMultiplier", "significand_product_inst", parametric.str(), inputs.str(), outputs.str());
		vhdl << endl;

		// detect product significand overflow to eventually re-adjust scale
		addFullComment("detect product significand overflow to eventually re-adjust scale");
		vhdl << tab << declare(.0, "mantissa_ovf", 1, false, Signal::wire) << " <= significand_product" << of((2*(mantissa_size+1)-1)) << ";" << endl;

		// scale processing
		addFullComment("scale processing");
		vhdl << tab << declare(.0, "scale_X", scale_size, true, Signal::wire) << " <= pd_x" << range(scale_size+mantissa_size-1,mantissa_size) << ";" << endl;
		vhdl << tab << declare(.0, "scale_Y", scale_size, true, Signal::wire) << " <= pd_y" << range(scale_size+mantissa_size-1,mantissa_size) << ";" << endl;

		// build the output result PD
		addFullComment("build the final PD");
		vhdl << tab << "pd_r <= sign_R & \"" << string(pd_size_mult-1, '0') << "\";" << endl;;
	}

	PositMult::~PositMult() {}

	void PositMult::emulate(TestCase * tc) {}

	OperatorPtr PositMult::parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args) {
		int posit_width, posit_es;
		double dspThreshold = 0.0f;
		UserInterface::parseInt(args, "n", &posit_width);
		UserInterface::parseInt(args, "es", &posit_es);
		UserInterface::parseFloat(args, "dspThreshold", &dspThreshold);
		return new PositMult(parentOp, target, posit_width, posit_es, dspThreshold);
	}

	void PositMult::registerFactory() {
		UserInterface::add(
			"PositMult",  // name
			"Multiplies 2 PD (posit denormalize) into a full precision one",  // description, string
			"SA",  // category, from the list defined in UserInterface.cpp
			"",  //seeAlso
			"n(int): The posit word size; \
			 es(int): The size of the tunable exponent; \
			 dspThreshold(real)=0.0: The ratio of dsp over logic for mantissa product",
			"",  // More documentation for the HTML pages. If you want to link to your blog, it is here.
			PositMult::parseArguments
		) ;
	}

}
