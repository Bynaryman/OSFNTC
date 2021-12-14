/*

 */


// #include <iostream>
// #include <fstream>
// #include <iomanip>
// #include <sstream>
// #include <vector>
// #include <math.h>
// #include <gmp.h>
// #include <mpfr.h>
// #include <gmpxx.h>
// #include <cstdlib>

#include <boost/algorithm/string/join.hpp>
#include "utils.hpp"
#include "PositUtils.hpp"
#include "LAICPT2_to_arith.hpp"
#include "ShiftersEtc/Shifters.hpp"

using namespace std;

namespace flopoco{

	LAICPT2_to_arith::LAICPT2_to_arith(
			OperatorPtr parentOp,
			Target* target,
			int nb_bits_ovf,
			int msb_summand,
			int lsb_summand,
			int nb_chunks,
			vector<string> * arithmetic_in,
			vector<string> * arithmetic_out):
				Operator(parentOp, target),
				m_nb_bits_ovf(nb_bits_ovf),
				m_msb_summand(msb_summand),
				m_lsb_summand(lsb_summand),
				m_nb_chunks(nb_chunks),
				m_arithmetic_in(arithmetic_in),
				m_arithmetic_out(arithmetic_out) {

		ostringstream module_name;
		setCopyrightString("Ledoux Louis - BSC / UPC");
		std::string joined = boost::algorithm::join(*arithmetic_out, "_");
		module_name <<"LAICPT2_to_arith_from_"
			  <<(m_msb_summand>=0?"":"m")<<abs(m_msb_summand)<<"_"
			  <<(m_lsb_summand>=0?"":"m")<<abs(m_lsb_summand)<<"_to_";
		module_name << joined;
		setNameWithFreqAndUID(module_name.str());

		int sizeAcc = m_nb_bits_ovf + m_msb_summand - m_lsb_summand + 1;
		m_chunk_size = ceil(double(sizeAcc)/double(m_nb_chunks));
		m_last_chunk_size = ( sizeAcc % m_chunk_size == 0 ? m_chunk_size : sizeAcc % m_chunk_size);

		parse_arithmetic(m_arithmetic_in, m_scale_width_in, m_fraction_width_in, m_bias_in, m_subnormals_in, m_dense_in);
		if (m_arithmetic_out->at(0).compare("exact")  == 0) {
			m_exact_return = true;
			m_is_posit_like = false;
		} else if (arithmetic_out->at(0).compare("same") == 0) {
			m_exact_return = false;
			m_scale_width_out = m_scale_width_in;
			m_fraction_width_out = m_fraction_width_in;
			m_bias_out = m_bias_in;
			m_subnormals_out = m_subnormals_in;
			m_dense_out = m_dense_in;
			if (arithmetic_in->at(0).compare("posit") == 0) {
            	m_is_posit_like = true;
				m_es_out = stoi(arithmetic_in->at(2));
			} else {
				m_is_posit_like = false;
			}
		} else {
			m_exact_return = false;
			parse_arithmetic(arithmetic_out, m_scale_width_out, m_fraction_width_out, m_bias_out, m_subnormals_out, m_dense_out);
			if (arithmetic_out->at(0).compare("posit") == 0) {
				m_es_out = stoi(arithmetic_out->at(2));
            	m_is_posit_like = true;
			} else {
				m_is_posit_like = false;
			}
		}
		if (m_exact_return) {
			m_dense_out = sizeAcc + m_nb_chunks - 1 + 1;
		}

		// inputs and outputs
		addInput("A", sizeAcc);
		if (m_nb_chunks > 1) addInput("C", m_nb_chunks-1);
		addInput("isNaN");
		addOutput("arith_o", m_dense_out);
		vhdl << endl;

        // Eventual Carry Rippling
		if (m_nb_chunks > 1) {

			// build the carry addend
			addFullComment("Reconstruct carry addend from carry bits");
			vhdl << tab << declare("carry_addend", sizeAcc) << " <= " << endl << tab << tab;
				for (int i=m_nb_chunks-1;i>=0; i--){
					vhdl << (i<m_nb_chunks-1? "C" + of(i) + " & ":"");
					vhdl << (i==m_nb_chunks-1?zg(m_last_chunk_size-(i>0?1:0))+(i>0?" & ":""):(i>0?zg(m_chunk_size-1)+" & ":zg(m_chunk_size)));
				}vhdl << ";";
			vhdl << endl;
			vhdl << endl;

			// sign extension
			addFullComment("sign extension");
			vhdl << tab << declare("signA") << " <= A" << of(sizeAcc-1) << ";" << endl;
			vhdl << tab << declare("signC") << " <= carry_addend" << of(sizeAcc-1) << ";" << endl;
			vhdl << tab << declare("ext_acc_addend", sizeAcc+1) << " <= signA & A;" << endl;
			vhdl << tab << declare("ext_carry_addend", sizeAcc+1) << " <= signC & carry_addend;" << endl;
			vhdl << endl;

			// Carry Ripling Adder (RCA)
			addFullComment("RCA");
			vhdl << tab << declare("CIN_rca") << " <= '0';" << endl;
			ostringstream parametric_adder, inputs_adder, outputs_adder;
			parametric_adder << "wIn=" << sizeAcc + 1;
			inputs_adder << "X=>ext_acc_addend,";
			inputs_adder << "Y=>ext_carry_addend,";
			inputs_adder << "Cin=>CIN_rca";
			outputs_adder << "R=>rca_out";
			newInstance("IntAdder", "RCA_inst", parametric_adder.str(), inputs_adder.str(), outputs_adder.str());
			vhdl << tab << declare("rippled_carry", sizeAcc) << " <= rca_out" << range(sizeAcc-1,0) << ";" << endl;
			vhdl << endl;

		} else {
			vhdl << tab << declare("rippled_carry", sizeAcc) << " <= A;" << endl;
			vhdl << endl;
		}

		if (!m_exact_return) {
			addFullComment("Count 0/1 while shifting and sticky computation");
			vhdl << tab << declare("count_bit") << " <= rippled_carry" << of(sizeAcc-1) << ";" << endl;
			ostringstream param_lzoc, inmap_lzoc, outmap_lzoc;
			param_lzoc << "wIn=" << sizeAcc;
			param_lzoc << " wOut=" << m_fraction_width_out + 2;  // catch implicit bit and g[R]s
			param_lzoc << " wCount=" << intlog2(sizeAcc);
			param_lzoc << " countType=-1";
			param_lzoc << " computeSticky=true";
			inmap_lzoc << "I => rippled_carry";
			inmap_lzoc << ",OZb => count_bit";
			outmap_lzoc << "Count => count_lzoc_o";
			outmap_lzoc << ",O => frac_lzoc_o";
			outmap_lzoc << ",Sticky => sticky_lzoc_o";
			newInstance("LZOCShifterSticky", "lzoc_inst", param_lzoc.str(), inmap_lzoc.str(), outmap_lzoc.str());
			vhdl << endl;

			addFullComment("Compute unbiased exponent from msb weigth and lzoc count");
		    vhdl << tab << declare(target->adderDelay(intlog2(sizeAcc)), "unbiased_exp", intlog2(sizeAcc)) << " <= CONV_STD_LOGIC_VECTOR(" << m_msb_summand + m_nb_bits_ovf << "," << intlog2(sizeAcc) << ") - (count_lzoc_o);" << endl;
			if (!m_is_posit_like) {  // if ieee, tfp, bfloat
				int ieee_like_bias = (1<<(m_scale_width_out-1))-1;
				if (intlog2(sizeAcc) < m_scale_width_out) {
					// accumulator does not cover all exponent range
					vhdl << tab << declare("bias", m_scale_width_out) << " <= CONV_STD_LOGIC_VECTOR(" << ieee_like_bias << "," << m_scale_width_out << ");" << endl;
					vhdl << tab << declare(target->adderDelay(m_scale_width_out), "biased_exp", m_scale_width_out) << " <= bias + ("<< rangeAssign(m_scale_width_out-1,intlog2(sizeAcc), "unbiased_exp"+of(intlog2(sizeAcc)-1)) << " & unbiased_exp);"<<endl;
				} else if (intlog2(sizeAcc) == m_scale_width_out) {
					// accumulator covers as much range as exponent
					vhdl << tab << declare("bias", m_scale_width_out) << " <= CONV_STD_LOGIC_VECTOR(" << ieee_like_bias << "," << m_scale_width_out << ");" << endl;
					vhdl << tab << declare(target->adderDelay(m_scale_width_out), "biased_exp", m_scale_width_out) << " <= bias + unbiased_exp;" << endl;
				} else {
					// accumulator covers more range than destination output format
					vhdl << tab << declare("bias", intlog2(sizeAcc)) << " <= CONV_STD_LOGIC_VECTOR(" << ieee_like_bias <<"," << intlog2(sizeAcc) << ");" << endl;
					vhdl << tab << declare(target->adderDelay(intlog2(sizeAcc)), "exp_ext" , intlog2(sizeAcc)) << " <= bias + unbiased_exp;"<<endl;
					vhdl << tab << declare("sign_exp_ext") << " <= exp_ext" << of(intlog2(sizeAcc)-1) << ";" << endl;

					if (intlog2(sizeAcc)-1 == m_scale_width_out) {
						vhdl << tab << declare("exp_ovf") << " <= '0';" << endl;
					} else {
						vhdl << tab << declare("exp_ovf") << " <= '1' when ( exp_ext" << range(intlog2(sizeAcc)-2,m_scale_width_out) << ">" << zg(intlog2(sizeAcc)-1-m_scale_width_out) << " and sign_exp_ext='0') else '0';" << endl;
					}
						if (m_subnormals_out) {
							vhdl << endl;
							addFullComment("Subnormals shifter");
							vhdl << tab << declare("pre_sub_exp_udf") << " <= sign_exp_ext;" << endl;
							vhdl << tab << declare("pad") << " <= not(rippled_carry" << of(sizeAcc-1) << ");" << endl;
							vhdl << tab << declare("shift_amount", intlog2(m_fraction_width_out+2)) << " <= " << zg(intlog2(m_fraction_width_out+2)) << " when pre_sub_exp_udf='1' else CONV_STD_LOGIC_VECTOR(-signed(exp_ext)," << intlog2(m_fraction_width_out+2) << ");" << endl;
							// instantiate a shifter that moves out to the right the implicit bit
							ostringstream param_subnormal_shift, inmap_subnormal_shift, outmap_subnormal_shift;
							param_subnormal_shift << "wIn=" << m_fraction_width_out+2;
							param_subnormal_shift << " maxShift=" << m_fraction_width_out+2;
							param_subnormal_shift << " wOut=" << m_fraction_width_out+2;
							param_subnormal_shift << " dir=" << Shifter::Right;
							param_subnormal_shift << " computeSticky=false";
							param_subnormal_shift << " inputPadBit=true";
							inmap_subnormal_shift << "X=>frac_lzoc_o,S=>shift_amount,padBit=>pad";
							outmap_subnormal_shift << "R=>shifted_mantissa";
							newInstance("Shifter", "rshift_mantissa_inst", param_subnormal_shift.str(), inmap_subnormal_shift.str(), outmap_subnormal_shift.str());
							vhdl << endl;

							vhdl << tab << declare("exp_udf") << " <= '1' when (shift_amount > " << m_fraction_width_out+2 << ") else '0';" << endl;
						}
						else {
							vhdl << tab << declare("exp_udf") << " <= sign_exp_ext;" << endl;

						}
					vhdl << tab << declare("biased_exp", m_scale_width_out) << " <= exp_ext" << range(m_scale_width_out-1,0) << ";" << endl;
				}
				vhdl << endl;

				// cpt1 + 1 the frac
				addFullComment("Convert in sign magnitude");
				if (m_subnormals_out && intlog2(sizeAcc) > m_scale_width_out){
					vhdl << tab << declare(target->logicDelay(m_fraction_width_out+2), "not_frac_lzoc", m_fraction_width_out+2) << " <=  shifted_mantissa" << range(m_fraction_width_out+2+m_fraction_width_out+2-1,m_fraction_width_out+2) << " xor " << rangeAssign(m_fraction_width_out+1, 0, join("rippled_carry(", (sizeAcc-1),")"))<<";"<<endl;  // taking the MSB of the shifter

				} else {
					vhdl << tab << declare(target->logicDelay(m_fraction_width_out+2), "not_frac_lzoc", m_fraction_width_out+2) << " <=  frac_lzoc_o xor " << rangeAssign(m_fraction_width_out+1, 0, join("rippled_carry(", (sizeAcc-1),")"))<<";"<<endl;
				}
				vhdl << tab << declare(target->adderDelay(m_fraction_width_out+2), "unrounded_frac", m_fraction_width_out+2) << " <= \"0\" & not_frac_lzoc" << range(m_fraction_width_out,0) << " + rippled_carry" << of(sizeAcc-1) << ";" << endl;
				vhdl << endl;
				// "G and R should be taken from lzoc adding one size more frac lzoc width"
				addFullComment("G and R should be taken from lzoc adding one size more frac lzoc width");
				// eventual rounding up
				addFullComment("GRS rounding up");
				vhdl << tab << declare("G") << " <= unrounded_frac(1);" << endl;
				vhdl << tab << declare("R") << " <= unrounded_frac(0);" << endl;
				vhdl << tab << declare("S") << " <= sticky_lzoc_o;" << endl;
				vhdl << tab << declare(target->logicDelay(3), "round_up") << " <= G and (R or S);" << endl;
				vhdl << tab << declare(target->adderDelay(m_fraction_width_out+2), "rounded_frac", m_fraction_width_out+2) << " <= unrounded_frac + round_up;" << endl;
				vhdl << tab << declare("post_round_ovf") << " <= rounded_frac" << of(m_fraction_width_out+1) << ";" << endl;
				vhdl << endl;
				addFullComment("post rounding scale handling");
				vhdl << tab << declare(target->adderDelay(m_scale_width_out+1), "post_rounding_exp", m_scale_width_out+1) << " <= (\"0\" & biased_exp) + (rounded_frac" << of(m_fraction_width_out+1) << ");" << endl;

				if (intlog2(sizeAcc) > m_scale_width_out) {
					vhdl << tab << declare(target->logicDelay(4), "nan_out") << " <= post_rounding_exp" << of(m_scale_width_out) << " or isNaN or exp_udf or exp_ovf;" << endl;
				} else {
					vhdl << tab << declare(target->logicDelay(2), "nan_out") << " <= post_rounding_exp" << of(m_scale_width_out) << " or isNaN;" << endl;
				}

				vhdl << declare(0., "is_zero") << " <= count_lzoc_o" << of(intlog2(sizeAcc)-1) << " when rounded_frac=\"" << string(m_fraction_width_out+2,'0') << "\" else '0';" << endl;
				vhdl << tab << declare(target->logicDelay(m_scale_width_out), "final_exp", m_scale_width_out ) << " <= post_rounding_exp" << range(m_scale_width_out-1,0) << " when nan_out = '0' else \"" << string(m_scale_width_out, '1') << "\";" << endl;
				vhdl << tab << "arith_o <= (rippled_carry" << of(sizeAcc-1) << " & final_exp" << range(m_scale_width_out-1,0)<<" & rounded_frac" << range(m_fraction_width_out,1) <<") when is_zero = '0' else \"" << string(m_dense_out ,'0') << "\";" << endl;

			} else {  // if posit
				vhdl << tab << declare(0.0, "fraction", m_fraction_width_out+1) << " <= frac_lzoc_o " << range(m_fraction_width_out, 0) << ";" << endl;
				if (m_es_out > 0) {
				  vhdl << declare(0., "partial_exponent", m_es_out) << "<= unbiased_exp" << range(m_es_out-1, 0) << ";" << endl;

				  vhdl << "with count_bit select " << declare(target->logicDelay(m_es_out), "partial_exponent_us", m_es_out) << " <= " << endl <<
				    tab << "partial_exponent when '0'," << endl <<
				    tab << "not partial_exponent when others;" << endl;
				}

				int wCount = intlog2(m_dense_out);
				if (m_dense_out > 3) {
					//vhdl << declare(.0, "bin_regime", wCount) << "<= unbiased_exp" << range(intlog2(sizeAcc)-2, intlog2(sizeAcc)-2-wCount+1) << ";" << endl;
					vhdl << declare(.0, "bin_regime", wCount) << "<= unbiased_exp" << range(wCount-1,0) << ";" << endl;
				} else {
					vhdl << declare(.0, "bin_regime", wCount) << "<= unbiased_exp" << range(intlog2(sizeAcc)-2, m_es_out) << ";" << endl;
				}
				vhdl << declare(.0, "first_regime", 1, false) << "<= unbiased_exp" << of(intlog2(sizeAcc)-1) << ";" << endl;

				vhdl << "with first_regime select " << declare(target->logicDelay(wCount), "regime", wCount) << " <= "  << endl <<
				  tab << "bin_regime when '0', " << endl <<
				  tab << "not bin_regime when others;" << endl;

				vhdl << declare(target->logicDelay(2), "pad", 1, false) << "<= not(first_regime xor count_bit);" << endl;
			    vhdl << "with pad select " << declare(target->logicDelay(1), "start_regime", 2) << " <= "  << endl <<
				  tab << "\"01\" when '0', " << endl <<
				  tab << "\"10\" when others; " << endl;

				if (m_es_out > 0) {
				  vhdl << declare(0., "in_shift", m_dense_out) << " <= start_regime & partial_exponent_us & fraction;" << endl;
				}
				else {
				   vhdl << declare(0., "in_shift", m_dense_out) << " <= start_regime & fraction;" << endl;
				}

				ostringstream param_shift, inmap_shift, outmap_shift;
				param_shift << "wIn=" << m_dense_out;
				param_shift << " maxShift=" << m_dense_out;
				param_shift << " wOut=" << m_dense_out;
				param_shift << " dir=" << Shifter::Right;
				param_shift << " computeSticky=true";
				param_shift << " inputPadBit=true";
				inmap_shift << "X=>in_shift,S=>regime,padBit=>pad";
				outmap_shift << "R=>extended_posit,Sticky=>pre_sticky";
				newInstance("Shifter", "rshift", param_shift.str(), inmap_shift.str(), outmap_shift.str());

				//rounding up

				vhdl << declare(0.,"truncated_posit", m_dense_out-1) << "<= extended_posit" << range(m_dense_out-1, 1) << ";" << endl;
				vhdl << declare(0., "lsb", 1, false) << " <= extended_posit" << of(1) << ";" << endl;
				vhdl << declare(0., "guard", 1, false) << " <= extended_posit" << of(0) << ";" << endl;
				vhdl << declare(0., "sticky", 1, false) << " <= fraction" << of(0) << " or pre_sticky or sticky_lzoc_o;" << endl;

				vhdl << declare(target->logicDelay(3),"round_bit", 1, false) << "<= guard and (sticky or lsb);" << endl;

				vhdl << declare("is_NAR", 1, false) << "<= isNaN;" << endl;

				vhdl << declare(target->adderDelay(m_dense_out-1), "rounded_reg_exp_frac", m_dense_out-1) << "<= truncated_posit + round_bit;" << endl;
				vhdl << declare(0., "rounded_posit", m_dense_out) << " <= count_bit & rounded_reg_exp_frac;" << endl;
				vhdl << declare(0., "is_zero") << " <= count_lzoc_o" << of(intlog2(sizeAcc)-1) << " when fraction=\"" << string(m_fraction_width_out+1,'0') << "\" else '0';" << endl;
				vhdl << declare(target->logicDelay(1), "rounded_posit_zero", m_dense_out) << "<= rounded_posit when is_zero= '0' else \"" << string(m_dense_out, '0') << "\";" << endl;
				vhdl << "arith_o <= rounded_posit_zero when is_NAR = '0' else \"1" << string(m_dense_out-1, '0') << "\";" << endl;
			}


		}
		else {  // if exact return
			addFullComment("Output Compose");
			vhdl << tab << "arith_o <= isNaN & rippled_carry;" << endl;
		}

	}

	LAICPT2_to_arith::~LAICPT2_to_arith() {
	}

	void LAICPT2_to_arith::emulate(TestCase *tc)
	{
	}

	void LAICPT2_to_arith::buildStandardTestCases(TestCaseList* tcl){
	}

	TestCase* LAICPT2_to_arith::buildRandomTestCase(int i){

		 TestCase *tc;
		tc = new TestCase(this);

		return tc;
	}

	OperatorPtr LAICPT2_to_arith::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {

		int nb_bits_ovf;
		int msb_summand;
		int lsb_summand;
		int nb_chunks;
		std::vector<std::string> arithmetic_in;
		std::vector<std::string> arithmetic_out;
		UserInterface::parseInt(args, "nb_bits_ovf", &nb_bits_ovf);
		UserInterface::parseInt(args, "msb_summand", &msb_summand);
		UserInterface::parseInt(args, "lsb_summand", &lsb_summand);
		UserInterface::parseInt(args, "nb_chunks", &nb_chunks);
		UserInterface::parseColonSeparatedStringList(args, "arithmetic_in", &arithmetic_in);
		UserInterface::parseColonSeparatedStringList(args, "arithmetic_out", &arithmetic_out);
		return new LAICPT2_to_arith(parentOp, target,
				nb_bits_ovf,
				msb_summand,
				lsb_summand,
				nb_chunks,
				&arithmetic_in,
				&arithmetic_out);
	}

	void LAICPT2_to_arith::registerFactory(){
		UserInterface::add("LAICPT2_to_arith", // name
			"Post-normalisation unit for S3FDP",
			"SA",
			"S3FDP", // seeAlso
			"nb_bits_ovf(int): The number of bits prepended before the actual MSB weight of summands. One bit adds one binade to prevent overflow in non cancellation accumulations. These bits affect the size of the adder but not of the shifter, they are not too expansive; \
			 msb_summand(int): The expected biggest product value weight. If not precised, the tool will go to full precision (aka exact); \
			 lsb_summand(int): The expected smallest product value weight. If not precised, the tool will go to full precision (aka exact); \
			 nb_chunks(int): in how many chunks the accumulator is split; \
			arithmetic_in(string): colon-separated list of input arith parameters. Example : \"posit:32\"(new posit, es=2),\"posit:16:1\"(old posit), \"ieee:8:23\", or \"bfloat16\"; \
			arithmetic_out(string): colon-separated list of output arith parameters. Example : \"exact\", \"same\" or specified : \"posit:32\"(new posit, es=2),\"posit:16:1\"(old posit), \"ieee:8:23\", or \"bfloat16\"",
	   	 	"",
			LAICPT2_to_arith::parseArguments
		 ) ;

	}

}
