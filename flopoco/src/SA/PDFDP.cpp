/*
  pdfdp(Posit Denormalized Fused Dot Product) for FloPoCo

  Author: Ledoux Louis

 */

#include "SA/PositUtils.hpp"
#include "PDFDP.hpp"

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

	PDFDP::PDFDP(
			OperatorPtr parentOp,
			Target* target,
			int n,
			int es,
			int nb_bits_ovf,
			double dspOccupationThreshold,
			int LSBQ,
			int chunk_size) :
				Operator(parentOp, target),
				m_n(n),
				m_es(es),
				m_nb_bits_ovf(nb_bits_ovf),
				m_dspOccupationThreshold(dspOccupationThreshold),
				m_LSBQ(LSBQ),
				m_chunk_size(chunk_size){

		setHasDelay1Feedbacks();

		ostringstream module_name;
		module_name << "pdfdp_" << n << "_" << es;
		setNameWithFreqAndUID(module_name.str());
		setCopyrightString("Ledoux Louis - BSC / UPC");
		m_reset_type = Signal::asyncReset;  // {noReset | asyncReset | syncReset}
		m_BIAS = get_BIAS(m_n, m_es, 0);
		// test bench purpose
		m_ftz_clock_positions.push_back(0);
		m_ftz_clock_positions.push_back(30);

		if (m_nb_bits_ovf == -1) {
			REPORT(DETAILED, "bits ovf not user-given, going for " <<m_n-1<<"b for memory friendly transcations.")
			m_nb_bits_ovf=m_n-1;
		} else {
			REPORT(DETAILED, "bits ovf: " << m_nb_bits_ovf);
		}

		REPORT(DETAILED, "this operator received user parameters: n=" << m_n << " / es=" << m_es << " / nb_bits_ovf=" << m_nb_bits_ovf);

		// Compute some internal parameters
		int mantissa_size, scale_size; get_pd_components_width(m_n, m_es, scale_size, mantissa_size);
		const int pd_size_normal = get_pd_size(m_n, m_es, POSIT_MEMORY);
		const int useed = (1<< (1<<m_es));  // 2^2^es
		m_max_absolute_dynamic_range = (m_n-2)*ceil(log2(useed));  // (2^2^es)^(n-2) ~ maxpos : in log domain
		int msb_summand = 2*m_max_absolute_dynamic_range;  // maxpos^2.
		m_MSB_summand = msb_summand;  // class atribute to be used in emulate and so on
		const int msb_q = msb_summand + m_nb_bits_ovf;

		if (m_LSBQ == -1) {
			REPORT(DETAILED, "LSB weight for the accumulator not given, going for full precision: -" << msb_summand);
			m_LSBQ = -msb_summand;
		} else {
			REPORT(DETAILED, "LSB weight for the accumulator is user-given and is: " << m_LSBQ);
		}

		REPORT(DETAILED, "MSB_summand is: " << msb_summand);

		int wQ = msb_q - m_LSBQ + 1;  // add one bit for the sign
		m_wQ = wQ;
		REPORT(DETAILED, "The quire width is: " << wQ);

		if (m_chunk_size == -1) {
  			// TODO(lledoux): the VUP target yields a 412bits adder for 400MHz which is a lot
			// TODO(2)(lledoux): i could create a suggestAccChunk method that takes into account the looped fanout(ed) routing
			target->suggestSubaddSize(m_chunk_size, wQ);
			REPORT(DETAILED, "Partial Carry Save accumulator chunk width not given. Auto-Computing for " << target->getID() << ": " << m_chunk_size << "b.");
			REPORT(DETAILED, "A more realistic suggestion is under devlopment");
		} else {
			REPORT(DETAILED, "Partial Carry Save accumulator chunk width is user-given and is: " << m_chunk_size);
		}

		int nb_chunk = ceil(double(wQ)/double(m_chunk_size));
		m_nb_chunk = nb_chunk;
		int last_chunk_size = ( wQ % m_chunk_size == 0 ? m_chunk_size  : wQ % m_chunk_size);
		if ( wQ % m_chunk_size == 0 ) {
			REPORT(DETAILED, "The quire is splited in " << nb_chunk << " chunks of " << m_chunk_size << "b.");
		} else {
			REPORT(DETAILED, "The quire is splited in " << nb_chunk-1 << " chunks of " << m_chunk_size << "b and a last one of " << last_chunk_size << "b.");
		}

		// IOs declaration
		addInput("pd_x", pd_size_normal);
		addInput("pd_y", pd_size_normal);
		addInput("FTZ");
		// Note on FTZ:
		// active high boolean to put the accumulator registers to 0. Sync with incoming data
		// if val !=0 and ftz the acc will be equal to val
		addInput("EOB");
		// Note on EOB:
		// stands for End Of Block, we input an active high boolean indicating the end of an accumulation in order to
		// propagates it with the inner delay of this operator

		addOutput("A", wQ);  // Partial accumulation
		if (nb_chunk > 1) {
			addOutput("C", wQ);  // Carry to ripple
		}
		addOutput("EOB_Q");

		// sign processing
		addFullComment("sign processing");
		vhdl << tab << declare(.0, "sign_X", 1, false, Signal::wire) << " <= pd_x" << of(pd_size_normal-1) << ";" << endl;
		vhdl << tab << declare(.0, "sign_Y", 1, false, Signal::wire) << " <= pd_y" << of(pd_size_normal-1) << ";" << endl;
		vhdl << tab << declare(target->logicDelay(2), "sign_M", 1, false, Signal::wire) << " <= sign_X xor sign_Y;" << endl;
		vhdl << endl;

		// zero processing
		// we will fill the isZero bit into the implicit bit of the multiplication operands assuming that mantissas are 0
		// they should since they come from my Posit Decode Unit
		addFullComment("zero processing");
		vhdl << tab << declare(.0, "is_not_zero_X", 1, false, Signal::wire) << " <= not(pd_x" << of(pd_size_normal-3) << ");" << endl;
		vhdl << tab << declare(.0, "is_not_zero_Y", 1, false, Signal::wire) << " <= not(pd_y" << of(pd_size_normal-3) << ");" << endl;
		vhdl << endl;

		// mantissa product processing
		// not checking "overflow" for scale re-adjustment since it is fused with accumulation process
		addFullComment("significand processing");
		const int product_size = 2*(mantissa_size+1);
		vhdl << tab << declare(.0, "mantissa_X", 1 + mantissa_size, true, Signal::wire) << " <= is_not_zero_X & pd_x" << range(mantissa_size-1,0) << ";" << endl;
		vhdl << tab << declare(.0, "mantissa_Y", 1 + mantissa_size, true, Signal::wire) << " <= is_not_zero_Y & pd_y" << range(mantissa_size-1,0) << ";" << endl;
		ostringstream parametric_mult, inputs_mult, outputs_mult;
		parametric_mult << "wX=" << mantissa_size + 1;
		parametric_mult << " wY=" << mantissa_size + 1;
		parametric_mult << " wOut=" << product_size;
		parametric_mult << " dspThreshold=" << m_dspOccupationThreshold;
		inputs_mult << "X=>mantissa_X,";
		inputs_mult << "Y=>mantissa_Y";
		outputs_mult << "R=>significand_product";
		newInstance("IntMultiplier", "significand_product_inst", parametric_mult.str(), inputs_mult.str(), outputs_mult.str());
		vhdl << endl;

		// scale processing
		const int product_scale_size = scale_size+1;
		REPORT(DETAILED, "BIAS for this posit configuration is: " << m_BIAS);
		addFullComment("scale processing");
		vhdl << tab << declare(.0, "scale_X_biased", scale_size, true, Signal::wire) << " <= pd_x" << range(scale_size+mantissa_size-1,mantissa_size) << ";" << endl;
		vhdl << tab << declare(.0, "scale_Y_biased", scale_size, true, Signal::wire) << " <= pd_y" << range(scale_size+mantissa_size-1,mantissa_size) << ";" << endl;
		vhdl << tab << declare(getTarget()->adderDelay(product_scale_size), "product_scale_twice_biased", product_scale_size) << " <= (\"0\" & scale_X_biased) + (\"0\" & scale_Y_biased);" << endl;
		vhdl << endl;

		// left shifter
		const int max_shift = -m_LSBQ + msb_summand;
		int size_shifter_S_bus = intlog2(max_shift);
		const int shifted_product_size = max_shift + product_size;  //TODO check when fresh mind
		const int summand_size = -m_LSBQ + msb_summand + 1;  // we do sign extension up this number of bit
		// this is the LSB weight but biased / offset
		// if it is 0 it is an exact accumulator and shift value is simply the scale summand biased
		const int minimum_quire_offset_binade = m_BIAS + m_LSBQ;
		addFullComment("significand product shifting");
		if (minimum_quire_offset_binade > 0) {
			vhdl << tab << declare(target->adderDelay(product_scale_size), "shift_value", product_scale_size) << " <= product_scale_twice_biased - " << minimum_quire_offset_binade << ";" << endl;
		} else {
			vhdl << tab << declare("shift_value", product_scale_size) << " <= product_scale_twice_biased;" << endl;
		}
		vhdl << tab << declare("shift_value_S_bus", size_shifter_S_bus) << " <= shift_value" << range(size_shifter_S_bus-1, 0) << ";" << endl;

		ostringstream parametric_shifter, inputs_shifter, outputs_shifter;
		parametric_shifter << "wIn=" << product_size;
		parametric_shifter << " maxShift=" << max_shift;
		parametric_shifter << " dir=" << Shifter::Left;
		parametric_shifter << " wOut=" << shifted_product_size;
		parametric_shifter << " computeSticky=False";
		parametric_shifter << " inputPadBit=False";
		inputs_shifter << "X=>significand_product,";
		inputs_shifter << "S=>shift_value_S_bus";
		outputs_shifter << "R=>shifted_frac";
		newInstance("Shifter", "significand_product_shifter_inst", parametric_shifter.str(), inputs_shifter.str(), outputs_shifter.str());
		vhdl << endl;

		// TODO(lledoux) : eventually add 0 when shiftval is neg in the case of minimum_quire_offset_binade > 0
		// this means the accum was not accurate enough in a case of a non exact ACCUM


		// 2's comp the summand
		addFullComment("2's complement of the summand");
		vhdl << tab << declare("summand", summand_size, true, Signal::wire) << " <= shifted_frac" << range(shifted_product_size-2,shifted_product_size-1-summand_size)<<";" << endl;

		// Don't compute 2's complement just yet, just invert the bits and leave
		// the addition of the extra 1 in accumulation, as a carry in bit for the
		// first chunk
		// TODO(lledoux): perform the bit inversion before the shift for + right and left padding with sign bit (less expensive)
		vhdl << tab << declare(target->logicDelay(summand_size), "summand1c", summand_size, true) << " <= summand when sign_M='0' else not(summand);"<< endl;
		vhdl << tab << declare("ext_summand1c",wQ,true) << " <= " << (wQ-1<summand_size?"":rangeAssign(wQ-1, summand_size, "sign_M") + " & ") << "summand1c;" << endl;
		vhdl << endl;

		// syncing signals
		addFullComment("Syncing some signals");
		vhdl << tab << declare(target->logicDelay(1), "not_ftz", 1, false, Signal::wire) << " <= not FTZ;" << endl;
		vhdl << tab << declare("EOB_internal") << " <= EOB;" << endl;
		Signal *not_ftz, *ext_summand1c, *carry_0, *EOB_internal;
		not_ftz = getSignalByName("not_ftz");
		ext_summand1c = getSignalByName("ext_summand1c");
		carry_0 = getSignalByName("sign_M");
		EOB_internal = getSignalByName("EOB_internal");
		// we schedule here to have the cycle of ext_summand1c to sync not_ftz
		schedule();
		int d1 = not_ftz->getCycle();
		int d2 = ext_summand1c->getCycle();
		int d3 = carry_0->getCycle();
		int d4 = EOB_internal->getCycle();
		if (d2<d1 || d2<d3 || d2 < d4) throw "develop sync the other way";
		addRegisteredSignalCopy("not_ftz_sync", "not_ftz", Signal::noReset, d2-d1);
		addRegisteredSignalCopy("carry_0_sync", "sign_M", Signal::noReset, d2-d3);
		addRegisteredSignalCopy("EOB_internal_delayed", "EOB_internal", Signal::noReset, d2-d4+1);
		vhdl << endl;

		// The Accumulation itself as a partial Carry Save Accumulator if nb_chunk is > 1
		addFullComment("Carry Save Accumulator");
		vhdl << tab << "-- DQ logic" << endl;
		for (int i = 0 ; i < nb_chunk ; ++i) {
		 	bool lc_i = (i==nb_chunk-1);  // lc : last chunk
		 	int chunk_size_i = lc_i ? last_chunk_size : m_chunk_size;
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
		for (int i = 0 ; i < nb_chunk ; ++i) {
		 	bool fc_i = (i==0);  // fc : first chunk
		 	bool lc_i = (i==nb_chunk-1);  // lc : last chunk
		 	int chunk_size_i = lc_i ? last_chunk_size : m_chunk_size;
		 	int part_select_offset = (lc_i? wQ : (m_chunk_size*(i+1)));
		 	vhdl << tab << "carry_" << i << " <= " << (fc_i ? "carry_0_sync":("acc_" + to_string(i-1) + "_q" + of(m_chunk_size))) << ";" << endl;
			vhdl << tab << "summand_" << i << " <= ext_summand1c" << range(part_select_offset-1, part_select_offset - chunk_size_i) << ";" << endl;
			vhdl << tab << "summand_and_carry_" << i << " <= (\"0\" & summand_" << i << ") + carry_" << i << ";" << endl;
			vhdl << tab << "acc_" << i << " <= ((\"0\" & acc_" << i << "_q" << range(chunk_size_i-1,0) << ") + summand_and_carry_" << i << ") when (not_ftz_sync='1') else" << endl
				 << tab << "         summand_and_carry_" << i << ";" << endl;
		}

		// for (int i = 0 ; i < nb_chunk ; i++) {
		// 	vhdl << tab << "-- partial carry save accumulator chunk " << i << endl;
		// 	// some variable to help building signals
		// 	bool lc_i = (i==nb_chunk-1);  // lc : last chunk
		// 	bool fc_i = (i==0);  // fc : first chunk
		// 	int chunk_size_i = lc_i ? last_chunk_size : m_chunk_size;
		// 	string csa_chunk_reg_im1, csa_chunk_comb_i, csa_chunk_reg_i, carry_i, summand_i, acc_i;
		// 	csa_chunk_reg_im1 = "csa_reg_" + to_string(i-1);
		// 	csa_chunk_comb_i = "csa_comb_" + to_string(i);
		// 	csa_chunk_reg_i = "csa_reg_" + to_string(i);
		// 	carry_i = "carry_" + to_string(i);
		// 	summand_i = "summand_" + to_string(i);
		// 	acc_i = "acc_" + to_string(i);
		// 	int part_select_offset = (lc_i? wQ : (m_chunk_size*(i+1)));
		// 	// comb logic design
		// 	vhdl << tab << declare(carry_i, 1, false, Signal::wire) << " <= " << (fc_i ? "sign_M":csa_chunk_reg_im1 + of(m_chunk_size)) << ";" << endl;
		// 	vhdl << tab << declare(summand_i, chunk_size_i, true) << " <= ext_summand1c" << range(part_select_offset-1, part_select_offset - chunk_size_i) << ";" << endl;
		// 	vhdl << tab << declare(csa_chunk_comb_i, chunk_size_i+1, true) << " <= ((\"0\" & " << summand_i << ") + " << carry_i << ");" << endl;
		// 	vhdl << tab << declare(acc_i, chunk_size_i+1, true) << " <= " << endl
		// 		 << tab << tab << "((\"0\" & "<< acc_i<<"^1"<< range(chunk_size_i-1,0) << ") + " << csa_chunk_comb_i << ") when (not_ftz_sync='1') else" << endl
		// 		 << tab << tab << rangeAssign(chunk_size_i-1+1,0,"'0'") << " ;"<< endl;
		// 	getSignalByName(acc_i) -> setHasBeenScheduled(true);
		// 	// DQ logic
		// 	addRegisteredSignalCopy(csa_chunk_reg_i, acc_i, m_reset_type);
		// }
		// vhdl << tab << "-- msb carry bit <=> NaR, inf, ovf" << endl;
		// vhdl << tab << declare(join("carry_", nb_chunk), 1, false, Signal::wire) << " <= csa_reg_" << nb_chunk-1 << of(last_chunk_size) << ";" << endl;
		vhdl << endl;

		// compose Output
		addFullComment("Output Compose");
		vhdl << tab << "A <= ";
		for (int i = nb_chunk - 1 ; i >= 0 ; i--) {
			vhdl << join("acc_",i,"_q") << range(((i==nb_chunk-1)?last_chunk_size:m_chunk_size)-1,0) << (i>0?" & ":";\n");
		}

		if (nb_chunk > 1) {
			vhdl << tab << "C <= ";
				for (int i=nb_chunk-1;i>=0; i--){
					vhdl << (i<nb_chunk-1?join("acc_",i,"_q")+ of(m_chunk_size) +" & ":"");
					vhdl << (i==nb_chunk-1?zg(last_chunk_size-(i>0?1:0))+(i>0?" & ":""):(i>0?zg(m_chunk_size-1)+" & ":zg(m_chunk_size)));
				}vhdl << ";";
			}
		vhdl << tab << "EOB_Q <= EOB_internal_delayed;";
		vhdl << endl;

	}

	PDFDP::~PDFDP() {}

	void PDFDP::emulate(TestCase * tc) {}

	bool PDFDP::hasCOutput() {
		return (m_nb_chunk > 1);
	}

	TestCase* PDFDP::buildRandomTestCase(int i){

		// sign[1],nar[1],zero[1],scale[wS],mantissa[wM]


		TestCase *tc;
		mpz_class x,y;

		int wM, wS; get_pd_components_width(m_n, m_es, wS, wM);

		// some signs
		mpz_class sign_builder_X, sign_builder_Y;
		mpz_class sign_pos = mpz_class(0) <<(wM+wS+2);
		mpz_class sign_neg = mpz_class(1) <<(wM+wS+2);
		mpz_class sign_random_1 = mpz_class(getLargeRandom(1)%2)<<(wM+wS+2);
		mpz_class sign_random_2 = mpz_class(getLargeRandom(1)%2)<<(wM+wS+2);

		// some scales
		mpz_class scale_builder_X, scale_builder_Y;
		mpz_class scale_zero = mpz_class(m_BIAS);  // 2^0 = scale 1
		mpz_class scale_min_pos = mpz_class(0);
		mpz_class scale_max_pos = mpz_class(m_max_absolute_dynamic_range+m_BIAS);
		mpz_class scale_random_1 = getLargeRandom(wS);
		mpz_class scale_random_2 = getLargeRandom(wS);
		while (! ( m_max_absolute_dynamic_range >= scale_random_1 - m_BIAS)){
			scale_random_1 = getLargeRandom(wS);
		}
		while (! ( m_max_absolute_dynamic_range >= scale_random_2 - m_BIAS)){
			scale_random_2 = getLargeRandom(wS);
		}

		// some mantissas
		mpz_class mantissa_builder_X, mantissa_builder_Y;
		mpz_class mantissa_zero = mpz_class(0);
		mpz_class mantissa_random_1 = getLargeRandom(wM);
		mpz_class mantissa_random_2 = getLargeRandom(wM);

		// building part depending on test number i:
		// flush to zero some times

		// the 10 first we add one to the accumulator
		if (i >= 0 && i < 10) {
			sign_builder_X = sign_pos;
			sign_builder_Y = sign_pos;
			scale_builder_X = scale_zero <<(wM);
			scale_builder_Y = scale_zero <<(wM);
			mantissa_builder_X = mantissa_zero;
			mantissa_builder_Y = mantissa_zero;
		}
		// the 20 following, we substract 1
		else if (i >= 10 && i < 30) {
			sign_builder_X = sign_pos;
			sign_builder_Y = sign_neg;
			scale_builder_X = scale_zero << (wM);
			scale_builder_Y = scale_zero << (wM);
			mantissa_builder_X = mantissa_zero;
			mantissa_builder_Y = mantissa_zero;
		}
		else if (i == 30) {
			sign_builder_X = sign_pos;
			sign_builder_Y = sign_pos;
			scale_builder_X = scale_min_pos << (wM);
			scale_builder_Y = scale_min_pos << (wM);
			mantissa_builder_X = mantissa_zero;
			mantissa_builder_Y = mantissa_zero;
		}
		else if (i == 31) {
			sign_builder_X = sign_pos;
			sign_builder_Y = sign_pos;
			scale_builder_X = scale_max_pos << (wM);
			scale_builder_Y = scale_max_pos << (wM);
			mantissa_builder_X = mantissa_zero;
			mantissa_builder_Y = mantissa_zero;
		}
		else {
			sign_builder_X = sign_random_1;
			sign_builder_Y = sign_random_2;
			scale_builder_X = scale_random_1 << (wM);
			scale_builder_Y = scale_random_2 << (wM);
			mantissa_builder_X = mantissa_random_1;
			mantissa_builder_Y = mantissa_random_2;

		}


		x = sign_builder_X + scale_builder_X + mantissa_builder_X;
		y = sign_builder_Y + scale_builder_Y + mantissa_builder_Y;

		tc = new TestCase(this);
		tc->addInput("pd_x", x);
		tc->addInput("pd_y", y);

		// FTZ clock positions
		if (m_ftz_clock_positions.front() == i) {
			tc->addInput("FTZ", mpz_class(1));
			m_ftz_clock_positions.pop_front();
		} else {
			tc->addInput("FTZ", mpz_class(0));
		}

		/* Get correct outputs */
		emulate(tc);
		return tc;
	}

	int PDFDP::get_wQ() {
		return m_wQ;
	}

	int PDFDP::getPipelineDepth() {
		// works only for this component, it is mult + shift + 1
		int PP_sum = 0;
		for (auto i : getSubComponentList()) {
			PP_sum += (i->getPipelineDepth());
		}
		return PP_sum+1;
	}

	OperatorPtr PDFDP::parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args) {
		int posit_width, posit_es;
		int nb_bits_ovf;
		double dspThreshold;
		int LSBQ;
		int chunk_size;
		UserInterface::parseInt(args, "n", &posit_width);
		UserInterface::parseInt(args, "es", &posit_es);
		UserInterface::parseInt(args, "nb_bits_ovf", &nb_bits_ovf);
		UserInterface::parseFloat(args, "dspThreshold", &dspThreshold);
		UserInterface::parseInt(args, "LSBQ", &LSBQ);
		UserInterface::parseInt(args, "chunk_size", &chunk_size);
		return new PDFDP(parentOp, target, posit_width, posit_es, nb_bits_ovf, dspThreshold, LSBQ, chunk_size);
	}

	void PDFDP::registerFactory() {
		UserInterface::add(
			"PDFDP",  // name
			"Accumulates w/o internal roundings(fused) a product of 2 PD in a customizable accumulator precision",  // description, string
			"SA",  // category, from the list defined in UserInterface.cpp
			"",  //seeAlso
			"n(int): The posit word size; \
			 es(int): The size of the tunable exponent; \
			 nb_bits_ovf(int)=-1: The number of bits after the actual MSB weight of summands. One bit adds one binade to prevent overflow in non cancellation accumulations. These bits affect the size of the adder but not of the shifter, they are not too expansive; \
			 dspThreshold(real)=0.0: The ratio of dsp over logic for mantissa product; \
			 LSBQ(int)=-1: The weight of the LSB of the customizable Quire. This controls the accuracy of the accumulator. If not precised, the tool will go to full precision; \
			 chunk_size(int)=-1: An expert can suggest a size for a sub adder chunk for the carry save accum. This should be target specific calculated by the tool",
			"",  // More documentation for the HTML pages. If you want to link to your blog, it is here.
			PDFDP::parseArguments
		) ;
	}

}
