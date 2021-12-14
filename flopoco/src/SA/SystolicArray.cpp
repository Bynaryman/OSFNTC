/*

  Systolic Array implementation. Orthogonal N*M for the moment.

  Author: Ledoux Louis

 */

#include "SA/PositUtils.hpp"
#include "SystolicArray.hpp"

#include <vector>
#include <sstream>

#include "Operator.hpp"
#include "SA/SystolicArrayKernel.hpp"
#include "SA/IEEE_to_S3.hpp"
#include "SA/Posit_to_S3.hpp"
#include "SA/LAICPT2_to_arith.hpp"

using namespace std;

namespace flopoco{

	SystolicArray::SystolicArray(
			OperatorPtr parentOp,
			Target* target,
			int N,
			int M,
			vector<string> * arithmetic_in,
			vector<string> * arithmetic_out,
			int nb_bits_ovf,
			int msb_summand,
			int lsb_summand,
			int chunk_size,
			double dspOccupationThreshold,
			bool has_HSSD):
				Operator(parentOp, target),
				m_N(N),
				m_M(M),
				m_nb_bits_ovf(nb_bits_ovf),
				m_msb_summand(msb_summand),
				m_lsb_summand(lsb_summand),
				m_chunk_size(chunk_size),
				m_dspOccupationThreshold(dspOccupationThreshold),
				m_has_HSSD(has_HSSD) {

		// 1.  We detect the incoming arithmetic and do some checkings
		// 2.  We part select the inputs buses to get right rows and cols
		// 3.a We delay the incoming data in meemory/dense format depending on row/col index
		// 3.b We delay the new_matrix_block signal with Arith_to_S3 delay for tagergeted fpga/freq
		// 4.  We denormalize them into fpga format (S3)
		// 5.  We connect this to SystolicArrayKernel
		// 6.  We connect SystolicArrayKernel output to Normalizing units (Kulisch2S3)
		// 7.  We perform S3_to_Arith
		// 8.  We delay the arithmetic in memory/dense format depending on col index
		// 9.  We instantiate a 0-delay FIFO of Systolic Array's Execuction time

		parse_arithmetic(arithmetic_in, m_scale_width_in, m_fraction_width_in, m_bias_in, m_subnormals_in, m_dense_in);
		if (arithmetic_out->at(0).compare("exact") == 0) {
			m_exact_return = true;
		} else if (arithmetic_out->at(0).compare("same") == 0) {
			m_scale_width_out = m_scale_width_in;
			m_fraction_width_out = m_fraction_width_in;
			m_bias_out = m_bias_in;
			m_subnormals_out = m_subnormals_in;
			m_dense_out = m_dense_in;
		} else {
			parse_arithmetic(arithmetic_out, m_scale_width_out, m_fraction_width_out, m_bias_out, m_subnormals_out, m_dense_out);
		}
		m_s3_in = m_scale_width_in + m_fraction_width_in + 3;

		ostringstream module_name;
		std::string topology = "orthogonal";  // only orthogonal for the moment
		module_name << "SA_" << topology << "_" <<  N << "w" << M << "h_";
		std::string joined = boost::algorithm::join(*arithmetic_in, "_");
		module_name << joined;
		if (m_has_HSSD) module_name << "_HSSD";
		setNameWithFreqAndUID(module_name.str());
		setCopyrightString("Ledoux Louis - BSC / UPC");

		// IOs declaration
		// One Bus for A rows, One bus for B columns
		addInput("rowsA",m_N*m_dense_in,true);
		addInput("colsB",m_M*m_dense_in,true);
		addInput("SOB");
		addInput("EOB");

		PE_S3* pe = new PE_S3(nullptr, target,
				m_scale_width_in,
				m_fraction_width_in,
				m_bias_in,
				m_nb_bits_ovf,
				m_msb_summand,
				m_lsb_summand,
				m_chunk_size,
				m_dspOccupationThreshold,
				m_has_HSSD);
		vector<Operator*> pe_sub_components = pe->getSubComponentList();
		S3FDP* s3fdp = (S3FDP*)pe_sub_components[0];
		s3fdp->schedule();
		s3fdp->applySchedule();
		int wLAICPT2 = s3fdp->get_wLAICPT2();
		int wCOutput = s3fdp->get_wC();
		int s3fdp_ppDepth = s3fdp->getPipelineDepth();
		delete pe;
		delete s3fdp;
		if (m_exact_return) {
			m_dense_out = wLAICPT2 + wCOutput + 1;
		}

		addOutput("colsC", m_M*m_dense_out, true);
		addOutput("EOB_Q_o");  // signal that will be connected to top my_sv_wrapper.sv taht indicated sort of valid

		addFullComment("Delay depending on row index incoming dense arithmetic");
		Signal * s;
		ostringstream delayed_arith_in;
		for (int i = 0 ; i < m_N ; i++ ) {
			vhdl << tab << declare(join("arith_in_row_", i), m_dense_in) << " <= rowsA(" <<(((i+1)*m_dense_in)-1)<< " downto " <<(i*m_dense_in)<< ");" << endl;
			s = getSignalByName(join("arith_in_row_",i));
			s->setResetType(Signal::noReset);
			delayed_arith_in << "arith_in_row_" << i << "_q" << i;
			vhdl << tab << declare(delayed_arith_in.str(),m_dense_in) << " <= arith_in_row_" << i << "^" << i <<";" << endl;
			getSignalByName(delayed_arith_in.str())->setHasBeenScheduled(true);
			delayed_arith_in.str("");
		}
		vhdl << endl;

		addFullComment("Delay depending on col index incoming dense arithmetic");
		for (int j = 0 ; j < m_M ; j++ ) {
			vhdl << tab << declare(join("arith_in_col_", j), m_dense_in) << " <= colsB(" <<(((j+1)*m_dense_in)-1)<< " downto " <<(j*m_dense_in)<< ");" << endl;
			s = getSignalByName(join("arith_in_col_",j));
			s->setResetType(Signal::noReset);
			delayed_arith_in << "arith_in_col_" << j << "_q" << j;
			vhdl << tab << declare(delayed_arith_in.str(),m_dense_in) << " <= arith_in_col_" << j << "^" << j <<";" << endl;
			getSignalByName(delayed_arith_in.str())->setHasBeenScheduled(true);
			delayed_arith_in.str("");
		}
		vhdl << endl;

		Operator* a2s3;
		if (arithmetic_in->at(0).compare("posit") == 0) {
			int es = 0;
			if (arithmetic_in->size() >= 3) {es = stoi(arithmetic_in->at(2));}else {es = 2;}
			a2s3 = new Posit_to_S3(target, nullptr, m_dense_in, es);
		} else {
			a2s3 = new IEEE_to_S3(target, nullptr, m_scale_width_in, m_fraction_width_in);
		}
		a2s3->setName("Arith_to_S3");
		a2s3->schedule();
		a2s3->applySchedule();
		int a2s3_ppDepth = a2s3->getPipelineDepth();
		addSubComponent(a2s3);

		// TODO(lledoux): update constructor params
		std::string joined_tmp = boost::algorithm::join(*arithmetic_out, "_");
		std::cout << wCOutput << " " << joined_tmp  << endl;
		LAICPT2_to_arith* laicpt2_to_arith_dummy = new LAICPT2_to_arith(nullptr, target,
			m_nb_bits_ovf,
			m_msb_summand,
			m_lsb_summand,
			wCOutput + 1,  // LAICPT2 expects a number of chunk so we add 1
			arithmetic_in,
			arithmetic_out);
		laicpt2_to_arith_dummy->setName("l2a");
		laicpt2_to_arith_dummy->schedule();
		laicpt2_to_arith_dummy->applySchedule();
		addSubComponent(laicpt2_to_arith_dummy);

		if (m_has_HSSD) {
			declare("colsC_LAICPT2", m_M*(wLAICPT2+wCOutput+1), true);
		} else {
			declare("colsC_LAICPT2", m_N*m_M*(wLAICPT2+wCOutput+1), true);
		}

		addFullComment("Delay SOB/EOB with Arith_to_S3 delay to feed SAK");
		vhdl << tab << declare(.0, "SOB_select") << " <= SOB;" << endl;
		addRegisteredSignalCopy(join("SOB_q",a2s3_ppDepth), "SOB_select", Signal::noReset, a2s3_ppDepth);
		vhdl << tab << declare(.0, "EOB_select") << " <= EOB;" << endl;
		addRegisteredSignalCopy(join("EOB_q",a2s3_ppDepth), "EOB_select", Signal::noReset, a2s3_ppDepth);
		vhdl << endl;


		addFullComment("Delay outgoing arithmetic depending on col index");
		declare("LAICPT2_to_arith",m_M*m_dense_out,true);
		ostringstream delayed_arith_out;
		for (int j = 0 ; j < m_M ; j++ ) {
			vhdl << tab << declare(join("arith_out_col_out_", j), m_dense_out) << " <= LAICPT2_to_arith(" <<(((j+1)*m_dense_out)-1)<< " downto " <<(j*m_dense_out)<< ");" << endl;
			s = getSignalByName(join("arith_out_col_out_",j));
			s->setResetType(Signal::noReset);
			delayed_arith_out << "arith_out_col_out_" << j << "_q" << m_M-1-j;
			vhdl << tab << declare(delayed_arith_out.str(),m_dense_out) << " <= arith_out_col_out_" << j << "^" << m_M-1-j <<";" << endl;
			getSignalByName(delayed_arith_out.str())->setHasBeenScheduled(true);
			delayed_arith_out.str("");
		}
		vhdl << endl;

		if (!m_has_HSSD) {
			addFullComment("Generate Shift-register for EOB time-alignment");
			addRegisteredSignalCopy("EOB_aligned", "EOB_select", Signal::noReset, s3fdp_ppDepth+m_N+m_M-1);
			vhdl << endl;
		}

		// schedule();
		// applySchedule();
		setNoParseNoSchedule();

		if (!m_has_HSSD) {
			addFullComment("Generate One-Hot Muxes when no HSSD");
			vhdl << tab << declare("EOB_select_d0") << " <= EOB_select;" << endl;
			declare("colsC_LAICPT2_muxed", m_M*(wLAICPT2+wCOutput+1), true);
			for (int j=0 ; j < m_M ; j++) {
				vhdl << tab << "colsC_LAICPT2_muxed" << range((j+1)*(wLAICPT2+wCOutput+1)-1, j*(wLAICPT2+wCOutput+1)) << " <= " << endl;
				for (int i=0 ; i < m_N ; i++) {
					vhdl << tab << tab << "colsC_LAICPT2" << range(((j*m_N)+i+1)*(wLAICPT2+wCOutput+1)-1,((j*m_N)+i)*(wLAICPT2+wCOutput+1)) << " when (EOB_select_d" << s3fdp_ppDepth-1+i+j << "='1') else " << endl;
					if (i==m_N-1) {vhdl << tab << tab << "\"" << string((wLAICPT2+wCOutput+1), '0') << "\";" << endl;}
				}
			}
			vhdl << endl;
		}

		addFullComment("Generate Arith_to_S3 for rows and connect them");
		declare("rows_i_arith", m_N*m_dense_in);
		declare("rows_i_s3", m_N*m_s3_in);
		ostringstream type_array_N_dense, type_array_N_s3;
		type_array_N_dense << "array(" << m_N-1 << " downto 0) of std_logic_vector(" << m_dense_in-1 << " downto 0)";
		type_array_N_s3 << "array(" << m_N-1 << " downto 0) of std_logic_vector(" << m_s3_in-1 << " downto 0)";
		addType("array_N_dense", type_array_N_dense.str());
		addType("array_N_s3", type_array_N_s3.str());
		for (int i = 0 ; i < m_N ; i++) {
			vhdl << tab << "rows_i_arith("<<i<<") <= " << "arith_in_row_" << i << "_q" << i << ";" << endl;
		}
		vhdl << tab << "rows_a2s3: for II in 0 to " << m_N-1 << " generate" << endl;
		vhdl << tab << tab << "a2s3_i: Arith_to_S3" << endl;
		vhdl << tab << tab << tab << "port map ( clk => clk," << endl;
		vhdl << tab << tab << tab << "           arith_i => rows_i_arith(II)," << endl;
		vhdl << tab << tab << tab << "           s3_o => rows_i_s3(((II+1)*"<< m_s3_in <<")-1 downto II*"<< m_s3_in<<"));" << endl;
		vhdl << tab << "end generate;" << endl;
		vhdl << endl;

		addFullComment("Generate Arith_to_S3 for cols and connect them");
		declare("cols_j_arith", m_M*m_dense_in);
		declare("cols_j_s3", m_M*m_s3_in);
		ostringstream type_array_M_dense, type_array_M_s3;
		type_array_M_dense << "array(" << m_M-1 << " downto 0) of std_logic_vector(" << m_dense_in-1 << " downto 0)";
		type_array_M_s3 << "array(" << m_M-1 << " downto 0) of std_logic_vector(" << m_s3_in-1 << " downto 0)";
		addType("array_M_dense", type_array_M_dense.str());
		addType("array_M_s3", type_array_M_s3.str());
		for (int j = 0 ; j < m_M ; j++) {
			vhdl << tab << "cols_j_arith("<<j<<") <= " << "arith_in_col_" << j << "_q" << j << ";" << endl;
		}

		vhdl << tab << "cols_a2s3: for JJ in 0 to " << m_M-1 << " generate" << endl;
		vhdl << tab << tab << "a2s3_j: Arith_to_S3" << endl;
		vhdl << tab << tab << tab << "port map ( clk => clk," << endl;
		vhdl << tab << tab << tab << "           arith_i => cols_j_arith(JJ)," << endl;
		vhdl << tab << tab << tab << "           s3_o => cols_j_s3(((JJ+1)*"<< m_s3_in <<")-1 downto JJ*"<< m_s3_in<<"));" << endl;
		vhdl << tab << "end generate;" << endl;
		vhdl << endl;

		addFullComment("Instantiate the Systolic Array Kernel");
		SystolicArrayKernel* sak = new SystolicArrayKernel(nullptr, target,
			m_N,
			m_M,
			m_scale_width_in,
			m_fraction_width_in,
			m_bias_in,
			m_nb_bits_ovf,
			m_msb_summand,
			m_lsb_summand,
			m_chunk_size,
			m_dspOccupationThreshold,
			m_has_HSSD);

		sak->setName("SystolicArrayKernel");
		addSubComponent(sak);
		vhdl << tab << "sak: SystolicArrayKernel" << endl;
		vhdl << tab << tab << "port map ( clk => clk," << endl;
		vhdl << tab << tab << "           rst => rst," << endl;
		vhdl << tab << tab << "           rowsA => rows_i_s3," << endl;
		vhdl << tab << tab << "           colsB => cols_j_s3," << endl;
		vhdl << tab << tab << "           SOB => SOB_q" << a2s3_ppDepth << "," << endl;
		if (m_has_HSSD) vhdl << tab << tab << "           EOB => EOB_q" << a2s3_ppDepth << "," << endl;
		if (m_has_HSSD) vhdl << tab << tab << "           EOB_Q_o => EOB_Q_o," << endl;
		vhdl << tab << tab << "           colsC => colsC_LAICPT2 );" << endl;
		vhdl << endl;

		addFullComment("Generate LAICPT2_to_arith");
		vhdl << tab << "cols_l2a: for JJ in 0 to " << m_M-1 << " generate" << endl;
		vhdl << tab << tab << "l2a_i: l2a" << endl;
		vhdl << tab << tab << tab << "port map ( clk => clk," << endl;
		if (m_has_HSSD) {
			vhdl << tab << tab << tab << "           A => colsC_LAICPT2(((JJ+1)*" << wLAICPT2+wCOutput+1 <<")-1-1-"<<wCOutput <<" downto JJ*" << wLAICPT2+wCOutput+1 << ")," << endl;
			if (wCOutput > 0) vhdl << tab << tab << tab << "           C => colsC_LAICPT2(((JJ+1)*" << wLAICPT2+wCOutput+1 <<")-1-1 downto JJ*" << wLAICPT2+wCOutput+1 << "+"<<wLAICPT2<<")," << endl;
			vhdl << tab << tab << tab << "           isNaN => colsC_LAICPT2(((JJ+1)*" << wLAICPT2+wCOutput+1 <<")- 1)," << endl;
		}
		else {
			vhdl << tab << tab << tab << "           A => colsC_LAICPT2_muxed(((JJ+1)*" << wLAICPT2+wCOutput+1 <<")-1-1-"<<wCOutput <<" downto JJ*" << wLAICPT2+wCOutput+1 << ")," << endl;
			if (wCOutput > 0) vhdl << tab << tab << tab << "           C => colsC_LAICPT2_muxed(((JJ+1)*" << wLAICPT2+wCOutput+1 <<")-1-1 downto JJ*" << wLAICPT2+wCOutput+1 << "+"<<wLAICPT2<<")," << endl;
			vhdl << tab << tab << tab << "           isNaN => colsC_LAICPT2_muxed(((JJ+1)*" << wLAICPT2+wCOutput+1 <<")- 1)," << endl;
		}
		vhdl << tab << tab << tab << "           arith_o => LAICPT2_to_arith(((JJ+1)*"<< m_dense_out <<")-1 downto JJ*"<< m_dense_out <<"));" << endl;
		vhdl << tab << "end generate;" << endl;
		vhdl << endl;

		addFullComment("Connect outgoing delayed dense arith words to colsC output bus");
		for( int j=0 ; j < m_M ; j++) {
			vhdl << tab << "colsC(" <<(((j+1)*m_dense_out)-1)<< " downto " <<(j*m_dense_out)<< ") <= arith_out_col_out_" << j << "_q" << m_M-1-j << ";" << endl;
		}
		vhdl << endl;

		if (!m_has_HSSD) {
			vhdl << tab << "EOB_Q_o <= EOB_select_d" << s3fdp_ppDepth + m_M + m_N - 1 << ";" << endl;
		}
	}

	SystolicArray::~SystolicArray() {}

	string SystolicArray::buildVHDLSignalDeclarations() {
		ostringstream o;

		for(unsigned int i=0; i<signalList_.size(); i++) {
			//constant signals don't need a declaration
			//inputs/outputs treated separately
			if((signalList_[i]->type() == Signal::constant) ||
				 (signalList_[i]->type() == Signal::in) || (signalList_[i]->type() == Signal::out))
				continue;

			if(signalList_[i]->getName()=="rows_i_arith") {
				o << "signal rows_i_arith : array_N_dense;" << endl;
			} else if(signalList_[i]->getName()=="cols_j_arith") {
				o << "signal cols_j_arith : array_M_dense;" << endl;
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

	TestCase* SystolicArray::buildRandomTestCase(int i) {
		TestCase *tc;
		tc = new TestCase(this);
		// TODO (lledoux) : broken but w/e we testing with full snap integration
		tc->addInput("new_matrix_block", mpz_class(0));
		tc->addInput("colsB", mpz_class(0));
		tc->addInput("rowsA", mpz_class(0));
		emulate(tc);
		return tc;
	}

	void SystolicArray::buildStandardTestCases(TestCaseList* tcl) {
		//TestCase *tc;

		// TODO (lledoux) : broken but w/e we testing with full snap integration

		// 1 : A full 1 by B identity
		// for (int i = 0 ; i < m_N ; i++) {
		// 	tc = new TestCase(this);
		// 	mpz_class row_bus(0);
		// 	mpz_class col_bus(0);
		// 	row_bus = (0b01000000) << (i*m_n);
		// 	col_bus = (0b01000000) << (i*m_n);
		// 	//for (int j = 0 ; j < m_M ; j++) {
		// 	//}
		// 	tc->addInput("rowsA", row_bus);
		// 	tc->addInput("colsB", col_bus);
		// 	tc->addInput("new_matrix_block", mpz_class(i==0));
		// 	tcl->add(tc);
		// }
	}

	void SystolicArray::emulate(TestCase * tc) {}

	OperatorPtr SystolicArray::parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args) {

		int SA_width;
		int SA_height;
		std::vector<std::string> arithmetic_in;
		std::vector<std::string> arithmetic_out;
		int nb_bits_ovf;
		int msb_summand;
		int lsb_summand;
		int chunk_size;
		double dspOccupationThreshold;
		bool has_HSSD;
		UserInterface::parseInt(args, "N", &SA_width);
		UserInterface::parseInt(args, "M", &SA_height);
		UserInterface::parseColonSeparatedStringList(args, "arithmetic_in", &arithmetic_in);
		UserInterface::parseColonSeparatedStringList(args, "arithmetic_out", &arithmetic_out);
		UserInterface::parseInt(args, "nb_bits_ovf", &nb_bits_ovf);
		UserInterface::parseInt(args, "msb_summand", &msb_summand);
		UserInterface::parseInt(args, "lsb_summand", &lsb_summand);
		UserInterface::parseInt(args, "chunk_size", &chunk_size);
		UserInterface::parseFloat(args, "dspThreshold", &dspOccupationThreshold);
		UserInterface::parseBoolean(args, "has_HSSD", &has_HSSD);
		return new SystolicArray(parentOp, target,
				SA_width,
				SA_height,
				&arithmetic_in,
				&arithmetic_out,
				nb_bits_ovf,
				msb_summand,
				lsb_summand,
				chunk_size,
				dspOccupationThreshold,
				has_HSSD);
	}

	void SystolicArray::registerFactory() {
		UserInterface::add(
			"SystolicArray",  // name
			"Build the raw orthogonal Systolic Array with data alignment and denormalization of I/Os",  // description, string
			"SA",  // category, from the list defined in UserInterface.cpp
			"",  //seeAlso
			"N(int): The Systolic Array width; \
			 M(int): The Systolic Array height; \
			 arithmetic_in(string): colon-separated list of input arith parameters. Example : \"posit:32\"(new posit, es=2),\"posit:16:1\"(old posit), \"ieee:8:23\", or \"bfloat16\"; \
			 nb_bits_ovf(int)=-1: The number of bits prepended before the actual MSB weight of summands. One bit adds one binade to prevent overflow in non cancellation accumulations. These bits affect the size of the adder but not of the shifter, they are not too expansive; \
			 msb_summand(int)=-1: The expected biggest product value weight. If not precised, the tool will go to full precision (aka exact); \
			 lsb_summand(int)=-1: The expected smallest product value weight. If not precised, the tool will go to full precision (aka exact); \
			 chunk_size(int)=-1: An expert can suggest a size for a sub adder chunk for the carry save accum. This should be target specific calculated by the tool; \
			 arithmetic_out(string): colon-separated list of output arith parameters. Example : \"exact\", \"same\" or specified : \"posit:32\"(new posit, es=2),\"posit:16:1\"(old posit), \"ieee:8:23\", or \"bfloat16\"; \
			 dspThreshold(real)=0.0: The ratio of dsp over logic for mantissa product; \
			 has_HSSD(bool)=true: indicates the presence of the Half Speed Sink Down chain",
			"",  // More documentation for the HTML pages. If you want to link to your blog, it is here.
			SystolicArray::parseArguments
		) ;
	}

}

