#include "Posit_to_S3.hpp"

// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

#include "utils.hpp"
#include "SA/PositUtils.hpp"
#include "ShiftersEtc/LZOCShifterSticky.hpp"

using namespace std;

namespace flopoco {

	Posit_to_S3::Posit_to_S3(Target* target, Operator* parentOp, const int posit_width, const int posit_es):Operator(parentOp, target), m_posit_width(posit_width), m_posit_es(posit_es) {

		// definition of the name of the operator
		ostringstream name;
		name << "Posit_to_S3_" << m_posit_width << "_" << m_posit_es;
		setName(name.str());
		// Copyright
		setCopyrightString("BSC / UPC - Ledoux Louis");

		// Initial checks to handle if the posit configuration is doable
		// 1 : Minimum size posit is 3bits/word
		if (m_posit_width < 3) {
			throw std::string("Posit_to_S3 Constructor : width is too small, should be at least 3 bits wide");
		}
		// 2 : Avoid posits without even one bit of precision
		if (m_posit_es >= m_posit_width - 3) {
			throw std::string("Posit_to_S3 Constructor : posit exponent is too big, no room for mantissa");
		}

		// compute internal field size
		get_pd_components_width(m_posit_width, m_posit_es, m_scale_width, m_fraction_width);

		// SET UP THE IO SIGNALS

		addInput ("arith_i" , m_posit_width);
		addOutput("S3_o" , m_scale_width + m_fraction_width + 3);  // scale + fraction + isZero + S + isNaR

		REPORT(DETAILED, "this operator has received two parameters " << m_posit_width << " and " << m_posit_es);

		// Extract parts of input
		vhdl << declare(.0, "sign", 1, false) << " <= arith_i" << of(m_posit_width - 1) << ";" << endl;
		vhdl << declare(.0, "regime_check", 1, false) << " <= arith_i" << of(m_posit_width - 2) << ";" << endl;
		vhdl << declare(.0, "remainder", m_posit_width - 2) << " <= arith_i" << range(m_posit_width - 3, 0) << ";" << endl;

		vhdl << declare(target->logicDelay(1), "not_s", 1, false) << " <= not sign;" << endl;

		vhdl << declare(target->adderDelay(m_posit_width-2), "zero_NAR", 1, false) << " <= " <<
			"not regime_check when remainder=\"" << string(m_posit_width -2, '0') <<
			"\" else '0';" << endl;


		vhdl << declare(target->logicDelay(2), "is_NAR", 1, false) << " <= zero_NAR and sign;" << endl;

		vhdl << declare(target->logicDelay(2), "implicit", 1, false) << " <= not(zero_NAR and not_s);" << endl;

		vhdl << declare(target->logicDelay(2), "neg_count", 1, false) << " <= not (sign xor regime_check);" << endl;

		// LZOC instantiation
		ostringstream parametric_lzoc, inputs_lzoc, outputs_lzoc;
		int wCount = intlog2(m_posit_width-2);
		//int wCount = ceil(log2(m_posit_width-2));
		parametric_lzoc << "wIn=" << m_posit_width - 2;
		parametric_lzoc << " wOut=" << m_posit_width - 2;
		parametric_lzoc << " wCount=" << wCount;
		inputs_lzoc << "I => remainder,";
		inputs_lzoc << "OZb => regime_check";
		outputs_lzoc << "Count => lzCount,";
		outputs_lzoc << "O => usefulBits";
		newInstance("LZOCShifterSticky", "lzoc", parametric_lzoc.str(), inputs_lzoc.str(), outputs_lzoc.str());

		vhdl << "with neg_count select " << declare(0., "extended_neg_count", wCount+1) << " <= "  << endl <<
		  tab << "\"" << string(wCount+1, '1') << "\" when '1', "<< endl <<
		  tab << "\"" << string(wCount+1, '0') << "\" when others;" << endl;

		vhdl << declare(target->logicDelay(wCount+1), "comp2_range_count", wCount+1) << " <= extended_neg_count xor (\"0\" & lzCount);" << endl;

		vhdl << declare(0., "fraction", m_fraction_width) << " <= usefulBits" << range( m_fraction_width - 1, 0) <<";" << endl;

		if (m_posit_es > 0) {
		  vhdl << declare(0., "partialExponent", m_posit_es) << " <= usefulBits" << range( m_posit_width - 4, m_fraction_width) << ";" << endl;

		  vhdl << "with sign select " << declare(target->logicDelay(m_posit_es), "us_partialExponent", m_posit_es) << " <= " << endl <<
		    tab << "partialExponent when '0'," << endl <<
		    tab << "not partialExponent when '1'," << endl <<
		    tab << "\"" << string(m_posit_es, '-') << "\" when others;" << endl;

		  vhdl << declare(0., "exponent", m_scale_width) << " <= comp2_range_count & us_partialExponent;" << endl;
		}
		else {
		  vhdl << declare(0., "exponent", m_scale_width) << " <= comp2_range_count;" << endl;
		}

		vhdl << declare(target->adderDelay(m_scale_width), "biased_exponent", m_scale_width) << " <= exponent + " << ((m_posit_width - 2)<< m_posit_es) << ";" << endl;

		vhdl << "S3_o <= is_NAR & sign & implicit & fraction & biased_exponent;" << endl;
	};


	void Posit_to_S3::emulate(TestCase * tc) {
	}


	OperatorPtr Posit_to_S3::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int width, es;
		UserInterface::parseInt(args, "posit_width", &width);
		UserInterface::parseInt(args, "posit_es", &es);
		return new Posit_to_S3(target, parentOp, width, es);
	}

	void Posit_to_S3::registerFactory() {
		UserInterface::add(
			"Posit_to_S3",  // name
			"Converts Posit to \"S3 format\"(HW friendly representation)",  // description, string
			"SA",  // category, from the list defined in UserInterface.cpp
			"",  //seeAlso
			"posit_width(int): The posit word size; \
			 posit_es(int): The size of tunable exponent",
			"",  // More documentation for the HTML pages. If you want to link to your blog, it is here.
			Posit_to_S3::parseArguments
		) ;
	}

}//namespace
