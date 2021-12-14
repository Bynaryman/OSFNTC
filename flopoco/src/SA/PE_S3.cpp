/*
  One implementation of a processing element (PE) for Systolic Array (SA) with S3FDP

  Author: Ledoux Louis

 */

//#include "SA/PositUtils.hpp"
#include "PE_S3.hpp"

#include <vector>
#include <sstream>

#include "Operator.hpp"
#include "SA/S3FDP.hpp"
using namespace std;

namespace flopoco{

	PE_S3::PE_S3(
			OperatorPtr parentOp,
			Target* target,
			int scale_width,
			int fraction_width,
			int bias,
			int nb_bits_ovf,
			int msb_summand,
			int lsb_summand,
			int chunk_size,
			double dspOccupationThreshold,
			bool has_HSSD):
				Operator(parentOp, target),
				m_scale_width(scale_width),
				m_fraction_width(fraction_width),
				m_bias(bias),
				m_nb_bits_ovf(nb_bits_ovf),
				m_msb_summand(msb_summand),
				m_lsb_summand(lsb_summand),
				m_chunk_size(chunk_size),
				m_dspOccupationThreshold(dspOccupationThreshold),
				m_has_HSSD(has_HSSD) {

		ostringstream module_name;
		module_name << "PE_S3_" << scale_width << "_" << fraction_width;
		if(m_has_HSSD) module_name << "_HSSD";
		setNameWithFreqAndUID(module_name.str());
		setCopyrightString("Ledoux Louis - BSC / UPC");

		const int S3_size = m_scale_width + m_fraction_width + 3;
		S3FDP * s3fdp_dummy = new S3FDP(nullptr, target,
			m_scale_width,
			m_fraction_width,
			m_bias,
			m_nb_bits_ovf,
			m_msb_summand,
			m_lsb_summand,
			m_chunk_size,
			m_dspOccupationThreshold,
			m_has_HSSD);
		s3fdp_dummy->setName("s3fdp");
		s3fdp_dummy->schedule();
		s3fdp_dummy->applySchedule();
		addSubComponent(s3fdp_dummy);
		int wLAICPT2 = s3fdp_dummy->get_wLAICPT2();
		int wCOutput = s3fdp_dummy->get_wC();

		// IOs declaration
		addInput("s3_row_i_A", S3_size);
		addInput("s3_col_j_B", S3_size);
		if (m_has_HSSD) addInput("C_out", wLAICPT2+wCOutput+1);  // accumulator + carry bits + nan
		addInput("SOB");  // active high boolean to put the accumulator registers to 0
		if (m_has_HSSD) addInput("EOB");  // active high boolean to indicate the end of an accumulation

		addOutput("s3_row_im1_A", S3_size);
		addOutput("s3_col_jm1_B", S3_size);
		addOutput("SOB_Q");
		if (m_has_HSSD) addOutput("EOB_Q");
		addOutput("C_out_Q", wLAICPT2+wCOutput+1);  // accumulator + carry bits + nan

		// Delay the S3 buses
		addFullComment("Functional delay z-1 of inputs");
		addRegisteredSignalCopy("s3_row_i_A_q", "s3_row_i_A", Signal::noReset);
		addRegisteredSignalCopy("s3_col_j_B_q", "s3_col_j_B", Signal::noReset);
		vhdl << endl;

		// Carry and delay Start of Block and end of Block (SOB + EOB)
		addFullComment("DQ flip flop for SOB and EOB");
		addRegisteredSignalCopy("sob_delayed", "SOB", Signal::noReset);
		if (m_has_HSSD) addRegisteredSignalCopy("eob_delayed", "EOB", Signal::noReset);
		vhdl << endl;

		// Feed the inputs to the S3FDP
		// delete s3fdp_dummy;
		// addFullComment("Instantiates the S3FDP");
		// ostringstream parametric_S3FDP, inputs_S3FDP, outputs_S3FDP;
		// parametric_S3FDP << "scale_width=" << m_scale_width;
		// parametric_S3FDP << " fraction_width=" << m_fraction_width;
		// parametric_S3FDP << " bias=" << m_bias;
		// parametric_S3FDP << " nb_bits_ovf=" << m_nb_bits_ovf;
		// parametric_S3FDP << " msb_summand=" << m_msb_summand;
		// parametric_S3FDP << " lsb_summand=" << m_lsb_summand;
		// parametric_S3FDP << " chunk_size=" << m_chunk_size;
		// parametric_S3FDP << " dspOccupationThreshold=" << m_dspOccupationThreshold;
		// parametric_S3FDP << " has_HSSD=" << m_has_HSSD;
		// std::cout << parametric_S3FDP.str() << std::endl;
		// inputs_S3FDP << "S3_x=>s3_row_i_A";
		// inputs_S3FDP << ",S3_y=>s3_col_j_B";
		// inputs_S3FDP << ",FTZ=>SOB";
		// if (m_has_HSSD) inputs_S3FDP << ",EOB=>EOB";
		// outputs_S3FDP << "A=>A_s3fdp";
		// if (m_has_HSSD) outputs_S3FDP << ",EOB_Q=>EOB_s3fdp";
		// outputs_S3FDP << ",isNaN=>isNaN_s3fdp";
		// if (wCOutput > 0) outputs_S3FDP << ",C=>C_s3fdp";
		// newInstance("S3FDP", "s3fdp_inst", parametric_S3FDP.str(), inputs_S3FDP.str(), outputs_S3FDP.str());
		// vhdl << endl;
		// std::cout << "here2" << std::endl;

		// Eventually create the Half Speed Sink Down
		// for paper purpose we have to remove HSSD to compare with nothing
		if (m_has_HSSD) {
			addFullComment("Half Speed Sink Down");
			vhdl << tab << "with EOB_s3fdp select " << declare(getTarget()->logicDelay(wLAICPT2+wCOutput+1), "mux_C_out", (wLAICPT2+wCOutput+1), true, Signal::wire) << " <= " << endl <<
				tab << "     (" << (wCOutput>0 ? "isNaN_s3fdp & A_s3fdp & C_s3fdp" : "isNaN_s3fdp & A_s3fdp") << ") when '1', " << endl <<
				tab << "     C_out" << " when others;" << endl;
			addRegisteredSignalCopy("mux_C_out_HSSD", "mux_C_out", Signal::noReset, 2);
			vhdl << endl;
		} else {
			addFullComment("without Half Speed Sink Down");
			vhdl << tab << declare(getTarget()->logicDelay(0), "out_s3fdp", (wLAICPT2+wCOutput+1), true, Signal::wire) << " <= " << (wCOutput>0 ? "isNaN_s3fdp & A_s3fdp & C_s3fdp" : "isNaN_s3fdp & A_s3fdp") << ";";
			vhdl << endl << endl;
		}

		setNoParseNoSchedule();
		addFullComment("Instantiates the S3FDP");
		if (wCOutput > 0) declare(0,"C_s3fdp",wCOutput);
		declare(0,"isNaN_s3fdp");
		if (m_has_HSSD) declare(0,"EOB_s3fdp");
		declare(0,"A_s3fdp", wLAICPT2);
		vhdl << tab << "s3fdp_inst: s3fdp" << endl;
		vhdl << tab << tab << "port map ( clk => clk," << endl;
        vhdl << tab << tab << "           rst  => rst," << endl;
        if (m_has_HSSD) vhdl << tab << tab << "           EOB => EOB," << endl;
        vhdl << tab << tab << "           FTZ => SOB," << endl;
        vhdl << tab << tab << "           S3_x => s3_row_i_A," << endl;
        vhdl << tab << tab << "           S3_y => s3_col_j_B," << endl;
        vhdl << tab << tab << "           A => A_s3fdp," << endl;
        if (wCOutput > 0) vhdl << tab << tab << "           C => C_s3fdp," << endl;
        if (m_has_HSSD) vhdl << tab << tab << "           EOB_Q => EOB_s3fdp," << endl;
        vhdl << tab << tab << "           isNaN => isNaN_s3fdp);" << endl;
		vhdl << endl;

		// Compose output
		addFullComment("Compose the outputs signals");
		vhdl << tab << "s3_row_im1_A <= s3_row_i_A_q;" << endl;
		vhdl << tab << "s3_col_jm1_B <= s3_col_j_B_q;" << endl;
		vhdl << tab << "SOB_Q <= sob_delayed;" << endl;
		if (m_has_HSSD) {
			vhdl << tab << "EOB_Q <= eob_delayed;" << endl;
			vhdl << tab << "C_out_Q <= mux_C_out_HSSD;" << endl;
		} else {
			vhdl << tab << "C_out_Q <= out_s3fdp;" << endl;
		}

	}


	PE_S3::~PE_S3() {}

	void PE_S3::emulate(TestCase * tc) {}

	bool PE_S3::has_HSSD() {
		return m_has_HSSD;
	}

	OperatorPtr PE_S3::parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args) {
		int scale_width, fraction_width, bias;
		int nb_bits_ovf;
		int msb_summand;
		int lsb_summand;
		int chunk_size;
		double dspOccupationThreshold;
		bool has_HSSD;
		UserInterface::parseInt(args, "scale_width", &scale_width);
		UserInterface::parseInt(args, "fraction_width", &fraction_width);
		UserInterface::parseInt(args, "bias", &bias);
		UserInterface::parseInt(args, "nb_bits_ovf", &nb_bits_ovf);
		UserInterface::parseInt(args, "msb_summand", &msb_summand);
		UserInterface::parseInt(args, "lsb_summand", &lsb_summand);
		UserInterface::parseInt(args, "chunk_size", &chunk_size);
		UserInterface::parseFloat(args, "dspThreshold", &dspOccupationThreshold);
		UserInterface::parseBoolean(args, "has_HSSD", &has_HSSD);

		return new PE_S3(parentOp, target,
				scale_width,
				fraction_width,
				bias,
				nb_bits_ovf,
				msb_summand,
				lsb_summand,
				chunk_size,
				dspOccupationThreshold,
				has_HSSD);
	}

	void PE_S3::registerFactory() {
		UserInterface::add(
			"PE_S3",  // name
			"The classical processing element of an orthogonal Systolic Array for Mat Mat Mult. Delays both inputs and exactly accumulates them. S3 version with optional HSSD",  // description, string
			"SA",  // category, from the list defined in UserInterface.cpp
			"",  //seeAlso
			"scale_width(int): The scale bitwidth; \
			 fraction_width(int): The fraction bitwidth (appended to 1 implicit bit); \
			 bias(int): The eventual bias of the scale; \
			 nb_bits_ovf(int)=-1: The number of bits prepended before the actual MSB weight of summands. One bit adds one binade to prevent overflow in non cancellation accumulations. These bits affect the size of the adder but not of the shifter, they are not too expansive; \
			 msb_summand(int)=-1: The expected biggest product value weight. If not precised, the tool will go to full precision (aka exact); \
			 lsb_summand(int)=-1: The expected smallest product value weight. If not precised, the tool will go to full precision (aka exact); \
			 chunk_size(int)=-1: An expert can suggest a size for a sub adder chunk for the carry save accum. This should be target specific calculated by the tool; \
			 dspThreshold(real)=0.0: The ratio of dsp over logic for mantissa product; \
			 has_HSSD(bool)=true: indicates the presence of the Half Speed Sink Down chain",
			"",  // More documentation for the HTML pages. If you want to link to your blog, it is here.
			PE_S3::parseArguments
		) ;
	}

}

