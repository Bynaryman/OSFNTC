#include "IEEE_to_S3.hpp"

// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

#include "utils.hpp"

using namespace std;

namespace flopoco {

	IEEE_to_S3::IEEE_to_S3(Target* target, Operator* parentOp, const int exponent_width, const int mantissa_width):Operator(parentOp, target), m_exponent_width(exponent_width), m_mantissa_width(mantissa_width) {

		// definition of the name of the operator
		ostringstream name;
		name << "IEEE_to_S3_" << m_exponent_width << "_" << m_mantissa_width;
		setName(name.str());
		// Copyright
		setCopyrightString("BSC / UPC - Ledoux Louis");

		uint32_t input_width  = 1+m_exponent_width+m_mantissa_width;
		uint32_t output_width = 3+m_exponent_width+m_mantissa_width;

		// SET UP THE IO SIGNALS
		addInput ("arith_i" , input_width);
		addOutput("S3_o" , output_width);  // output has the form "isNaN,sign,implicit,fraction,exponent"

		// Extract parts of input
		vhdl << tab << declare(.0, "sign", 1, false) << " <= arith_i" << of(input_width - 1) << ";" << endl;
		vhdl << tab << declare(.0, "exponent", m_exponent_width) << " <= arith_i" << range(input_width - 2, input_width-1-m_exponent_width) << ";" << endl;
		vhdl << tab << declare(.0, "fraction", m_mantissa_width) << " <= arith_i" << range(m_mantissa_width-1, 0) << ";" << endl;

		// Detect special cases (NaNs and Subnormals)
		vhdl << tab << declare(target->adderDelay(m_exponent_width), "isNaN", 1, false) << " <= " <<
			"'1' when exponent=\"" << string(m_exponent_width, '1') <<
			"\" else '0';" << endl;
		vhdl << tab << declare(target->adderDelay(m_exponent_width), "isExpSubnormalZero", 1, false) << " <= " <<
			"'1' when exponent=\"" << string(m_exponent_width, '0') <<
			"\" else '0';" << endl;

		// Implicit bit is zero when ieee is zero or when it is subnormal.
		vhdl << tab << declare(target->logicDelay(1), "implicit", 1, false) << " <= not(isExpSubnormalZero);" << endl;

		// final scale is the input scale except for the subnormal case
		// The exponent of S3(0) will be the same as for subnormals but significand distinguish the two cases
		vhdl << tab << declare(target->logicDelay(m_exponent_width), "final_scale", m_exponent_width) << "<= \""<< string(m_exponent_width-1, '0') <<"1\" when isExpSubnormalZero= '1' else  exponent;" << endl;

		vhdl << tab << "S3_o <= isNaN & sign & implicit & fraction & final_scale;" << endl;
	};


	void IEEE_to_S3::emulate(TestCase * tc) {
	}


	OperatorPtr IEEE_to_S3::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int exponent_width, mantissa_width;
		UserInterface::parseInt(args, "exponent_width", &exponent_width);
		UserInterface::parseInt(args, "mantissa_width", &mantissa_width);
		return new IEEE_to_S3(target, parentOp, exponent_width, mantissa_width);
	}

	void IEEE_to_S3::registerFactory() {
		UserInterface::add(
			"IEEE_to_S3",  // name
			"Converts IEEE/Bfloat/Tapered Float to a \"S3 format\"(HW friendly representation)",  // description, string
			"SA",  // category, from the list defined in UserInterface.cpp
			"",  //seeAlso
			"exponent_width(int): The width of the biased exponent, controls the dynamic range; \
			 mantissa_width(int): The width of the mantissa, controls the precision",
			"",  // More documentation for the HTML pages. If you want to link to your blog, it is here.
			IEEE_to_S3::parseArguments
		) ;
	}

}//namespace
