/*
  S3FDP(<Sign,Significand,Scale> Fused Dot Product) for FloPoCo

  Author: Ledoux Louis

 */

#include "SA/PositUtils.hpp"
#include "S3FDP.hpp"

#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <cmath>  // pow,log2

#include "Operator.hpp"
#include "IntMult/IntMultiplier.hpp"
#include "IntAddSubCmp/IntAdder.hpp"
#include "ShiftersEtc/Shifters.hpp"
using namespace std;

namespace flopoco{

	S3FDP::S3FDP(
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

		setHasDelay1Feedbacks();

		ostringstream module_name;
		module_name << "S3FDP_" << scale_width << "_" << fraction_width;
		if (m_has_HSSD) module_name << "_HSDD";
		setNameWithFreqAndUID(module_name.str());
		setCopyrightString("Ledoux Louis - BSC / UPC");
		m_reset_type = Signal::asyncReset;  // {noReset | asyncReset | syncReset}

		// Set default values and check inputs
		// test bench purpose
		m_ftz_clock_positions.push_back(0);
		m_ftz_clock_positions.push_back(30);

		if (m_nb_bits_ovf == -1) {
			REPORT(DETAILED, "bits ovf not user-given, going for " <<m_scale_width+m_fraction_width-1<<"b.")
			m_nb_bits_ovf=m_scale_width+m_fraction_width-1;  // arbitrary size that increases proportionally with the "problem" size
		} else {
			REPORT(DETAILED, "bits ovf: " << m_nb_bits_ovf);
		}


		// Compute some internal parameters
		//int fraction_width, scale_size; get_pd_components_width(m_n, m_es, scale_size, fraction_width);
		//const int pd_size_normal = get_pd_size(m_n, m_es, POSIT_MEMORY);
		//const int useed = (1<< (1<<m_es));  // 2^2^es
		//m_max_absolute_dynamic_range = (m_n-2)*ceil(log2(useed));  // (2^2^es)^(n-2) ~ maxpos : in log domain
		//int msb_summand = 2*m_max_absolute_dynamic_range;  // maxpos^2.
		//m_MSB_summand = msb_summand;  // class atribute to be used in emulate and so on
		// might be useless
		//const int msb_q = m_msb_summand + m_nb_bits_ovf;

		if (m_msb_summand == -1) {
			m_msb_summand = ((int)(pow(m_scale_width,2))-1)<<1;
		}
		if (m_lsb_summand == -1) {
			m_lsb_summand = -(((int)(pow(m_scale_width,2))-1)<<1);
		}

		m_wLAICPT2 = m_msb_summand + m_nb_bits_ovf - m_lsb_summand + 1;  // add one bit for the sign

		if (m_chunk_size == -1) {
  			// TODO(lledoux): the VUP target yields a 412bits adder for 400MHz which is a lot
			// TODO(2)(lledoux): i could create a suggestAccChunk method that takes into account the looped fanout(ed) routing
			target->suggestSubaddSize(m_chunk_size, m_wLAICPT2);
		}

		m_nb_chunk = ceil(double(m_wLAICPT2)/double(m_chunk_size));
		m_last_chunk_size = ( m_wLAICPT2 % m_chunk_size == 0 ? m_chunk_size  : m_wLAICPT2 % m_chunk_size);

		std::ostringstream report_parameters;
		report_parameters << "this operator has parameters:	scale_width=" << m_scale_width <<
				" / fraction_width=" << m_fraction_width <<
				" / bias=" << m_bias <<
				" / nb_bits_ovf=" << m_nb_bits_ovf <<
				" / msb_summand=" << m_msb_summand <<
				" / lsb_summand=" << m_lsb_summand <<
				" / wLAICPT2=" << m_wLAICPT2 <<
				" / nb_chunk=" << m_nb_chunk <<
				" / chunk_size=" << m_chunk_size <<
				" / last_chunk_size=" << m_last_chunk_size <<
				" / has_HSSD=" << m_has_HSSD;
		REPORT(DETAILED, report_parameters.str());

		const int S3_size = scale_width + fraction_width + 3;
		// IOs declaration
		addInput("S3_x", S3_size);
		addInput("S3_y", S3_size);
		addInput("FTZ");
		// Note on FTZ:
		// active high boolean to put the accumulator registers to 0. Sync with incoming data
		// if val !=0 and ftz the acc will be equal to val
		if (m_has_HSSD) addInput("EOB");
		// Note on EOB:
		// stands for End Of Block, we input an active high boolean indicating the end of an accumulation in order to
		// propagates it with the inner delay of this operator

		addOutput("A", m_wLAICPT2);  // Partial accumulation
		if (m_nb_chunk > 1) {
			// addOutput("C", m_wLAICPT2);  // Carry to ripple with intermediate low-entropy 0s
			// TODO(lledoux): only carry the carry bits and not full a LAICPT2 full of zeroes
			addOutput("C", m_nb_chunk-1);
		}
		if (m_has_HSSD) addOutput("EOB_Q");
		addOutput("isNaN");

		// sign product processing
		addFullComment("sign product processing");
		vhdl << tab << declare(.0, "sign_X", 1, false, Signal::wire) << " <= S3_x" << of(S3_size-2) << ";" << endl;
		vhdl << tab << declare(.0, "sign_Y", 1, false, Signal::wire) << " <= S3_y" << of(S3_size-2) << ";" << endl;
		vhdl << tab << declare(target->logicDelay(2), "sign_M", 1, false, Signal::wire) << " <= sign_X xor sign_Y;" << endl;
		vhdl << endl;

		// NaN product processing
		addFullComment("NaN product processing");
		vhdl << tab << declare(.0, "isNaN_X", 1, false, Signal::wire) << " <= S3_x" << of(S3_size-1) << ";" << endl;
		vhdl << tab << declare(.0, "isNaN_Y", 1, false, Signal::wire) << " <= S3_y" << of(S3_size-1) << ";" << endl;
		vhdl << tab << declare(target->logicDelay(2), "isNaN_M", 1, false, Signal::wire) << " <= isNaN_X or isNaN_Y;" << endl;
		vhdl << endl;

		// significand product processing
		// All the decode units should give implicit and fraction set to 0
		// not checking "overflow" for scale re-adjustment since it is fused with accumulation process
		addFullComment("significand processing");
		const int significand_product_width = 2*(fraction_width+1);
		vhdl << tab << declare(.0, "significand_X", 1 + fraction_width, true, Signal::wire) << " <= S3_x" << range(S3_size-3,scale_width) << ";" << endl;
		vhdl << tab << declare(.0, "significand_Y", 1 + fraction_width, true, Signal::wire) << " <= S3_y" << range(S3_size-3,scale_width) << ";" << endl;
		ostringstream parametric_mult, inputs_mult, outputs_mult;
		parametric_mult << "wX=" << fraction_width + 1;
		parametric_mult << " wY=" << fraction_width + 1;
		parametric_mult << " wOut=" << significand_product_width;
		parametric_mult << " dspThreshold=" << m_dspOccupationThreshold;
		inputs_mult << "X=>significand_X,";
		inputs_mult << "Y=>significand_Y";
		outputs_mult << "R=>significand_product";
		newInstance("IntMultiplier", "significand_product_inst", parametric_mult.str(), inputs_mult.str(), outputs_mult.str());
		vhdl << endl;

		// scale processing
		const int scale_product_width = scale_width+1;
		addFullComment("scale processing");
		vhdl << tab << declare(.0, "scale_X_biased", scale_width, true, Signal::wire) << " <= S3_x" << range(scale_width-1,0) << ";" << endl;
		vhdl << tab << declare(.0, "scale_Y_biased", scale_width, true, Signal::wire) << " <= S3_y" << range(scale_width-1,0) << ";" << endl;
		vhdl << tab << declare(getTarget()->adderDelay(scale_product_width), "scale_product_twice_biased", scale_product_width) << " <= (\"0\" & scale_X_biased) + (\"0\" & scale_Y_biased);" << endl;
		vhdl << endl;

		// Start cpt2 process before shifting part
		// Cpt2 = cpt1 + 1
		// we can do the cpt1 before the shift as it will result in a smaller xor as long as the shifter can right pad 0s or 1s (sign bit)
		// Cpt1 here:
		addFullComment("pre-shift xoring (cpt1)");
		vhdl << tab << declare(target->logicDelay(significand_product_width), "significand_product_cpt1", significand_product_width, true) << " <= significand_product when sign_M='0' else not(significand_product);"<< endl;
		vhdl << endl;


		// left shifter
		const int allowed_max_shift = -m_lsb_summand + m_msb_summand;  // there is a +1 -1 hidden
		//const int max_shift = (1 << scale_product_width)-1;
		const int max_shift = (1 << scale_product_width)-1;
		// if (max_shift > allowed_max_shift) {
    	// 		throw std::string("Sorry, but an accumulator not \"left-large\" enough for some scales input does not exist yet.");
    	// 	}
		const int shifted_product_size = max_shift + significand_product_width;
		const int offset = (2*m_bias + m_lsb_summand - 1);  // 1 comes from the fact that the product msb is 2^1 and not 2^0
		addFullComment("significand product shifting");
		//vhdl << tab << declare(target->adderDelay(scale_product_width), "shift_value", scale_product_width) << " <= scale_product_twice_biased - CONV_STD_LOGIC_VECTOR(" << offset << ",\"" << scale_product_width << "\");" << endl;
		//vhdl << tab << declare(target->adderDelay(scale_product_width), "shift_value", scale_product_width) << " <= scale_product_twice_biased - " << offset << ";" << endl;
		vhdl << tab << declare(target->adderDelay(scale_product_width), "shift_value", scale_product_width) << " <= (scale_product_twice_biased) - (" << offset << ");" << endl;

		// TODO(lledoux): check the size of shift_value : scale_product_width or intlog2(max_shift)

		ostringstream parametric_shifter, inputs_shifter, outputs_shifter;
		parametric_shifter << "wIn=" << significand_product_width;
		parametric_shifter << " maxShift=" << max_shift;
		parametric_shifter << " dir=" << Shifter::Left;
		parametric_shifter << " wOut=" << shifted_product_size;
		parametric_shifter << " computeSticky=False";
		parametric_shifter << " inputPadBit=True";
		inputs_shifter << "X=>significand_product_cpt1,";
		inputs_shifter << "padBit=>sign_M,";
		inputs_shifter << "S=>shift_value";
		outputs_shifter << "R=>shifted_significand";
		newInstance("Shifter", "significand_product_shifter_inst", parametric_shifter.str(), inputs_shifter.str(), outputs_shifter.str());
		vhdl << endl;

		// add 0 when shiftval is negative
		addFullComment("detect too low scale for this specific scratchpad");
		vhdl << tab << declare("too_small") << " <= '1' when (shift_value" << of(scale_product_width-1)<<"='1') else '0';" << endl;
		vhdl << endl;

		// detect too_big to put in the NaN Flip-Flop
		addFullComment("detect too big scale for this specific scratchpad");
		//vhdl << tab << declare("too_big") << " <= '1' when (shift_value > CONV_STD_LOGIC_VECTOR(" << allowed_max_shift << "," << intlog2(allowed_max_shift) << ")) else '0';" << endl;
		vhdl << tab << declare("too_big") << " <= '1' when (unsigned(shift_value) > " << allowed_max_shift << " and too_small='0') else '0';" << endl;
		vhdl << endl;

		// part select from the shifted significand to partially form the summand
		const int summand_size = max_shift + 1;  // we do sign extension up to this number of bit
		if ( (m_msb_summand - m_lsb_summand + 1) < summand_size ) {
			addFullComment("shifted significand part select to form summand");
			//vhdl << tab << declare("ext_summand1c",m_wLAICPT2,true) << " <= " << zg(m_wLAICPT2) << " when too_small='1' else shifted_significand" << range(m_wLAICPT2-1,0) << ";" << endl;
			//vhdl << tab << declare("ext_summand1c",m_wLAICPT2,true) << " <= " << zg(m_wLAICPT2) << " when too_small='1' else shifted_significand" << range(m_wLAICPT2-1+significand_product_width,significand_product_width) << ";" << endl;
			vhdl << tab << declare("ext_summand1c",m_wLAICPT2,true) << " <= " << zg(m_wLAICPT2) << " when too_small='1' else shifted_significand" << range(m_wLAICPT2-1+significand_product_width-1,significand_product_width-1) << ";" << endl;
		} else {
			addFullComment("shifted significand part select to form summand");
			vhdl << tab << declare("summand1c", summand_size, true, Signal::wire) << " <= shifted_significand" << range(shifted_product_size-1,shifted_product_size-summand_size)<<";" << endl;

			// 2nd part of cpt2 building is sign extension in cpt1
			// leave the +1 for carry in of the 1st chunk in accumulation
			vhdl << tab << declare("ext_summand1c",m_wLAICPT2,true) << " <= " << zg(m_wLAICPT2) << " when too_small='1' else (" << (m_wLAICPT2-1<summand_size?"":rangeAssign(m_wLAICPT2-1, summand_size, "sign_M") + " & ") << "summand1c);" << endl;
			vhdl << endl;
		}
		// syncing signals
		addFullComment("Syncing some signals");
		vhdl << tab << declare(target->logicDelay(1), "not_ftz", 1, false, Signal::wire) << " <= not FTZ;" << endl;
		if (m_has_HSSD) vhdl << tab << declare("EOB_internal") << " <= EOB;" << endl;
		Signal *not_ftz, *ext_summand1c, *carry_0, *EOB_internal, *isNaN_M, *too_big;
		not_ftz = getSignalByName("not_ftz");
		ext_summand1c = getSignalByName("ext_summand1c");
		carry_0 = getSignalByName("sign_M");
		if (m_has_HSSD) EOB_internal = getSignalByName("EOB_internal");
		isNaN_M = getSignalByName("isNaN_M");
		too_big = getSignalByName("too_big");
		// we schedule here to have the cycle of ext_summand1c to sync not_ftz and other signals
		schedule();
		// applySchedule();
		int d1 = not_ftz->getCycle();
		int d2 = ext_summand1c->getCycle();
		int d3 = carry_0->getCycle();
		int d4 = 0;
        if (m_has_HSSD) d4 = EOB_internal->getCycle();
		int d5 = isNaN_M->getCycle();
		int d6 = too_big->getCycle();
		if (d2<d1 || d2<d3 || d2<d4 || d2<d5 || d2<d6) throw "develop sync the other way";
		addRegisteredSignalCopy("not_ftz_sync", "not_ftz", Signal::noReset, d2-d1);
		addRegisteredSignalCopy("carry_0_sync", "sign_M", Signal::noReset, d2-d3);
		if (m_has_HSSD) addRegisteredSignalCopy("EOB_internal_delayed", "EOB_internal", Signal::noReset, d2-d4+1);
		addRegisteredSignalCopy("isNaN_M_sync", "isNaN_M", Signal::noReset, d2-d5);
		addRegisteredSignalCopy("too_big_sync", "too_big", Signal::noReset, d2-d6);
		vhdl << endl;

		// The output NaN latch
		addFullComment("Output isNaN latch");
		vhdl << tab << declare(target->logicDelay(4), "isNaN_o", 1, false, Signal::wire) << " <= (too_big_sync or isNaN_M_sync or isNaN_delayed) when not_ftz_sync='1' else '0';" << endl;
		addRegisteredSignalCopy("isNaN_delayed", "isNaN_o", m_reset_type);
		vhdl << endl;

		// The Accumulation itself as a partial Carry Save Accumulator if m_nb_chunk is > 1
		addFullComment("Carry Save Accumulator");
		vhdl << tab << "-- DQ logic" << endl;
		for (int i = 0 ; i < m_nb_chunk ; ++i) {
		 	bool lc_i = (i==m_nb_chunk-1);  // lc : last chunk
		 	int chunk_size_i = lc_i ? m_last_chunk_size : m_chunk_size;
		 	string carry_i, summand_i, acc_i, summand_and_carry_i;
		 	summand_i = "summand_" + to_string(i);
		 	carry_i = "carry_" + to_string(i);
		 	acc_i = "acc_" + to_string(i);
			summand_and_carry_i = "summand_and_carry_" + to_string(i);
			declare(carry_i, 1, false, Signal::wire);
			declare(summand_i, chunk_size_i, true);
			declare(summand_and_carry_i, chunk_size_i+1, true);
			declare(acc_i, chunk_size_i+1, true);
		 	addRegisteredSignalCopy(acc_i+"_q", acc_i, m_reset_type);
		}

		// from here we do not schedule, we just connect loop FF in direct vhdl
		setNoParseNoSchedule();

		vhdl << endl << tab << "-- sequential addition logic" << endl;
		for (int i = 0 ; i < m_nb_chunk ; ++i) {
		 	bool fc_i = (i==0);  // fc : first chunk
		 	bool lc_i = (i==m_nb_chunk-1);  // lc : last chunk
		 	int chunk_size_i = lc_i ? m_last_chunk_size : m_chunk_size;
		 	int part_select_offset = (lc_i? m_wLAICPT2 : (m_chunk_size*(i+1)));
		 	vhdl << tab << "carry_" << i << " <= " << (fc_i ? "carry_0_sync":("acc_" + to_string(i-1) + "_q" + of(m_chunk_size))) << ";" << endl;
			vhdl << tab << "summand_" << i << " <= ext_summand1c" << range(part_select_offset-1, part_select_offset - chunk_size_i) << ";" << endl;
			vhdl << tab << "summand_and_carry_" << i << " <= (\"0\" & summand_" << i << ") + carry_" << i << ";" << endl;
			vhdl << tab << "acc_" << i << " <= ((\"0\" & acc_" << i << "_q" << range(chunk_size_i-1,0) << ") + summand_and_carry_" << i << ") when (not_ftz_sync='1') else" << endl
				 << tab << "         summand_and_carry_" << i << ";" << endl;
		}
		vhdl << endl;

		// compose Output
		addFullComment("Output Compose");
		vhdl << tab << "A <= ";
		for (int i = m_nb_chunk - 1 ; i >= 0 ; i--) {
			vhdl << join("acc_",i,"_q") << range(((i==m_nb_chunk-1)?m_last_chunk_size:m_chunk_size)-1,0) << (i>0?" & ":";\n");
		}

		if (m_nb_chunk > 1) {
			if (m_nb_chunk>2) {
				vhdl << tab << "C" << range(m_nb_chunk-2,0) << " <= ";
			} else {  // one bit
				vhdl << tab << "C(0) <= ";
			}
				for (int i=m_nb_chunk-1;i>=0; i--){
					vhdl << ((i<m_nb_chunk-1)?join("acc_",i,"_q")+ of(m_chunk_size) +((i>0)?" & ":""):"");
					// Uncomment the following line to generate the 0s for a direct rippling addition of A and C. Here we save bits for systolic array transports.
					// vhdl << (i==m_nb_chunk-1?zg(m_last_chunk_size-(i>0?1:0))+(i>0?" & ":""):(i>0?zg(m_chunk_size-1)+" & ":zg(m_chunk_size)));
				}vhdl << ";";
			}
		vhdl << endl;
		if (m_has_HSSD) vhdl << tab << "EOB_Q <= EOB_internal_delayed;" << endl;
		vhdl << tab << "isNaN <= isNaN_delayed;";
		vhdl << endl;

	}

	S3FDP::~S3FDP() {}

	void S3FDP::emulate(TestCase * tc) {}

	int S3FDP::get_wC() {
		return (m_nb_chunk-1);
	}

	bool S3FDP::hasCOutput() {
		return (m_nb_chunk > 1);
	}

	TestCase* S3FDP::buildRandomTestCase(int i){

		// // sign[1],nar[1],zero[1],scale[wS],mantissa[wM]


		TestCase *tc;
		// mpz_class x,y;

		// int wM, wS; get_pd_components_width(m_n, m_es, wS, wM);

		// // some signs
		// mpz_class sign_builder_X, sign_builder_Y;
		// mpz_class sign_pos = mpz_class(0) <<(wM+wS+2);
		// mpz_class sign_neg = mpz_class(1) <<(wM+wS+2);
		// mpz_class sign_random_1 = mpz_class(getLargeRandom(1)%2)<<(wM+wS+2);
		// mpz_class sign_random_2 = mpz_class(getLargeRandom(1)%2)<<(wM+wS+2);

		// // some scales
		// mpz_class scale_builder_X, scale_builder_Y;
		// mpz_class scale_zero = mpz_class(m_BIAS);  // 2^0 = scale 1
		// mpz_class scale_min_pos = mpz_class(0);
		// mpz_class scale_max_pos = mpz_class(m_max_absolute_dynamic_range+m_BIAS);
		// mpz_class scale_random_1 = getLargeRandom(wS);
		// mpz_class scale_random_2 = getLargeRandom(wS);
		// while (! ( m_max_absolute_dynamic_range >= scale_random_1 - m_BIAS)){
		// 	scale_random_1 = getLargeRandom(wS);
		// }
		// while (! ( m_max_absolute_dynamic_range >= scale_random_2 - m_BIAS)){
		// 	scale_random_2 = getLargeRandom(wS);
		// }

		// // some mantissas
		// mpz_class mantissa_builder_X, mantissa_builder_Y;
		// mpz_class mantissa_zero = mpz_class(0);
		// mpz_class mantissa_random_1 = getLargeRandom(wM);
		// mpz_class mantissa_random_2 = getLargeRandom(wM);

		// // building part depending on test number i:
		// // flush to zero some times

		// // the 10 first we add one to the accumulator
		// if (i >= 0 && i < 10) {
		// 	sign_builder_X = sign_pos;
		// 	sign_builder_Y = sign_pos;
		// 	scale_builder_X = scale_zero <<(wM);
		// 	scale_builder_Y = scale_zero <<(wM);
		// 	mantissa_builder_X = mantissa_zero;
		// 	mantissa_builder_Y = mantissa_zero;
		// }
		// // the 20 following, we substract 1
		// else if (i >= 10 && i < 30) {
		// 	sign_builder_X = sign_pos;
		// 	sign_builder_Y = sign_neg;
		// 	scale_builder_X = scale_zero << (wM);
		// 	scale_builder_Y = scale_zero << (wM);
		// 	mantissa_builder_X = mantissa_zero;
		// 	mantissa_builder_Y = mantissa_zero;
		// }
		// else if (i == 30) {
		// 	sign_builder_X = sign_pos;
		// 	sign_builder_Y = sign_pos;
		// 	scale_builder_X = scale_min_pos << (wM);
		// 	scale_builder_Y = scale_min_pos << (wM);
		// 	mantissa_builder_X = mantissa_zero;
		// 	mantissa_builder_Y = mantissa_zero;
		// }
		// else if (i == 31) {
		// 	sign_builder_X = sign_pos;
		// 	sign_builder_Y = sign_pos;
		// 	scale_builder_X = scale_max_pos << (wM);
		// 	scale_builder_Y = scale_max_pos << (wM);
		// 	mantissa_builder_X = mantissa_zero;
		// 	mantissa_builder_Y = mantissa_zero;
		// }
		// else {
		// 	sign_builder_X = sign_random_1;
		// 	sign_builder_Y = sign_random_2;
		// 	scale_builder_X = scale_random_1 << (wM);
		// 	scale_builder_Y = scale_random_2 << (wM);
		// 	mantissa_builder_X = mantissa_random_1;
		// 	mantissa_builder_Y = mantissa_random_2;

		// }


		// x = sign_builder_X + scale_builder_X + mantissa_builder_X;
		// y = sign_builder_Y + scale_builder_Y + mantissa_builder_Y;

		tc = new TestCase(this);
		// tc->addInput("pd_x", x);
		// tc->addInput("pd_y", y);

		// FTZ clock positions
		if (m_ftz_clock_positions.front() == i) {
			tc->addInput("FTZ", mpz_class(1));
			m_ftz_clock_positions.pop_front();
		} else {
			tc->addInput("FTZ", mpz_class(0));
		}

		// /* Get correct outputs */
		// emulate(tc);
		return tc;
	}

	int S3FDP::get_wLAICPT2() {
		return m_wLAICPT2;
	}

	int S3FDP::getPipelineDepth() {
		// works only for this component, it is mult + shift + 1
		int PP_sum = 0;
		for (auto i : getSubComponentList()) {
			PP_sum += (i->getPipelineDepth());
		}
		return PP_sum+1;
	}


	OperatorPtr S3FDP::parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args) {
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

		return new S3FDP(parentOp, target,
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

	void S3FDP::registerFactory() {
		UserInterface::add(
			"S3FDP",  // name
			"Accumulates w/o internal roundings(fused) a product of 2 S3 in a customizable accumulator precision",  // description, string
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
			S3FDP::parseArguments
		) ;
	}

}
