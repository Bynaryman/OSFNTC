/*
  One implementation of a processing element (PE) for Systolic Array (SA) with PDFDP

  Author: Ledoux Louis

 */

#include "SA/PositUtils.hpp"
#include "PE.hpp"

#include <vector>
#include <sstream>

#include "Operator.hpp"
#include "SA/PDFDP.hpp"
using namespace std;

namespace flopoco{

	PE::PE(
			OperatorPtr parentOp,
			Target* target,
			int n,
			int es
			):
				Operator(parentOp, target){

		ostringstream module_name;
		module_name << "pe_" << n << "_" << es;
		setNameWithFreqAndUID(module_name.str());
		setCopyrightString("Ledoux Louis - BSC / UPC");

		const int pd_size_normal = get_pd_size(n, es, POSIT_MEMORY);
		PDFDP * pdfdp_dummy = new PDFDP(nullptr, target, n, es);
		int wQ =  pdfdp_dummy->get_wQ();
		bool hasCOutput = pdfdp_dummy->hasCOutput();

		// IOs declaration
		addInput("pd_row_i_A", pd_size_normal);
		addInput("pd_col_j_B", pd_size_normal);
		addInput("C_out", (hasCOutput ? 2*wQ : wQ));
		addInput("SOB");  // active high boolean to put the accumulator registers to 0
		addInput("EOB");  // active high boolean to indicate the end of an accumulation

		addOutput("pd_row_im1_A", pd_size_normal);
		addOutput("pd_col_jm1_B", pd_size_normal);
		addOutput("SOB_Q");
		addOutput("EOB_Q");
		addOutput("C_out_Q", (hasCOutput ? 2*wQ : wQ));

		// Delay the triplet buses
		addFullComment("Functional delay z-1 of inputs");
		addRegisteredSignalCopy("pd_row_i_A_q", "pd_row_i_A", Signal::noReset);
		addRegisteredSignalCopy("pd_col_j_B_q", "pd_col_j_B", Signal::noReset);
		vhdl << endl;

		// Carry and delay Start of Block and end of Block (SOB + EOB)
		addFullComment("DQ flip flop for SOB and EOB");
		addRegisteredSignalCopy("sob_delayed", "SOB", Signal::noReset);
		addRegisteredSignalCopy("eob_delayed", "EOB", Signal::noReset);
		vhdl << endl;

		// Feed the inputs to the PDFDP
		delete pdfdp_dummy;
		addFullComment("Instantiates the PDFDP");
		ostringstream parametric_PDFDP, inputs_PDFDP, outputs_PDFDP;
		parametric_PDFDP << "n=" << n;
		parametric_PDFDP << " es=" << es;
		inputs_PDFDP << "pd_x=>pd_row_i_A,";
		inputs_PDFDP << "pd_y=>pd_col_j_B,";
		inputs_PDFDP << "FTZ=>SOB,";
		inputs_PDFDP << "EOB=>EOB";
		outputs_PDFDP << "A=>A_pdfdp";
		outputs_PDFDP << ",EOB_Q=>EOB_sync";
		if (hasCOutput) outputs_PDFDP << ",C=>C_pdfdp";
		newInstance("PDFDP", "pdfdp_inst", parametric_PDFDP.str(), inputs_PDFDP.str(), outputs_PDFDP.str());
		vhdl << endl;

		// Create the Half Speed Sink Down
		addFullComment("Half Speed Sink Down");
		vhdl << tab << "with EOB_sync select " << declare(getTarget()->logicDelay((hasCOutput ? 2*wQ:wQ)), "mux_C_out", (hasCOutput ? 2*wQ:wQ), true, Signal::wire) << " <= " << endl <<
			tab << "     " << (hasCOutput ? "A_pdfdp & C_pdfdp" : "A_pdfdp") << " when '1', " << endl <<
			tab << "     C_out" << " when others;" << endl;
		addRegisteredSignalCopy("mux_C_out_HSSD", "mux_C_out", Signal::noReset, 2);
		vhdl << endl;

		// Compose output
		addFullComment("Compose the outputs signals");
		vhdl << tab << "pd_row_im1_A <= pd_row_i_A_q;" << endl;
		vhdl << tab << "pd_col_jm1_B <= pd_col_j_B_q;" << endl;
		vhdl << tab << "SOB_Q <= sob_delayed;" << endl;
		vhdl << tab << "EOB_Q <= eob_delayed;" << endl;
		vhdl << tab << "C_out_Q <= mux_C_out_HSSD;" << endl;

	}

	PE::~PE() {}

	void PE::emulate(TestCase * tc) {}

	OperatorPtr PE::parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args) {
		int posit_width, posit_es;
		UserInterface::parseInt(args, "n", &posit_width);
		UserInterface::parseInt(args, "es", &posit_es);
		return new PE(parentOp, target, posit_width, posit_es);
	}

	void PE::registerFactory() {
		UserInterface::add(
			"PE",  // name
			"The classical processing element of an orthogonal Systolic Array for Mat Mat Mult. Delays both inputs and exactly accumulates them",  // description, string
			"SA",  // category, from the list defined in UserInterface.cpp
			"",  //seeAlso
			"n(int): The posit word size; \
			 es(int): The size of the tunable exponent",
			"",  // More documentation for the HTML pages. If you want to link to your blog, it is here.
			PE::parseArguments
		) ;
	}

}

