#include <sstream>

#include "Posit2FP.hpp"
#include "ShiftersEtc/LZOCShifterSticky.hpp"
#include "TestBenches/PositNumber.hpp"
#include "TestBenches/IEEENumber.hpp"

using namespace std;

namespace flopoco{
	void Posit2FP::computeFloatWidths(int const widthI, int const eS, int* wE, int* wF)
	{
		*wE = intlog2(widthI - 1) + 1 + eS;
		*wF = widthI - (eS + 3);
	}

	Posit2FP::Posit2FP(Operator* parentOp, Target* target, 
			int widthI, 
			int esI):Operator(parentOp, target), widthI_(widthI), esI_(esI)
	{
		srcFileName = "Posit2FP";

		ostringstream name;
		name << "Posit2FP_" << widthI << "_" << esI ;
		setNameWithFreqAndUID(name.str());
		setCopyrightString("test");

		if (widthI < 3) {
			throw std::string("Posit2FP Constructor : widthI is too small, should be greater than two");
		}
		int freeWidth = widthI - 1;
		if (esI >= freeWidth) {
			//Avoid posits without even one bit of precision
			throw std::string("Posit2FP Constructor : invalid value of esI");
		}
	
		addInput("I", widthI);		
		computeFloatWidths(widthI, esI, &wE_, &wF_);
		addOutput("O", wE_ + wF_ + 1);				

		vhdl << declare(target->adderDelay(widthI), "isZero", 1, false) << " <= " <<
			"'1' when I=\"" << string(widthI, '0') << "\" else '0';" << endl;

		vhdl << declare(.0, "sign", 1, false) << "<= I" << of(widthI - 1) << 
			";" << endl;
		vhdl << declare(.0, "encoding", widthI - 1) << "<= I" << range(widthI - 2, 0) << 
			";" << endl;
		vhdl << "with sign select " << 
			declare(target->logicDelay(widthI), "absoluteEncoding", widthI - 1) <<
			" <= " << endl << tab <<  "encoding when '0'," << endl <<
			tab << "not(encoding) + 1 when '1'," << endl <<
			tab << "\"" << string(widthI - 1, '-') << "\" when others;" << endl;

		vhdl << declare(0., "exponentSign", 1, false) << " <= absoluteEncoding" <<
			of(widthI - 2) << ";" << endl;
		vhdl << declare(0., "encodingTail", widthI - 2, true) << " <= absoluteEncoding" <<
		   range(widthI - 3, 0) << ";" << endl;	
		ostringstream param, inmap, outmap;
		int wCount = intlog2(widthI - 1);
		param << "wIn=" << widthI - 2;
		param << " wOut=" << widthI - 2;
		param << " wCount=" << wCount; 

		inmap << "I=>encodingTail,OZb=>exponentSign";

		outmap << "Count=>lzCount,O=>shiftedResult";

		newInstance("LZOCShifterSticky", "lzoc", param.str(), inmap.str(), outmap.str());
		
		vhdl << "with exponentSign select " << 
			declare(target->logicDelay(wCount), "rangeExp", wCount + 1, true) <<
			" <= " << endl << tab <<  "('1' & not (lzCount)) when '0'," << endl <<
			tab << "('0' & lzCount) when '1'," << endl <<
			tab << "\"" << string(wCount + 1, '-') << "\" when others;" << endl;

		ostringstream concat;

		if (esI > 0) {
			vhdl << declare(0., "exponentVal", esI, true) << " <= shiftedResult" <<
				range(wF_ + esI -1, wF_) << ";" << endl;

			concat << " & exponentVal";
		}
	
		uint64_t bias = (1 << (wE_ - 1)) - 1;
		vhdl << declare(target->adderDelay(wE_), "exponent", wE_, true) << 
			" <=  (rangeExp " << concat.str() << ") + " << bias << ";" << endl;

		vhdl << declare(0., "mantissa", wF_, true) << " <= shiftedResult" <<
			range(wF_ - 1, 0) << ";" << endl;

		vhdl << "with isZero select O <=" << endl <<
			tab << "\"" << string(1+wE_+wF_, '0') << "\" when '1'," << endl <<
			tab << "(sign & exponent & mantissa) when '0'," << endl <<
			tab << "\"" << string(1+wE_ + wF_, '-') << "\" when others;" << endl;
	}

	void Posit2FP::emulate(TestCase* tc)
	{
		mpz_class si = tc->getInputValue("I");
		PositNumber posit(widthI_, esI_, si);		
		mpfr_t val;
		mpfr_init2(val, widthI_ - 2);
		posit.getMPFR(val);
		IEEENumber num(wE_, wF_, val);
		tc->addExpectedOutput("O", num.getSignalValue());
		mpfr_clear(val);
	}

	OperatorPtr Posit2FP::parseArguments(Operator* parentOp, Target *target, 
			std::vector<std::string> & args)
	{
		int width, es;
		UserInterface::parseStrictlyPositiveInt(args, "width", &width);
		UserInterface::parsePositiveInt(args, "es", &es);
		return new Posit2FP(parentOp, target, width, es);
	}
	
	void Posit2FP::registerFactory()
	{
		UserInterface::add("Posit2FP",
				"Convert Posit to floating point",
				"Conversions",
				"",
				"width(int): total size of the encoding;\
				es(int): exponent field length;",
				"",
				Posit2FP::parseArguments
			);
	}

}
