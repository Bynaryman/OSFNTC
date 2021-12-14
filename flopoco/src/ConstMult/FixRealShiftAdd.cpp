// TODO: repair FixFIR, FixIIR, FixComplexKCM
/*
 * A faithful multiplier by a real constant, using a variation of the KCM method
 *
 * This file is part of the FloPoCo project developed by the Arenaire
 * team at Ecole Normale Superieure de Lyon
 * 
 * Authors : Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr
 * 			 3IF-Dev-Team-2015
 *
 * Initial software.
 * Copyright © ENS-Lyon, INRIA, CNRS, UCBL, 
 * 2008-2011.
 * All rights reserved.
 */
/*

Remaining 1-ulp bug:
flopoco verbose=3 FixRealShiftAdd lsbIn=-8 msbIn=0 lsbOut=-7 constant="0.16" signedIn=true TestBench
It is the limit case of removing one table altogether because it contributes nothng.
I don't really understand

*/
#include "FixRealShiftAdd.hpp"

#if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB) && defined(HAVE_SCALP)


#include "pagsuite/types.h"

#include "../Operator.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <sollya.h>
#include "../utils.hpp"
#include "../IntAddSubCmp/IntAdder.hpp"

#include "pagsuite/log2_64.h"
#include "pagsuite/csd.h"
#include "pagsuite/rpag.h"

#include "adder_cost.hpp"
#include "error_comp_graph.hpp"
#include "WordLengthCalculator.hpp"
#include "IntConstMultShiftAddTypes.hpp"
#include "error_comp_graph.hpp"

using namespace std;


namespace flopoco{




	
	//standalone operator
	FixRealShiftAdd::FixRealShiftAdd(OperatorPtr parentOp, Target* target, bool signedIn_, int msbIn_, int lsbIn_, int lsbOut_, string constant_, double targetUlpError_):
		FixRealConstMult(parentOp, target, signedIn_, msbIn_, lsbIn_, lsbOut_, constant_, targetUlpError_)
	{
		vhdl << "-- This operator multiplies by " << constant << endl;

		srcFileName = "FixRealShiftAdd";
		setNameWithFreqAndUID("FixRealShiftAdd");


		constStringToSollya();

		msbOut = msbIn + msbC;
		int wIn = msbIn - lsbIn + 1;

		useNumericStd();
		addInput("X", wIn);

		// Now we can check when this is a multiplier by 0: either because the it is zero, or because it is close enough
		if (mpfr_zero_p(mpC) != 0)
		{
			REPORT(INFO, "It seems somebody asked for a multiplication by 0. We can do that.");
			addOutput("R", wIn);
			vhdl << tab << "R <= (others => '0');" << endl;
			return;
		}

		REPORT(INFO, "Output precisions: msbOut=" << msbOut << ", lsbOut=" << lsbOut);

		mpfr_t mpOp1, mpOp2; //temporary variables for operands
		mpfr_init2(mpOp1, 100);
		mpfr_init2(mpOp2, 100);

		int guardBits = 2;  //two additional guard bit required??
		//compute error bounds of the result (epsilon_max)
		mpfr_t mpEpsilonMax;
		mpfr_init2(mpEpsilonMax, 100);
		mpfr_set_si(mpOp1, 2, GMP_RNDN);
		mpfr_set_si(mpOp2, lsbOut - guardBits, GMP_RNDN);

		mpfr_pow(mpEpsilonMax, mpOp1, mpOp2, GMP_RNDN); //2^(lsbOut-1)
		ios::fmtflags old_settings = cout.flags();
		REPORT(INFO, "Epsilon max=" << std::scientific << mpfr_get_d(mpEpsilonMax, GMP_RNDN));
		cout.flags(old_settings);

		//compute the different integer representations of the constant
		int q = 4;
		//mx = msbIn
		//lR = lsbOut
		int wCOut = msbIn - lsbOut + q + 1;
		mpfr_t s;
		mpfr_init2(s, 64);
		mpfr_set_si(s, wCOut, GMP_RNDN);

		cout << "coefficient word size = " << msbC + wCOut << endl;
		mpfr_t mpCInt;
		mpfr_init2(mpCInt, msbC + wCOut); //msbC-lsbOut+q+1

		int noOfFullAddersBest = INT32_MAX;
		mpz_class mpzCIntBest;
		int shiftTotalBest;
		string adderGraphStrBest;
		PAGSuite::adder_graph_t adderGraphBest;
		string trunactionStrBest;
		IntConstMultShiftAdd_TYPES::TruncationRegister truncationRegBest("");

		for (int k = -(1 << q); k <= (1 << q); k++)
		{
			mpfr_rnd_t roundingDirection;
			if (k > 0)
				roundingDirection = MPFR_RNDD;
			else
				roundingDirection = MPFR_RNDU;

			mpfr_t two_pow_s;
			mpfr_init2(two_pow_s, 100); //msbIn-lsbOut+q+1
			mpfr_set_si(mpOp1, 2, GMP_RNDN);
			mpfr_pow(two_pow_s, mpOp1, s, GMP_RNDN);
			mpfr_mul(mpCInt, mpC, two_pow_s, roundingDirection);

			mpfr_t mpk;
			mpfr_init2(mpk, 64);
			mpfr_set_si(mpk, k, GMP_RNDN);
			mpfr_add(mpCInt, mpCInt, mpk, roundingDirection);

			mpz_class mpzCInt;
			mpfr_get_z(mpzCInt.get_mpz_t(), mpCInt, GMP_RNDN);

			mpz_class isEven = (mpzCInt & 0x01) != 1;
			int shift = 0;
			while (isEven.get_ui())
			{
				mpzCInt /= 2;
				mpfr_div_ui(mpCInt, mpCInt, 2, GMP_RNDN);
				shift++;
				isEven = (mpzCInt & 0x01) != 1;
			}

			//compute epsilons
			//compute mpEpsilonCoeff = C - Cint * 2^(-(wIn+q-shift));
			mpfr_t mpEpsilonCoeff;
			mpfr_init2(mpEpsilonCoeff, 100);
			mpfr_set_si(mpOp1, 2, GMP_RNDN);
			mpfr_set_si(mpOp2, -(wIn + q - shift), GMP_RNDN);
			mpfr_pow(mpOp1, mpOp1, mpOp2, GMP_RNDN);
			mpfr_mul(mpOp1, mpCInt, mpOp1, GMP_RNDN);
			mpfr_sub(mpEpsilonCoeff, mpC, mpOp1, GMP_RNDN);

			//compute mpEpsilonMult = mpEpsilonMax - abs(mpEpsilonCoeff);
			mpfr_t mpEpsilonMult;
			mpfr_init2(mpEpsilonMult, 100);
			mpfr_abs(mpOp1, mpEpsilonCoeff, GMP_RNDN);
			mpfr_sub(mpEpsilonMult, mpEpsilonMax, mpOp1, GMP_RNDN);

			//compute mpEpsilonCoeffNorm = mpEpsilonCoeff*2^(wIn+q-shift-lsbOut);
			mpfr_t mpEpsilonCoeffNorm;
			mpfr_init2(mpEpsilonCoeffNorm, 100);
			mpfr_set_si(mpOp1, 2, GMP_RNDN);
			mpfr_set_si(mpOp2, wIn + q - shift - lsbOut, GMP_RNDN);
			mpfr_pow(mpOp1, mpOp1, mpOp2, GMP_RNDN);
			mpfr_mul(mpEpsilonCoeffNorm, mpEpsilonCoeff, mpOp1, GMP_RNDN);

			//compute mpEpsilonMultNorm = mpEpsilonMult*2^(wIn+q-shift-lsbOut);
			mpfr_t mpEpsilonMultNorm;
			mpfr_init2(mpEpsilonMultNorm, 100);
			mpfr_mul(mpEpsilonMultNorm, mpEpsilonMult, mpOp1, GMP_RNDN);

			int shiftTotal = mpfr_get_si(s, GMP_RNDN) - shift;

			ios::fmtflags old_settings = cerr.flags();
			REPORT(INFO, "k=" << k << ", constant = " << mpzCInt << " / 2^" << shiftTotal
							  << ", mpEpsilonCoeff=" << std::scientific << mpfr_get_d(mpEpsilonCoeff, GMP_RNDN)
							  << ", mpEpsilonMult=" << std::scientific << mpfr_get_d(mpEpsilonMult, GMP_RNDN)
							  << ", mpEpsilonCoeffNorm=" << std::fixed << mpfr_get_d(mpEpsilonCoeffNorm, GMP_RNDN)
							  << ", mpEpsilonMultNorm=" << std::fixed << mpfr_get_d(mpEpsilonMultNorm, GMP_RNDN));
			cerr.flags(old_settings);

			mpfr_t mpEpsilonMultNormInt;
			mpfr_init2(mpEpsilonMultNormInt, 100);
			mpfr_floor(mpEpsilonMultNormInt, mpEpsilonMultNorm);
			long epsilonMultNormInt = mpfr_get_si(mpEpsilonMultNormInt, GMP_RNDN); //should be mpz type instead

			PAGSuite::adder_graph_t adderGraph;
			string adderGraphStr;
			computeAdderGraph(adderGraph, adderGraphStr, (PAGSuite::int_t) mpzCInt.get_si());

			int noOfFullAddersBeforeTrunc = IntConstMultShiftAdd_TYPES::getGraphAdderCost(adderGraph, wIn, false);
			REPORT(INFO, "  adder graph before truncation requires " << noOfFullAddersBeforeTrunc << " full adders");


			map<pair<mpz_class, int>, vector<int> > wordSizeMap;

			WordLengthCalculator wlc = WordLengthCalculator(adderGraph, wIn, epsilonMultNormInt);
			wordSizeMap = wlc.optimizeTruncation();
			REPORT(DEBUG, "Finished computing word sizes of truncated MCM");
			if (UserInterface::verbose >= DETAILED)
			{
				for (auto &it : wordSizeMap)
				{
					std::cout << "(" << it.first.first << ", " << it.first.second << "): ";
					for (auto &itV : it.second)
						std::cout << itV << " ";
					std::cout << std::endl;
				}
			}
			IntConstMultShiftAdd_TYPES::TruncationRegister truncationReg(wordSizeMap);

			string trunactionStr = truncationReg.convertToString();
			REPORT(INFO, "  truncation: " << trunactionStr);
			int noOfFullAddersAfterTrunc = IntConstMultShiftAdd_TYPES::getGraphAdderCost(adderGraph, wIn, false, truncationReg);
			REPORT(INFO, "  adder graph after truncation requires " << noOfFullAddersAfterTrunc << " full adders");

//			IntConstMultShiftAdd_TYPES::print_aligned_word_graph(adderGraph, "", wIn, cout);
//			IntConstMultShiftAdd_TYPES::print_aligned_word_graph(adderGraph, truncationReg, wIn, cout);

			old_settings = cout.flags();
			cout << k << " & $" << mpzCInt << "/2^" << shiftTotal << "$"
				 << " & " << std::scientific << mpfr_get_d(mpEpsilonCoeff, GMP_RNDN)
				 << " & " << std::scientific << mpfr_get_d(mpEpsilonMult, GMP_RNDN)
				 << " & " << std::fixed << mpfr_get_d(mpEpsilonCoeffNorm, GMP_RNDN)
				 << " & " << std::fixed << mpfr_get_d(mpEpsilonMultNorm, GMP_RNDN)
				 << " & " << std::fixed << noOfFullAddersBeforeTrunc
				 << " & " << std::fixed << noOfFullAddersAfterTrunc
				 << " & "
			;
			cout.flags(old_settings);


			if (noOfFullAddersAfterTrunc < noOfFullAddersBest)
			{
				noOfFullAddersBest = noOfFullAddersAfterTrunc;
				mpzCIntBest = mpzCInt;
				shiftTotalBest = shiftTotal;
				adderGraphStrBest = adderGraphStr;
				adderGraphBest = adderGraph;
				trunactionStrBest = trunactionStr;
				truncationRegBest = truncationReg;
			}

		}
		REPORT(INFO, "best solution found for coefficient " << mpzCIntBest << " / 2^" << shiftTotalBest << " with "
															<< noOfFullAddersBest << " full adders")
		REPORT(INFO, "  adder graph " << adderGraphStrBest);
		REPORT(INFO, "  truncations:" << trunactionStrBest);

		IntConstMultShiftAdd_TYPES::print_aligned_word_graph(adderGraphBest, "", wIn, cout);
		IntConstMultShiftAdd_TYPES::print_aligned_word_graph(adderGraphBest, truncationRegBest, wIn, cout);

		output_node_t* output_node= nullptr;
		for(adder_graph_base_node_t *node : adderGraphBest.nodes_list)
		{
			if(PAGSuite::is_a<PAGSuite::output_node_t>(*node))
			{
				if(node->output_factor[0][0] == ((int64_t) mpzCIntBest.get_ui()))
				{
					output_node = ((output_node_t*) node);
					break;
				}
			}
		}
		if(output_node != nullptr)
		{
			IntConstMultShiftAdd_TYPES::ErrorStorage es = getAccumulatedErrorFor(output_node,truncationRegBest);
			cout << "error is +" << es.positive_error << " / -" << es.negative_error << endl;
		}

		//VHDL code generation:
		int wOut = msbOut - lsbOut + 1;

		mpfr_t tmp;
		mpfr_init2(tmp, 100);
		mpfr_set_z(tmp, mpzCIntBest.get_mpz_t(), GMP_RNDN);
		mpfr_log2(tmp, tmp, GMP_RNDN);
		mpfr_ceil(tmp, tmp);
		long wC = mpfr_get_si(tmp, GMP_RNDN);

		int wConstMultRes = wIn + wC;

		addOutput("R", wOut);

		declare("tmp", wOut + 1);
		declare("constMultRes", wConstMultRes);

		stringstream parameters;
		parameters << "wIn=" << wIn << " graph=" << adderGraphStrBest;
		parameters << " truncations=" << trunactionStrBest;
		string inPortMaps = "x_in0=>X";
		stringstream outPortMaps;
		outPortMaps << "x_out0_c" << mpzCIntBest << "=>constMultRes";

		cout << "outPortMaps: " << outPortMaps.str() << endl;
		newInstance("IntConstMultShiftAdd", "IntConstMultShiftAddComponent", parameters.str(), inPortMaps,
					outPortMaps.str());

		bool doProperRounding=true;
		if(doProperRounding)
		{
			stringstream one;
			for (int i = 0; i < wOut; i++)
				one << "0";
			one << "1";
			vhdl << tab << "tmp <= std_logic_vector(signed(constMultRes(" << wConstMultRes - 1 << " downto "
				 << wConstMultRes - wOut - 1 << ")) + \"" << one.str() << "\");" << endl;
			vhdl << tab << "R <= tmp(" << wOut << " downto " << 1 << ");" << endl;
		}
		else
		{
			vhdl << tab << "R <= constMultRes(" << wConstMultRes-1 << " downto " << wConstMultRes-wOut << ");" << endl;
		}
	}


	bool FixRealShiftAdd::computeAdderGraph(PAGSuite::adder_graph_t &adderGraph, string &adderGraphStr, long long int coefficient)
	{
		set<PAGSuite::int_t> target_set;

		target_set.insert(coefficient);

		int depth = PAGSuite::log2c_64(PAGSuite::nonzeros(coefficient));

//		REPORT(INFO, "depth=" << depth);

		PAGSuite::rpag *rpag = new PAGSuite::rpag(); //default is RPAG with 2 input adders

		for(PAGSuite::int_t t : target_set)
			rpag->target_set->insert(t);

		if(depth > 3)
		{
			REPORT(DEBUG, "depth is 4 or more, limit search limit to 1");
			rpag->search_limit = 1;
		}
		if(depth > 4)
		{
			REPORT(DEBUG, "depth is 5 or more, limit MSD permutation limit");
			rpag->msd_digit_permutation_limit = 1000;
		}
		PAGSuite::global_verbose = UserInterface::verbose-2; //set rpag to one less than verbose of FloPoCo

		PAGSuite::cost_model_t cost_model = PAGSuite::LL_FPGA;// with default value
		rpag->input_wordsize = msbIn-lsbIn;
		rpag->set_cost_model(cost_model);
		rpag->optimize();

		vector<set<PAGSuite::int_t>> pipeline_set = rpag->get_best_pipeline_set();

		list<PAGSuite::realization_row<PAGSuite::int_t> > rpagAdderGraph;
		PAGSuite::pipeline_set_to_adder_graph(pipeline_set, rpagAdderGraph, true, rpag->get_c_max());
		PAGSuite::append_targets_to_adder_graph(pipeline_set, rpagAdderGraph, target_set);

		adderGraphStr = PAGSuite::output_adder_graph(rpagAdderGraph,true);

		REPORT(INFO, "  adderGraphStr=" << adderGraphStr);

		if(UserInterface::verbose >= 3)
			adderGraph.quiet = false; //enable debug output
		else
			adderGraph.quiet = true; //disable debug output, except errors

		REPORT( DETAILED, "parse graph...")
		bool validParse = adderGraph.parse_to_graph(adderGraphStr);

		if(validParse)
		{

			REPORT(DETAILED, "check graph...")
			adderGraph.check_and_correct(adderGraphStr);

			if (UserInterface::verbose >= DETAILED)
				adderGraph.print_graph();
			adderGraph.drawdot("pag_input_graph.dot");

//			REPORT(INFO, "  adderGraph=" << adderGraph.get_adder_graph_as_string());

			return true;
		}
		return false;
	}



	OperatorPtr FixRealShiftAdd::parseArguments(OperatorPtr parentOp, Target* target, std::vector<std::string> &args)
	{
		int lsbIn, lsbOut, msbIn;
		bool signedIn;
		double targetUlpError;
		string constant;
		UserInterface::parseInt(args, "lsbIn", &lsbIn);
		UserInterface::parseString(args, "constant", &constant);
		UserInterface::parseInt(args, "lsbOut", &lsbOut);
		UserInterface::parseInt(args, "msbIn", &msbIn);
		UserInterface::parseBoolean(args, "signedIn", &signedIn);
		UserInterface::parseFloat(args, "targetUlpError", &targetUlpError);	
		return new FixRealShiftAdd(
													parentOp,
													target, 
													signedIn,
													msbIn,
													lsbIn,
													lsbOut,
													constant, 
													targetUlpError
													);
	}


	void flopoco::FixRealShiftAdd::registerFactory()	{
		UserInterface::add(
				"FixRealShiftAdd",
				"Table based real multiplier. Output size is computed",
				"ConstMultDiv",
				"",
				"signedIn(bool): 0=unsigned, 1=signed; \
				msbIn(int): weight associated to most significant bit (including sign bit);\
				lsbIn(int): weight associated to least significant bit;\
				lsbOut(int): weight associated to output least significant bit; \
				constant(string): constant given in arbitrary-precision decimal, or as a Sollya expression, e.g \"log(2)\"; \
				targetUlpError(real)=1.0: required precision on last bit. Should be strictly greater than 0.5 and lesser than 1;",
				"This variant of Ken Chapman's Multiplier is briefly described in <a href=\"bib/flopoco.html#DinIstoMas2014-SOPCJR\">this article</a>.<br> Special constants, such as 0 or powers of two, are handled efficiently.",
				FixRealShiftAdd::parseArguments,
				FixRealShiftAdd::unitTest
		);
	}
}//namespace
#else // we need the factory but we need it to be empty
namespace flopoco{
	void flopoco::FixRealShiftAdd::registerFactory()	{};
}	
#endif //#defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB)




