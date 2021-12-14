/*

  Systolic Array implementaion. Orthogonal N*N for the moment.

  Author: Ledoux Louis

 */

#include "SA/PositUtils.hpp"
#include "SystolicArrayKernel.hpp"

#include <vector>
#include <sstream>

#include "Operator.hpp"

using namespace std;

namespace flopoco{

	SystolicArrayKernel::SystolicArrayKernel(
			OperatorPtr parentOp,
			Target* target,
			int N,
			int M,
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
				m_N(N),
				m_M(M),
				m_scale_width(scale_width),
				m_fraction_width(fraction_width),
				m_bias(bias),
				m_nb_bits_ovf(nb_bits_ovf),
				m_msb_summand(msb_summand),
				m_lsb_summand(lsb_summand),
				m_chunk_size(chunk_size),
				m_dspOccupationThreshold(dspOccupationThreshold),
				m_has_HSSD(has_HSSD) {

		setNoParseNoSchedule();  // we do not Schedule this operator

		// The systolic array is only about connecting Processing elements in a certain topology with local registering
		// The philosophy is to have short and local connections. That is the reason here the HDL is directly writen w/o
		// the help of automated pipeline framework. We use the CLI helpers, so we stay in flopoco.
		// this constructor does:
		// 1) Build with flopoco and schedule one processing element and the underneath operators and add it to this
		// 2) Build by hand the HDL with for generate (flopoco can not instantiate only one shared thing) the SA

		PE_S3* pe = new PE_S3(nullptr, target,
				m_scale_width,
				m_fraction_width,
				m_bias,
				m_nb_bits_ovf,
				m_msb_summand,
				m_lsb_summand,
				m_chunk_size,
				m_dspOccupationThreshold,
				m_has_HSSD);
		vector<Operator*> pe_sub_components = pe->getSubComponentList();
		S3FDP* s3fdp = (S3FDP*)pe_sub_components[0];
		int wLAICPT2 = s3fdp->get_wLAICPT2();
		int wCOutput = s3fdp->get_wC();
		pe->setName("PE_S3");
		pe->schedule();
		pe->applySchedule();
		addSubComponent(pe);

		ostringstream module_name;
		std::string topology = "orthogonal";  // only orthogonal for the moment
		module_name << "SA_kernel_" << topology << "_" <<  m_N << "w" << m_M << "h";
		if(m_has_HSSD) module_name << "_HSSD";
		setNameWithFreqAndUID(module_name.str());
		setCopyrightString("Ledoux Louis - BSC / UPC");

		const int S3_size = m_scale_width + m_fraction_width + 3;

		// IOs declaration
		// One Bus for A rows, One bus for B columns
		addInput("rowsA",m_N*S3_size,true);
		addInput("colsB",m_M*S3_size,true);
		addInput("SOB");  // active high boolean to indicate the begining of a new block (every P clock edges)
		if (m_has_HSSD) addInput("EOB");  // active high boolean to indicate the end of a block (every P clock edges)

		if (m_has_HSSD) {
			addOutput("colsC", m_M*(wLAICPT2+wCOutput+1),true);
		} else {
			addOutput("colsC", m_M*m_N*(wLAICPT2+wCOutput+1),true);
		}
		if (m_has_HSSD) addOutput("EOB_Q_o");

		// put a method of Operator class as virtual to be rewriten in this instance to catch special signal types
		// since this vhdl is not parsed.
		// also inverted the write VHDL output generation to have the types before the signals
		//ostringstream type_2d_val_n_m;
		//type_2d_val_n_m << "array(" << N-1 << " downto 0, " << M-1 << " downto 0) of std_logic";
		//addType("T_2D_n_m", type_2d_val_n_m.str());
		ostringstream type_2d_val_np1_m;
		type_2d_val_np1_m << "array(" << N << " downto 0, " << M-1 << " downto 0) of std_logic_vector(" << S3_size-1 << " downto 0)";
		addType("T_2D_np1_m", type_2d_val_np1_m.str());

		ostringstream type_2d_val_np1_m_logic;
		type_2d_val_np1_m_logic << "array(" << N << " downto 0, " << M-1 << " downto 0) of std_logic";
		addType("T_2D_np1_m_logic", type_2d_val_np1_m_logic.str());

		ostringstream type_2d_val_n_mp1;
		type_2d_val_n_mp1 << "array(" << N-1 << " downto 0, " << M << " downto 0) of std_logic_vector(" << S3_size-1 << " downto 0)";
		addType("T_2D_n_mp1", type_2d_val_n_mp1.str());

		ostringstream type_2d_val_LAICPT2_np1_m;
		type_2d_val_LAICPT2_np1_m << "array(" << N << " downto 0, " << M-1 << " downto 0) of std_logic_vector(" << (wLAICPT2+wCOutput+1-1) << " downto 0)";
		addType("T_2D_LAICPT2_np1_m", type_2d_val_LAICPT2_np1_m.str());

		declare("systolic_wires_rows_2D", 4);
		declare("systolic_wires_cols_2D", 4);
		declare("systolic_sob_2D", 4);
		if (m_has_HSSD) declare("systolic_eob_2D", 4);
		declare("systolic_C_out_2D", 4);
		vhdl << endl;

		addFullComment("Connect bus of B columns to top edges SA PEs");
		vhdl << tab << "cols_in: for JJ in 0 to " << M-1 << " generate" << endl;
		vhdl << tab << tab << "systolic_wires_cols_2D(0,JJ) <= colsB(((JJ+1)*" << S3_size << ")-1 downto (JJ*" << S3_size << "));" << endl;
		vhdl << tab << "end generate;" << endl;
		vhdl << endl;

		addFullComment("Connect bus of A rows to left edges SA PEs");
		vhdl << tab << "rows_in: for II in 0 to " << N-1 << " generate" << endl;
		vhdl << tab << tab << "systolic_wires_rows_2D(II,0) <= rowsA(((II+1)*" << S3_size << ")-1 downto (II*" << S3_size << "));" << endl;
		vhdl << tab << "end generate;" << endl;
		vhdl << endl;

		addFullComment("Connect the Start of Block signals of the TOP PEs");
		vhdl << tab << "systolic_sob_2D(0,0) <= SOB;" << endl;
		vhdl << tab << "sob_1st_row: for JJ in 1 to " << m_M-1 << " generate" << endl;
		vhdl << tab << tab << "systolic_sob_2D(0,JJ) <= systolic_sob_2D(1,JJ-1);" << endl;
		vhdl << tab << "end generate;" << endl;
		vhdl << endl;

		if (m_has_HSSD) {
			addFullComment("Connect the End of Block signals of the TOP PEs");
			vhdl << tab << "systolic_eob_2D(0,0) <= EOB;" << endl;
			vhdl << tab << "eob_1st_row: for JJ in 1 to " << m_M-1 << " generate" << endl;
			vhdl << tab << tab << "systolic_eob_2D(0,JJ) <= systolic_eob_2D(1,JJ-1);" << endl;
			vhdl << tab << "end generate;" << endl;
			vhdl << endl;
			addFullComment("Connect with 0s the input C carry out scheme of TOP PEs");
			vhdl << tab << "C_out_input_1st_row: for JJ in 0 to " << m_M-1 << " generate" << endl;
			vhdl << tab << tab << "systolic_C_out_2D(0,JJ) <= " << zg(wLAICPT2+wCOutput+1,0) << ";" << endl;
			vhdl << tab << "end generate;" << endl;
			vhdl << endl;
		}

		addFullComment("Connect PEs locally together");
		vhdl << tab << "rows: for II in 0 to " << m_N-1 << " generate" << endl;
		vhdl << tab << tab << "cols: for JJ in 0 to " << m_M-1 << " generate" << endl;
		vhdl << tab << tab << tab << "PE_ij: PE_S3" << endl;
		vhdl << tab << tab << tab << tab << "port map ( clk => clk," << endl;
		vhdl << tab << tab << tab << tab << "           rst => rst," << endl;
		vhdl << tab << tab << tab << tab << "           s3_row_i_A => systolic_wires_rows_2D(II,JJ)," << endl;
		vhdl << tab << tab << tab << tab << "           s3_col_j_B => systolic_wires_cols_2D(II,JJ)," << endl;
		vhdl << tab << tab << tab << tab << "           SOB => systolic_sob_2D(II,JJ)," << endl;
		vhdl << tab << tab << tab << tab << "           SOB_Q => systolic_sob_2D(II+1,JJ)," << endl;
		if (m_has_HSSD) vhdl << tab << tab << tab << tab << "           EOB => systolic_eob_2D(II,JJ)," << endl;
		if (m_has_HSSD) vhdl << tab << tab << tab << tab << "           EOB_Q => systolic_eob_2D(II+1,JJ)," << endl;
		if (m_has_HSSD) vhdl << tab << tab << tab << tab << "           C_out => systolic_C_out_2D(II,JJ)," << endl;
		vhdl << tab << tab << tab << tab << "           C_out_Q => systolic_C_out_2D(II+1,JJ)," << endl;
		vhdl << tab << tab << tab << tab << "           s3_row_im1_A => systolic_wires_rows_2D(II,JJ+1)," << endl;
		vhdl << tab << tab << tab << tab << "           s3_col_jm1_B => systolic_wires_cols_2D(II+1,JJ));" << endl;
		vhdl << tab << tab << "end generate;" << endl;
		vhdl << tab << "end generate;" << endl;
		vhdl << endl;

		if (m_has_HSSD) {
			addFullComment("Connect last row output C to output C bus");
			vhdl << tab << "cols_C_out: for JJ in 0 to " << m_M-1 << " generate" << endl;
			vhdl << tab << tab << "colsC(((JJ+1)*" << wLAICPT2+wCOutput+1 << ")-1 downto (JJ*" << wLAICPT2+wCOutput+1 << ")) <= systolic_C_out_2D(" << m_N << ",JJ);" << endl;
			vhdl << tab << "end generate;" << endl;
			vhdl << endl;
		} else {
			addFullComment("Connect PEs C's output to output C bus");
			vhdl << tab << "cols_C_out_i: for II in 1 to " << m_N << " generate" << endl;
			vhdl << tab << tab << "cols_C_out_j: for JJ in 0 to " << m_M-1 << " generate" << endl;
			vhdl << tab << tab << tab << "colsC((((((II-1)*"<< m_M <<")+JJ+1)*" << wLAICPT2+wCOutput+1 << ")-1) downto ((((II-1)*"<< m_M <<")+JJ)*" << wLAICPT2+wCOutput+1 << ")) <= systolic_C_out_2D(II,JJ);" << endl;
			vhdl << tab << tab << "end generate;" << endl;
			vhdl << tab << "end generate;" << endl;
		}

		if (m_has_HSSD) {
			addFullComment("Connect PE(N-1,M-1) EOB_Q to out world for valid data computation");
			vhdl << tab << "EOB_Q_o <= systolic_eob_2D(" << m_N << "," << m_M-1 << ");" << endl;
			vhdl << endl;
		}
	}

	SystolicArrayKernel::~SystolicArrayKernel() {}

	string SystolicArrayKernel::buildVHDLSignalDeclarations() {
		ostringstream o;

		for(unsigned int i=0; i<signalList_.size(); i++) {
			//constant signals don't need a declaration
			//inputs/outputs treated separately
			if((signalList_[i]->type() == Signal::constant) ||
				 (signalList_[i]->type() == Signal::in) || (signalList_[i]->type() == Signal::out))
				continue;

			if(signalList_[i]->getName()=="systolic_wires_rows_2D") {
				o << "signal systolic_wires_rows_2D : T_2D_n_mp1;" << endl;
			} else if(signalList_[i]->getName()=="systolic_wires_cols_2D") {
				o << "signal systolic_wires_cols_2D : T_2D_np1_m;" << endl;
			} else if(signalList_[i]->getName()=="systolic_sob_2D") {
				o << "signal systolic_sob_2D : T_2D_np1_m_logic;" << endl;
			} else if(signalList_[i]->getName()=="systolic_eob_2D") {
				o << "signal systolic_eob_2D : T_2D_np1_m_logic;" << endl;
			} else if(signalList_[i]->getName()=="systolic_C_out_2D") {
				o << "signal systolic_C_out_2D : T_2D_LAICPT2_np1_m;" << endl;
			} else {
				o << signalList_[i]->toVHDLDeclaration() << endl;
			}
		}

		//now the signals from the I/O List which have the cycle>0
		for(unsigned int i=0; i<ioList_.size(); i++) {
			if(ioList_[i]->getLifeSpan()>0){
				o << ioList_[i]->toVHDLDeclaration() << endl;
			}

		}

		return o.str();
	}

	void SystolicArrayKernel::emulate(TestCase * tc) {}

	OperatorPtr SystolicArrayKernel::parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args) {
		int SA_width;
		int SA_height;
		int scale_width, fraction_width, bias;
		int nb_bits_ovf;
		int msb_summand;
		int lsb_summand;
		int chunk_size;
		double dspOccupationThreshold;
		bool has_HSSD;
		UserInterface::parseInt(args, "N", &SA_width);
		UserInterface::parseInt(args, "M", &SA_height);
		UserInterface::parseInt(args, "scale_width", &scale_width);
		UserInterface::parseInt(args, "fraction_width", &fraction_width);
		UserInterface::parseInt(args, "bias", &bias);
		UserInterface::parseInt(args, "nb_bits_ovf", &nb_bits_ovf);
		UserInterface::parseInt(args, "msb_summand", &msb_summand);
		UserInterface::parseInt(args, "lsb_summand", &lsb_summand);
		UserInterface::parseInt(args, "chunk_size", &chunk_size);
		UserInterface::parseFloat(args, "dspThreshold", &dspOccupationThreshold);
		UserInterface::parseBoolean(args, "has_HSSD", &has_HSSD);
		return new SystolicArrayKernel(parentOp, target,
				SA_width,
				SA_height,
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

	void SystolicArrayKernel::registerFactory() {
		UserInterface::add(
			"SystolicArrayKernel",  // name
			"Build the raw orthogonal Systolic Array",  // description, string
			"SA",  // category, from the list defined in UserInterface.cpp
			"",  //seeAlso
			"N(int): The Systolic Array width; \
			 M(int): The Systolic Array height; \
			 scale_width(int): The scale bitwidth; \
			 fraction_width(int): The fraction bitwidth (appended to 1 implicit bit); \
			 bias(int): The eventual bias of the scale; \
			 nb_bits_ovf(int)=-1: The number of bits prepended before the actual MSB weight of summands. One bit adds one binade to prevent overflow in non cancellation accumulations. These bits affect the size of the adder but not of the shifter, they are not too expansive; \
			 msb_summand(int)=-1: The expected biggest product value weight. If not precised, the tool will go to full precision (aka exact); \
			 lsb_summand(int)=-1: The expected smallest product value weight. If not precised, the tool will go to full precision (aka exact); \
			 chunk_size(int)=-1: An expert can suggest a size for a sub adder chunk for the carry save accum. This should be target specific calculated by the tool; \
			 dspThreshold(real)=0.0: The ratio of dsp over logic for mantissa product; \
			 has_HSSD(bool)=true: indicates the presence of the Half Speed Sink Down chain",
			"",  // More documentation for the HTML pages. If you want to link to your blog, it is here.
			SystolicArrayKernel::parseArguments
		) ;
	}

}

