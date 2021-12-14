/*

  Normalazing Unit to eventually put after PDFDP
  Ripple the carry if partial CSA
  Correctly round the fixed point exact accumulation to packed posit format

  Author: Ledoux Louis

 */

#include "SA/PositUtils.hpp"
#include "Quire2Posit.hpp"

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

	Quire2Posit::Quire2Posit(
			OperatorPtr parentOp,
			Target* target,
			int n,
			int es,
			bool has_carry,
			int nb_bits_ovf,
			int LSBQ ) :
				Operator(parentOp, target),
				m_n(n),
				m_es(es),
				m_has_carry(has_carry),
				m_nb_bits_ovf(nb_bits_ovf),
				m_LSBQ(LSBQ){


		int mantissa_size, scale_size; get_pd_components_width(m_n, m_es, scale_size, mantissa_size);
		const int useed = (1<< (1<<m_es));  // 2^2^es
		m_max_absolute_dynamic_range = (m_n-2)*ceil(log2(useed));  // (2^2^es)^(n-2) ~ maxpos : in log domain
		m_MSB_summand = 2*m_max_absolute_dynamic_range;  // maxpos^2.

		if (m_nb_bits_ovf == -1) {
			REPORT(DETAILED, "bits ovf not user-given, going for " <<m_n-1<<"b for memory friendly transcations.")
			m_nb_bits_ovf=m_n-1;
		} else {
			REPORT(DETAILED, "bits ovf: " << m_nb_bits_ovf);
		}
		m_MSBQ = m_MSB_summand + m_nb_bits_ovf;

		if (m_LSBQ == -1) {
			REPORT(DETAILED, "LSB weight for the accumulator not given, going for full precision: -" << m_MSB_summand);
			m_LSBQ = -m_MSB_summand;
		} else {
			REPORT(DETAILED, "LSB weight for the accumulator is user-given and is: " << m_LSBQ);
		}

		REPORT(DETAILED, "MSB_summand is: " << m_MSB_summand);

		m_wQ = m_MSBQ - m_LSBQ + 1;  // add one bit for the sign

		ostringstream module_name;
		module_name << "Quire2Posit_"<< m_wQ << "_to_posit_" << m_n << "_" << m_es;
		setNameWithFreqAndUID(module_name.str());
		setCopyrightString("Ledoux Louis - BSC / UPC");
		m_BIAS = get_BIAS(m_n, m_es, 0);

		addInput("A", m_wQ);
		if (has_carry) addInput("C", m_wQ);
		addOutput("posit_O", m_n);

		if (has_carry) {
			vhdl << tab << declare("quire", m_wQ) << " <= A + C;" << endl; // can not overflow? TODO(replace) by PP adder of flopoco lib
		} else {
			vhdl << tab << declare("quire", m_wQ) << " <= A;" << endl;
		}

		vhdl << tab << declare("count_bit") << " <= quire" << of(m_wQ-1) << ";" << endl;

		ostringstream param_lzoc, inmap_lzoc, outmap_lzoc;
		param_lzoc << "wIn=" << m_wQ;
		param_lzoc << " wOut=" << mantissa_size + 2;  // catch implicit bit
		param_lzoc << " wCount=" << intlog2(m_wQ);
		param_lzoc << " countType=-1";
		param_lzoc << " computeSticky=true";
		inmap_lzoc << "I => quire";
		inmap_lzoc << ",OZb => count_bit";
		outmap_lzoc << "Count => count_lzoc_o";
		outmap_lzoc << ",O => frac_lzoc_o";
		outmap_lzoc << ",Sticky => sticky_lzoc_o";
		newInstance("LZOCShifterSticky", "lzoc_inst", param_lzoc.str(), inmap_lzoc.str(), outmap_lzoc.str());

		vhdl << endl << tab <<declare("biased_exp", intlog2(m_wQ)) << " <= CONV_STD_LOGIC_VECTOR("<<m_wQ-1<<","<<intlog2(m_wQ)<<") - count_lzoc_o;" << endl;
		vhdl << tab << declare("unbiased_exp", intlog2(m_wQ)) << " <= biased_exp - CONV_STD_LOGIC_VECTOR(" << 2*m_BIAS << "," << intlog2(m_wQ) <<");" << endl;
		vhdl << endl;

		///////////////////////////////////////////////
		vhdl << tab << declare(0.0, "fraction", mantissa_size+1) << " <= frac_lzoc_o " << range (mantissa_size, 0) << ";" << endl;
		if (m_es > 0) {
		  vhdl << declare(0., "partial_exponent", m_es) << "<= unbiased_exp" << range(m_es-1, 0) << ";" << endl;

		  vhdl << "with count_bit select " << declare(target->logicDelay(m_es), "partial_exponent_us", m_es) << " <= " << endl <<
		    tab << "partial_exponent when '0'," << endl <<
		    tab << "not partial_exponent when others;" << endl;
		}

		int wCount = intlog2(m_n);
		if (m_n > 4) {
			vhdl << declare(.0, "bin_regime", wCount) << "<= unbiased_exp" << range(intlog2(m_wQ) -3, m_es) << ";" << endl;
		} else {
			vhdl << declare(.0, "bin_regime", wCount) << "<= unbiased_exp" << range(intlog2(m_wQ) -2, m_es) << ";" << endl;
		}
			vhdl << declare(.0, "first_regime", 1, false) << "<= unbiased_exp" << of(intlog2(m_wQ)-1) << ";" << endl;

		vhdl << "with first_regime select " << declare(target->logicDelay(wCount), "regime", wCount) << " <= "  << endl <<
		  tab << "bin_regime when '0', " << endl <<
		  tab << "not bin_regime when others;" << endl;

		//par quoi on commence (exp négatif ou positif)
		vhdl << declare(target->logicDelay(2), "pad", 1, false) << "<= not(first_regime xor count_bit);" << endl;

	       	vhdl << "with pad select " << declare(target->logicDelay(1), "start_regime", 2) << " <= "  << endl <<
		  tab << "\"01\" when '0', " << endl <<
		  tab << "\"10\" when others; " << endl;

		if (m_es>0) {
		  vhdl << declare(0., "in_shift", m_n) << " <= start_regime & partial_exponent_us & fraction;" << endl;
		}
		else {
		   vhdl << declare(0., "in_shift", m_n) << " <= start_regime & fraction;" << endl;
		}

		ostringstream param_shift, inmap_shift, outmap_shift;
		param_shift << "wIn=" << m_n;
		param_shift << " maxShift=" << m_n;
		param_shift << " wOut=" << m_n;
		param_shift << " dir=" << Shifter::Right;
		param_shift << " computeSticky=true";
		param_shift << " inputPadBit=true";
		inmap_shift << "X=>in_shift,S=>regime,padBit=>pad";
		outmap_shift << "R=>extended_posit,Sticky=>pre_sticky";
		newInstance("Shifter", "rshift", param_shift.str(), inmap_shift.str(), outmap_shift.str());


		//rounding up

		vhdl << declare(0.,"truncated_posit", m_n - 1) << "<= extended_posit" << range(m_n-1, 1) << ";" << endl;
		vhdl << declare(0., "lsb", 1, false) << " <= extended_posit" << of(1) << ";" << endl;
		vhdl << declare(0., "guard", 1, false) << " <= extended_posit" << of(0) << ";" << endl;
		vhdl << declare(0., "sticky", 1, false) << " <= fraction" << of(0) << " or pre_sticky or sticky_lzoc_o;" << endl;

		vhdl << declare(target->logicDelay(3),"round_bit", 1, false) << "<= guard and (sticky or lsb);" << endl;

		vhdl << declare(target->adderDelay(m_n-1), "rounded_reg_exp_frac", m_n-1) << "<= truncated_posit + round_bit;" << endl;
		vhdl << declare(0., "rounded_posit", m_n) << " <= count_bit & rounded_reg_exp_frac;" << endl;
		vhdl << declare(0., "is_zero") << " <= count_lzoc_o" << of(intlog2(m_wQ)-1) << " when fraction=\"" << string(mantissa_size+1,'0') << "\" else '0';" << endl;
		vhdl << declare(target->logicDelay(1), "rounded_posit_zero", m_n) << "<= rounded_posit when is_zero= '0' else \"" << string(m_n, '0') << "\";" << endl;
		vhdl << "posit_O <= rounded_posit_zero;" << endl; //when is_NAR = '0' else \"1" << string(m_n-1, '0') << "\";" << endl;

	}

	Quire2Posit::~Quire2Posit() {}

	void Quire2Posit::emulate(TestCase * tc) {}

	TestCase* Quire2Posit::buildRandomTestCase(int i){

		TestCase *tc;
		mpz_class x,y;
		tc = new TestCase(this);

		int wM, wS; get_pd_components_width(m_n, m_es, wS, wM);

		// the 10 first we add one to the accumulator
		if (i==0) {
			x = mpz_class(0xFFFFFFDF);
			tc->addInput("A", x);
			//tc->addInput("", y);
		}

		/* Get correct outputs */
		emulate(tc);
		return tc;
	}

	OperatorPtr Quire2Posit::parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args) {
		int posit_width, posit_es;
		int nb_bits_ovf;
		bool has_carry;
		int LSBQ;
		UserInterface::parseInt(args, "n", &posit_width);
		UserInterface::parseInt(args, "es", &posit_es);
		UserInterface::parseBoolean(args, "has_carry", &has_carry);
		UserInterface::parseInt(args, "nb_bits_ovf", &nb_bits_ovf);
		UserInterface::parseInt(args, "LSBQ", &LSBQ);
		return new Quire2Posit(parentOp, target, posit_width, posit_es, has_carry, nb_bits_ovf, LSBQ);
	}

	void Quire2Posit::registerFactory() {
		UserInterface::add(
			"Quire2Posit",  // name
			"Transforms a Quire value to a dense posit value. Eventually ripple the carry and performs 1 correct rounding (nearest)",  // description, string
			"SA",  // category, from the list defined in UserInterface.cpp
			"",  //seeAlso
			"n(int): The posit word size; \
			 es(int): The size of the tunable exponent; \
			 has_carry(bool)=false: If the Quire was a n-radix CSA and not a RCA; \
			 nb_bits_ovf(int)=-1: The number of bits after the actual MSB weight of summands; \
			 LSBQ(int)=-1: The weight of the LSB of the customizable Quire",
			"",  // More documentation for the HTML pages. If you want to link to your blog, it is here.
			Quire2Posit::parseArguments
		) ;
	}

}
