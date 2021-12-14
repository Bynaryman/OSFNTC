/*
 * Floating Point FMA for FloPoCo
 *
 * Author: Florent de Dinechin
 * Copyright 2010 ENS-Lyon, INRIA, CNRS, UCBL
 *
 * This file is part of the FloPoCo project developed by the Arenaire team at Ecole Normale Superieure de Lyon
 */

// Notations: as in the Handbook, FMA computes A*B+C


// TODO ieee: propagate input NaN to output, manage sNaNs/qNaNs properly 
// TODO ieee: other rounding modes than RN 

// TODO test: complete standardTestCases

// TODO opt: a shifter+sticky for the first shift if it is pipelined (otherwise useless?)
// TODO opt: rework negation of bigsum
// TODO opt: clean up exptentative/expupdate

#include "IEEEFMA.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <string.h>

#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include <utils.hpp>
#include <Operator.hpp>


using namespace std;

namespace flopoco{
	
	extern vector<Operator*> oplist;

#define DEBUGVHDL 0


	IEEEFMA::IEEEFMA(OperatorPtr parentOp, Target* target, int wE_, int wF_) :
		Operator(parentOp, target), wE(wE_), wF(wF_) {
	
		ostringstream name;

		srcFileName="IEEEFMA";

		name<<"IEEEFMA"; 
		name<<"_"<<wE<<"_"<<wF; 
		setNameWithFreqAndUID(name.str());

		setCopyrightString("Florent de Dinechin (2009-2019)");		
		addHeaderComment("Inputs: this FMA computes A*B+C");

		int emin = -(1 << (wE-1)) + 2;
		mpz_class bias = (mpz_class(1)<<(wE-1)) - 1;

		// Set up the IO signals
		// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		addIEEEInput ("A", wE, wF);
		addIEEEInput ("B", wE, wF);
		addIEEEInput ("C", wE, wF);
		addInput("negateAB"); 
		addInput("negateC"); 
		addInput("RndMode", 2); 
		addIEEEOutput("R", wE, wF);
		
		int p=wF+1; // notation used in the HandBook of Floating-Point Arithmetic
		
		vhdl << endl;
		vhdl << tab << " -- Input decomposition " << endl;
		vhdl << tab << declare("Asgn")
				 << " <= A" << of(wE+wF) << ";" << endl;
		vhdl << tab << declare("AexpField", wE)
				 << " <= A" << range(wE+wF-1, wF) << ";" << endl;
		vhdl << tab << declare("AsigField", wF)
				 << " <= A" << range(wF-1, 0) << ";" << endl;
		vhdl << tab << declare("AisNormal") << " <= A" << of(wF);
		for(int i=1; i<wE; i++) 
			vhdl << " or A" << of(wF+i);
		vhdl <<  ";" << endl;
		vhdl << tab << declare("AisInfOrNaN") << " <= A" << of(wF);
		for(int i=1; i<wE; i++) 
			vhdl << " and A" << of(wF+i);
		vhdl <<  ";" << endl;
		vhdl << tab << declare("AhasNonNullSig") << " <= A" << of(0);
		for(int i=1; i<wF; i++) 
			vhdl << " or A" << of(i);
		vhdl <<  ";" << endl;
		vhdl << tab << declare("AisZero") << " <= (not AisNormal) and not AhasNonNullSig;" << endl;
		vhdl << tab << declare("AisInf") << " <= AisInfOrNaN and not AhasNonNullSig;" << endl;
		vhdl << tab << declare("AisNaN") << " <= AisInfOrNaN and AhasNonNullSig;" << endl;


		vhdl << tab << declare("Bsgn")
				 << " <= B" << of(wE+wF) << ";" << endl;
		vhdl << tab << declare("BexpField", wE)
				 << " <= B" << range(wE+wF-1, wF) << ";" << endl;
		vhdl << tab << declare("BsigField", wF)
				 << " <= B" << range(wF-1, 0) << ";" << endl;
		vhdl << tab << declare("BisNormal") << " <= B" << of(wF);
		for(int i=1; i<wE; i++) 
			vhdl << " or B" << of(wF+i);
		vhdl <<  ";" << endl;
		vhdl << tab << declare("BisInfOrNaN") << " <= B" << of(wF);
		for(int i=1; i<wE; i++) 
			vhdl << " and B" << of(wF+i);
		vhdl <<  ";" << endl;
		vhdl << tab << declare("BhasNonNullSig") << " <= B" << of(0);
		for(int i=1; i<wF; i++) 
			vhdl << " or B" << of(i);
		vhdl <<  ";" << endl;
		vhdl << tab << declare("BisZero") << " <= (not BisNormal) and not BhasNonNullSig;" << endl;
		vhdl << tab << declare("BisInf") << " <= BisInfOrNaN and not BhasNonNullSig;" << endl;
		vhdl << tab << declare("BisNaN") << " <= BisInfOrNaN and BhasNonNullSig;" << endl;

		vhdl << endl << tab << "-- unbiased exponents make everything simpler but may lead to suboptimal arch " << endl;
		vhdl << tab << declare("Aexp", wE)
				 << " <= AexpField - (\"0\" & " << rangeAssign(wE-2,1,"'1'") << " & AisNormal);" << endl;
		vhdl << tab << declare("Bexp", wE)
				 << " <= BexpField - (\"0\" & " << rangeAssign(wE-2,1,"'1'") << " & BisNormal);" << endl;


		vhdl << endl << tab << "-- mantissa with the implicit 1 or 0 appended " << endl;
		vhdl << tab << declare("Asig", wF+1) << " <= AisNormal & AsigField ;" << endl;
		vhdl << tab << declare("Bsig", wF+1) << " <= BisNormal & BsigField ;" << endl;


		vhdl << tab << declare("AexpPlusBexp", wE+1) << " <= (Aexp" << of(wE-1) << " & Aexp) + (Bexp" << of(wE-1) << " & Bexp) ;" << endl;



		vhdl << tab << declare("Csgn")
				 << " <= C" << of(wE+wF) << ";" << endl;
		vhdl << tab << declare("CexpField", wE)
				 << " <= C" << range(wE+wF-1, wF) << ";" << endl;
		vhdl << tab << declare("CsigField", wF)
				 << " <= C" << range(wF-1, 0) << ";" << endl;
		vhdl << tab << declare("CisNormal") << " <= C" << of(wF);
		for(int i=1; i<wE; i++) 
			vhdl << " or C" << of(wF+i);
		vhdl <<  ";" << endl;
		vhdl << tab << declare("CisInfOrNaN") << " <= C" << of(wF);
		for(int i=1; i<wE; i++) 
			vhdl << " and C" << of(wF+i);
		vhdl <<  ";" << endl;
		vhdl << tab << declare("ChasNonNullSig") << " <= C" << of(0);
		for(int i=1; i<wF; i++) 
			vhdl << " or C" << of(i);
		vhdl <<  ";" << endl;
		vhdl << tab << declare("CisZero") << " <= (not CisNormal) and not ChasNonNullSig;" << endl;
		vhdl << tab << declare("CisInf") << " <= CisInfOrNaN and not ChasNonNullSig;" << endl;
		vhdl << tab << declare("CisNaN") << " <= CisInfOrNaN and ChasNonNullSig;" << endl;

		vhdl << tab << "-- result NaN iff one input is NaN, or 0*inf+x, or +/-(inf-inf)" <<endl;
		vhdl << tab << "-- not tentative: the last case may not happen for finite A and B and inf C, as in this case AB remains finite" <<endl;
		vhdl << tab << declare("RisNaN") << " <= AisNaN or BisNaN or CisNaN or ((AisInf or BisInf) and CisInf and ((Asgn xor Bsgn) xor Csgn));" << endl;
		vhdl << tab << "-- result inf iff either AB or C is inf, and both have the same sign" <<endl;
		vhdl << tab << "-- tentative, AB+C may overflow" <<endl;
		vhdl << tab << declare("tentativeRisInf") << " <= (((AisInf and not (BisZero or BisNaN)) or (BisInf and not (AisZero or AisNaN))) and (not CisNaN) and ((CisInf and not ((Asgn xor Bsgn) xor Csgn)) or not CisInf))" << endl;
		vhdl << tab << tab << "or (CisInf and (not (AisNaN or BisNaN)) and (((AisInf or BisInf) and not ((Asgn xor Bsgn) xor Csgn)) or not (AisInf or BisInf)));" << endl;
			
		vhdl << endl << tab << "-- unbiased exponents make everything simpler but may lead to suboptimal arch " << endl;
		vhdl << tab << declare("Cexp", wE)
				 << " <= CexpField - (\"0\" & " << rangeAssign(wE-2,1,"'1'") << " & CisNormal);" << endl;

		vhdl << tab << declare("effectiveSub") << " <= (negateAB xor Asgn xor Bsgn) xor (negateC xor Csgn);" << endl;


		vhdl << endl << tab << "-- mantissa with the implicit 1 or 0 appended " << endl;
		vhdl << tab << declare("Csig", wF+1) << " <= CisNormal & CsigField ;" << endl;


			
		vhdl << endl << tab << " -- Computation of the exponent difference " << endl;




			
		// Exp difference should be CexpField - AexpField - BexpField + (bias -1) - CisNormal + AisNormal + BisNormal
#if 0 // Cadence infers a very slow adder
			// compute this in a balanced tree of three add+carry as 
			//	(CexpField + (bias-1+AisNormal) + BisNormal)	-	((AexpField+BexpField) + CisNormal) 
		vhdl << tab << declare("leftExpDiffAddend", wE+2) << " <= " 
				 << "(\"00\" & CexpField) + (\"000\" & " << rangeAssign(wE-2,1,"'1'") << " & AisNormal) + BisNormal;" << endl;
		vhdl << tab << declare("rightExpDiffAddend", wE+2) << " <= " 
				 << "(\"00\" & AexpField) + (\"00\" & BexpField) + CisNormal;" << endl;
		vhdl << tab << declare("expDiff", wE+2) << " <= leftExpDiffAddend - rightExpDiffAddend;" << endl;
#else
		vhdl << tab << declare("expDiffPrepare", wE+2) << " <= (\"00\" & AexpField) + (\"00\" & BexpField) - (\"000\" & " << rangeAssign(wE-2,1,"'1'") << " & AisNormal) - BisNormal;" << endl;
		//			vhdl << tab << declare("expDiff", wE+2) << " <= (\"00\" & CexpField) - (\"00\" & AexpField) - (\"00\" & BexpField) + (\"000\" & " << rangeAssign(wE-2,1,"'1'") << " & AisNormal) + BisNormal - CisNormal;" << endl;

		vhdl << tab << declare("expDiff", wE+2) << " <= (\"00\" & CexpField) - expDiffPrepare - CisNormal;" << endl;

#endif


		vhdl << endl << tab << " -- Addend shift datapath " << endl;
		vhdl << tab << "-- Some logic to determine shift distance and tentative result exponent " << endl;

		vhdl << tab << declare("tmpExpComp1", wE+2) << " <= expDiff + \"" << unsignedBinary(mpz_class(2*p), wE+2) << "\";" << endl;
		vhdl << tab << declare("expDiffVerySmall") << " <= tmpExpComp1" << of(wE+1) << ";  -- if expDiff < -2p" << endl;

		vhdl << tab << declare("tmpExpComp2", wE+2) << " <= expDiff - \"" << unsignedBinary(mpz_class(3), wE+2) << "\";" << endl;
		vhdl << tab << declare("expDiffSmall") << " <= tmpExpComp2" << of(wE+1) << ";  -- if expDiff < 3" << endl;

		vhdl << tab << declare("tmpExpComp3", wE+2) << " <= expDiff - \"" << unsignedBinary(mpz_class(p+3), wE+2) << "\";" << endl;
		vhdl << tab << declare("expDiffNotLarge") << " <= tmpExpComp3" << of(wE+1) << ";  -- if expDiff < p+3" << endl;




		int shiftValWidth = intlog2(3*p+4);
		vhdl << tab << declare("ShiftValue", shiftValWidth) << " <= " << endl 
				 << tab << tab << "     \"" << unsignedBinary(mpz_class(3*p+4), shiftValWidth)  << "\" when expDiffVerySmall='1'" << endl 
				 << tab << tab << "else \""<< unsignedBinary(mpz_class(p+3), shiftValWidth) << "\" - (expDiff " << range(shiftValWidth-1, 0) << ") when expDiffNotLarge='1'" << endl 
				 << tab << tab << "else \""<< unsignedBinary(mpz_class(0), shiftValWidth) << "\" ;" << endl;


#if 1
#if 0 // to remove when it works
		Shifter* addendShifter = new Shifter(target, p, 3*p+4, Shifter::Right);
		oplist.push_back(addendShifter);
		inPortMap  (addendShifter, "X", "Csig");
		inPortMap  (addendShifter, "S", "ShiftValue");
		outPortMap (addendShifter, "R","CsigShifted");
		vhdl << instance(addendShifter, "addendShifter");
#endif
		newInstance(
								"Shifter", 
								"RightShifterComponent", 
								"wIn=" + to_string(p) + " maxShift=" + to_string(3*p+4) + " dir=1 " + "wOut=" + to_string(4*p+4) + " computeSticky=0", 
								"X=>Csig,S=>ShiftValue",
								"R=>CsigShifted");

#else
		vhdl << tab << declare("preCsigShifted", 4*p+4) << " <=  Csig & " << rangeAssign(4*p+3, p, "'0'") << ";" << endl;
		vhdl << tab << declare("CsigShifted", 4*p+4) << " <= std_logic_vector(SHR(unsigned(preCsigShifted), unsigned(ShiftValue)));" << endl;
#endif


		vhdl << tab << declare("sticky1") << " <= CsigShifted" << of(0);
		for(int i=1; i<p; i++) 
			vhdl << " or CsigShifted" << of(i);
		vhdl <<  ";" << endl;

		vhdl << tab << declare("CsigShiftedT", 3*p+4) << " <= CsigShifted" << range(4*p+3,p) << ";" << endl;

		vhdl << endl << tab << " -- Product datapath (using naive * operator, may improve in the future)" << endl;
		vhdl << tab << declare("P", 2*p) << " <= Asig * Bsig ;" << endl;
		vhdl << tab << declare("Paligned", 3*p+4) << " <= " << rangeAssign(p+1,0,"'0'") << " & P & \"00\";" << endl;



			
			

		vhdl << endl << tab << " -- The sum at last " << endl;

#if 0
		vhdl << tab << declare("CsigInverted",  3*p+5) << " <= ('0' &CsigShiftedT) when effectiveSub='0'  "<<
			"else ('1' & not CsigShiftedT);"<< endl;

		vhdl << tab << declare("BigSum", 3*p+5) << " <= CsigInverted + ('0' & Paligned) + effectiveSub;  -- effectiveSub is a carry in" << endl;
		vhdl << tab << declare("RsgnTentative") << " <= Asgn xor Bsgn xor negateAB xor BigSum" << of(3*p+4) << ";" << endl;

	
		vhdl << tab << declare("BigSumAbs", 3*p+4) << " <= BigSum" << range(3*p+3,0) << " when BigSum" << of(3*p+4) << "='0'  " << endl 
				 << tab << tab <<	"else (" << rangeAssign(3*p+3, 0, "'0'") << " - BigSum" << range(3*p+3,0) << ") ;  -- TODO Opt: This could probably be a (faster) complementation, with later injection of the carry bit"<< endl;

#elseif 0
		vhdl << tab << declare("BigSum1", 3*p+5) << " <= CsigShiftedT - ('0' & Paligned) when effectiveSub ='1'     else CsigShiftedT + ('0' & Paligned);" << endl;
			
		vhdl << tab << declare("BigSum2", 3*p+5) << " <=  ('0' & Paligned) - CsigShiftedT;" << endl;

		vhdl << tab << declare("RsgnTentative") << " <= Asgn xor Bsgn xor negateAB xor (BigSum2" << of(3*p+4) << " and effectiveSub);" << endl;

		vhdl << tab << declare("BigSumAbs", 3*p+4) << " <= BigSum1" << range(3*p+3,0) << " when (BigSum2" << of(3*p+4) << " or not effectiveSub)='1' else BigSum2" << range(3*p+3,0) << ";" << endl;

#else

		vhdl << tab << declare("CsigInverted",  3*p+5) << " <= ('0' &CsigShiftedT) when effectiveSub='0'  "<<
			"else ('1' & not CsigShiftedT);"<< endl;

		vhdl << tab << declare("BigSum", 3*p+5) << " <= CsigInverted + ('0' & Paligned) + effectiveSub;  -- P +/-CeffectiveSub is a carry in" << endl;
		vhdl << tab << declare("BigSum2", 3*p+5) << " <= CsigShiftedT - ('0' & Paligned);" << endl;

		vhdl << tab << declare("RsgnTentative") << " <= Asgn xor Bsgn xor negateAB xor BigSum" << of(3*p+4) << ";" << endl;
		vhdl << tab << declare("BigSumAbs", 3*p+4) << " <= BigSum" << range(3*p+3,0) << " when (BigSum2" << of(3*p+4) << " or not effectiveSub)='1' else BigSum2" << range(3*p+3,0) << ";" << endl;


#endif


		// A partir de là il n'est pas clair que un seul LZOCShifter ne serait pas plus efficace.

		int LSize=intlog2(2*p+4);
		vhdl << tab << declare("BigSumAbsLowerBits", 2*p+4) << " <= BigSumAbs" << range(2*p+3, 0) << ";" << endl;
#if 1
		newInstance(
								"LZOC", 
								getName()+"LeadingZeroCounter", 
								"wIn=" + to_string(2*p+4) + " countType=0", 
								"I=>BigSumAbsLowerBits",
								"O=>L"
								);
#if 0 // To remove when debugging done
		LZOC *lzc = new LZOC(target, 2*p+4);
		oplist.push_back(lzc);

		inPortMap  (lzc, "I", "BigSumAbsLowerBits");
		inPortMapCst(lzc, "OZB", "'0'");
		outPortMap (lzc, "O", "L"); 
		vhdl << instance(lzc, "lzc");
#endif
#else
		vhdl << tab <<"clz_p : process(BigSumAbsLowerBits)"<< endl;
		vhdl << tab << tab << "begin"<< endl;
		vhdl << tab << tab << declare("L", LSize) << "  <= conv_std_logic_vector(" << 2*p+4 << ", " << LSize << ");" << endl;
		vhdl << tab << tab <<"for j in 0 to " << 2*p+3 << " loop" << endl;
		vhdl << tab << tab <<"  if BigSumAbsLowerBits(j) = '1' then" << endl;
		vhdl << tab << tab <<"    L <= conv_std_logic_vector(" << 2*p+3 << "-j, " << LSize << ");" << endl;
		vhdl << tab << tab <<"  end if;" << endl;
		vhdl << tab << tab <<"end loop;" << endl;
		vhdl << tab << "end process clz_p;" << endl;

#endif			

		// Must test if ea+eb-l+1 >= emin,  so we test 		(AexpPlusBexp + (-(emin-1)))   -L  < 0
		// here emin is -126 = - (1<<(wE-1)) + 2   hence -emin+1 =   1<<(wE-1) +1
		vhdl << tab << declare("tmpExpCompRes1", wE+2) << " <= (AexpPlusBexp" << of(wE) << " & AexpPlusBexp) + \"" << unsignedBinary(mpz_class(1<<(wE-1))+1, wE+2) << "\";" << endl;
		vhdl << tab << declare("tmpExpCompRes2", wE+2) << " <= tmpExpCompRes1 - (" << rangeAssign(wE+1, LSize, "'0'") << " & L);" << endl;
		vhdl << tab << declare("RisSubNormal") << " <= (expDiffSmall or not CisNormal) and  tmpExpCompRes2" << of(wE+1) << ";" << endl; 
		vhdl << tab << declare("RisZero") << " <= expDiffSmall when L=\"" << unsignedBinary(2*p+4, LSize) << "\"  else '0';" << endl;
		vhdl << tab << declare("RisSubNormalOrZero") << " <= RisSubNormal or RisZero;" << endl;
		// TODO the following is for RN only
		vhdl << tab << declare("Rsgn") << " <=      (Asgn xor Bsgn xor negateAB) and (Csgn xor negateC) when ((AisZero or BisZero) and CisZero)='1'  -- negative only for -0 + -0 " << endl;
		vhdl << tab << tab << "else '0' when RisZero='1'  -- covers 1-1 = +0" << endl;
		vhdl << tab << tab << "else RsgnTentative;  -- covers to underflow to zero case" << endl;

		vhdl << tab << declare("shiftValueCaseSubnormal", wE+1) << " <= AexpPlusBexp + \"" << unsignedBinary(mpz_class(p+3 - emin), wE+1)  << "\";" << endl;



		int normShiftValWidth = intlog2(3*p+3);
		vhdl << tab << declare("normShiftValue", normShiftValWidth) << " <= " << endl 
				 << tab << tab << "     L + \"" << unsignedBinary(mpz_class(p+1), normShiftValWidth)  << "\" when (expDiffSmall and not RisSubNormal)='1'" << endl 
				 << tab << tab << "else shiftValueCaseSubnormal" << range(normShiftValWidth -1, 0) << " when (expDiffSmall and RisSubNormal)='1'"  << endl 
				 << tab << tab << "else ShiftValue; -- undo inital shift" << endl;


#if 1
#if 0 // To remove when debugging done
		Shifter* normalizationShifter = new Shifter(target, 3*p+4, 3*p+3, Shifter::Left); 
		oplist.push_back(normalizationShifter);

		inPortMap  (normalizationShifter, "X", "BigSumAbs");
		inPortMap  (normalizationShifter, "S", "normShiftValue");
		outPortMap (normalizationShifter, "R", "BigSumNormd");
		vhdl << endl << instance(normalizationShifter, "NormalizationShifter");
#endif
		newInstance(
								"Shifter", 
								"NormalizationShifter", 
								"wIn=" + to_string(3*p+4) + " maxShift=" + to_string(3*p+3) + " dir=0",
								"X=>BigSumAbs,S=>normShiftValue",
								"R=>BigSumNormd" // output size will be 2*wF+6 TODO: not output unused bits
								); 
#else
		vhdl << tab << declare("preBigSumNormd", 6*p+7) << " <= " << rangeAssign(3*p+2, 0, "'0'") << " & BigSumAbs;" << endl;
		vhdl << tab << declare("BigSumNormd", 6*p+7) << " <= std_logic_vector(SHL(unsigned(preBigSumNormd), unsigned(normShiftValue)));" << endl;
#endif



		vhdl<<tab<< "-- TODO opt: push all the constant additions to exponentUpdate" << endl;
		vhdl << tab << declare("expTentative", wE+2) << " <= " << endl 
				 << tab << tab << "     \"" << unsignedBinary(mpz_class((1 <<(wE+2)) + emin+1), wE+2)  << "\" when RisZero='1'"  << endl 
				 << tab << tab << "else \"" << unsignedBinary(mpz_class((1 <<(wE+2)) + emin), wE+2)  << "\" when RisSubNormal='1'"  << endl 
			//				  << tab << tab << "else \"" << unsignedBinary(mpz_class((1 <<(wE+2)) + emin-1), wE+2)  << "\" when ((not expDiffSmall) and RisSubNormal)='1'"  << endl 
				 << tab << tab << "else (AexpPlusBexp" << of(wE) << " & AexpPlusBexp - (" <<  rangeAssign(wE+1, LSize, "'0'") << " & L))  + \"" << unsignedBinary(mpz_class(3), wE+2)  << "\" when (expDiffSmall and not RisSubNormal)='1'" << endl 
				 << tab << tab << "else (Cexp" << of(wE-1) << " & Cexp" << of(wE-1) << " & Cexp) + \"" << unsignedBinary(mpz_class(1), wE+2)  << "\" ;" << endl;


		vhdl << tab << declare("sticky2") << " <= BigSumNormd" << of(0);
		for(int i=1; i<2*p+2; i++) 
			vhdl << " or BigSumNormd" << of(i);
		vhdl <<  ";" << endl << endl;

		vhdl << tab << declare("fracTentative",  p+3) << " <= BigSumNormd" << range(3*p+4, 2*p+2)<< ";" << endl;
	


		vhdl << endl << tab << " -- Last 2-bit normalization " << endl;

		// The leading one may be at position wF+3, wF+2 or wF+1
		// 
		vhdl<<tab<< declare("fracLeadingBitsNormal", 2) << " <=  fracTentative("<<wF+3<<" downto "<<wF+2<<") ;" << endl;
		// No final norm if result is subnormal
		vhdl<<tab<< declare("fracLeadingBits", 2) << " <= \"01\" when RisSubNormal='1' else  fracLeadingBitsNormal;" << endl;


		vhdl<<tab<< declare("fracResultNormd",wF) << " <=" << endl ;
		vhdl<<tab<<tab<< "     fracTentative("<<wF+0<<" downto 1)  when fracLeadingBits = \"00\" "<<endl;
		vhdl<<tab<<tab<< "else fracTentative("<<wF+1<<" downto 2)  when fracLeadingBits = \"01\" "<<endl;
		vhdl<<tab<<tab<< "else fracTentative("<<wF+2<<" downto 3);"<<endl;

		vhdl<<tab<< declare("fracResultRoundBit") << " <=" << endl ;
		vhdl<<tab<<tab<< "     fracTentative(0) 	 when fracLeadingBits = \"00\" "<<endl;
		vhdl<<tab<<tab<< "else fracTentative(1)    when fracLeadingBits = \"01\" "<<endl;
		vhdl<<tab<<tab<< "else fracTentative(2) ;"<<endl;
			
		vhdl<<tab<< declare("fracResultStickyBit") << " <=" << endl ;
		vhdl<<tab<<tab<< "     sticky1 or sticky2	 when fracLeadingBits = \"00\" "<<endl;
		vhdl<<tab<<tab<< "else fracTentative(0) or sticky1 or sticky2    when fracLeadingBits = \"01\" "<<endl;
		vhdl<<tab<<tab<< "else fracTentative(1) or fracTentative(0) or  sticky1 or sticky2;"<<endl;
		// round bit
		vhdl<<tab<< declare("round") <<" <= fracResultRoundBit and (fracResultStickyBit or fracResultNormd(0));"<<endl;


		// in addend-anchored case,  exponent must be adjusted after normalization		
		vhdl<<tab<<declare("expUpdate",wE+2) << " <= \"" << unsignedBinary(bias-2, wE+2)  << "\" when RisZero = '1'       -- bias - 2"<<endl;
		vhdl<<tab                   <<"      else   \""  << unsignedBinary(bias-2, wE+2)  << "\" when fracLeadingBits = \"00\" -- bias - 2"<<endl;
		vhdl<<tab                   <<"      else   \"" << unsignedBinary(bias-1, wE+2)  << "\" when fracLeadingBits = \"01\" -- bias - 1 "<<endl;
		vhdl<<tab                   <<"      else   \"" << unsignedBinary(bias, wE+2)  << "\";                            -- bias "<<endl;
		vhdl<<tab<<declare("exponentResult1",wE+2) << " <= expTentative + expUpdate;" << endl;
		// End of normalization stage
		vhdl<<tab<<declare("resultBeforeRound",wE+1 + wF+1) << " <= "
				<< "exponentResult1 & fracResultNormd;" << endl;		

		vhdl << tab << declare("resultRounded", wE+wF+2) << " <= resultBeforeRound + (" <<rangeAssign(wE+wF+1, 1, "'0'") << " & round);" << endl;
		vhdl << tab << declare("Roverflowed") << " <= resultRounded" << of(wE+wF+1) << " or resultRounded" << of(wE+wF) << " or (resultRounded" << of(wE+wF-1);
		for(int i=2; i<= wE; i++) 
			vhdl << " and resultRounded" << of(wE+wF-i);
		vhdl <<  ");" << endl;
			
		vhdl << tab << declare("finalRisInf") << " <= tentativeRisInf or Roverflowed; " << endl;

		vhdl << tab << declare("Inf", wE+wF) << " <= " << rangeAssign(wE+wF-1, wF, "'1'")  << " & " << rangeAssign(wF-1, 0, "'0'") << ";" << endl;
		vhdl << tab << declare("NaN", wE+wF) << " <= " << rangeAssign(wE+wF-1, wF, "'1'")  << " & " << rangeAssign(wF-1, 0, "'1'") << ";" << endl;
			

		vhdl << tab << "R <= " << endl 
				 << tab << tab << "     Rsgn & Inf when ((not RisNaN) and finalRisInf)='1'" << endl
				 << tab << tab << "else '0'  & NaN when RisNaN='1'" << endl
				 << tab << tab << "else Rsgn & resultRounded" <<range(wE+wF-1, 0) << ";" << endl;

	}

	IEEEFMA::~IEEEFMA() {
	}










	void IEEEFMA::emulate(TestCase * tc)
	{
		/* Get I/O values */
		mpz_class svA = tc->getInputValue("A");
		mpz_class svB = tc->getInputValue("B");
		mpz_class svC = tc->getInputValue("C");
		mpz_class svnegateAB = tc->getInputValue("negateAB");
		mpz_class svnegateC = tc->getInputValue("negateC");
		
		IEEENumber fpA(wE, wF), fpB(wE, wF), fpC(wE, wF);
		fpA = svA;
		fpB = svB;
		fpC = svC;
		mpfr_t a, b, c, r;
		mpfr_init2(a, 1+wF);
		mpfr_init2(b, 1+wF);
		mpfr_init2(c, 1+wF);
		mpfr_init2(r, 1+wF); 
		fpA.getMPFR(a);
		fpB.getMPFR(b);
		fpC.getMPFR(c);

		mpfr_set_default_prec (wF+1);
		mpfr_set_emin ( -(1<<(wE-1)) + 3 -wF); // in the MPFR doc, they suggest 1073 for double precision
		// 1073=-1024+3-52
		mpfr_set_emax (1<<(wE-1)); // in the MPFR doc, they suggest 1024 for double precision


		if (1==svnegateAB) { 
			mpfr_neg(a, a, GMP_RNDN);
		}
		if (1==svnegateC) { 
			mpfr_neg(c, c, GMP_RNDN);
		}

		int i = mpfr_fma(r, a, b, c, GMP_RNDN); // i remembers if the rounding was up, down or if the result was exact
		mpfr_subnormalize (r, i, GMP_RNDN);

		// Set outputs 
		IEEENumber  fpR(wE, wF, r);
		mpz_class svR = fpR.getSignalValue();
		tc->addExpectedOutput("R", svR);

#if 0
		double da, db,dc,dr;
		da=mpfr_get_d(a, GMP_RNDN);
		db=mpfr_get_d(b, GMP_RNDN);
		dc=mpfr_get_d(c, GMP_RNDN);
		dr=mpfr_get_d(r, GMP_RNDN);
		cout << "a=" << da << " b="<< db << " c=" << dc << "   r=" << dr << endl;
#endif

		// clean up
		mpfr_clears(a,b,c,r, NULL);
	}
	



	void IEEEFMA::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;

		// Tests with signed zeros
		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::plusZero);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::plusZero);
		tc->addComment("largest subnormal  + 0");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::minusZero);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::plusZero);
		tc->addComment("largest subnormal  + 0");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::plusZero);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::minusZero);
		tc->addComment("largest subnormal  + 0");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::minusZero);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::minusZero);
		tc->addComment("largest subnormal  + 0");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		// the same but with subtraction
		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::plusZero);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::plusZero);
		tc->addComment("largest subnormal  + 0");
		tc->addInput("negateAB", 1);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::minusZero);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::plusZero);
		tc->addComment("largest subnormal  + 0");
		tc->addInput("negateAB", 1);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::plusZero);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::minusZero);
		tc->addComment("largest subnormal  + 0");
		tc->addInput("negateAB", 1);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::minusZero);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::minusZero);
		tc->addComment("largest subnormal  + 0");
		tc->addInput("negateAB", 1);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		// the same but with the other subtraction
		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::plusZero);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::plusZero);
		tc->addComment("largest subnormal  + 0");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 1);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::minusZero);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::plusZero);
		tc->addComment("largest subnormal  + 0");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 1);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::plusZero);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::minusZero);
		tc->addComment("largest subnormal  + 0");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 1);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::minusZero);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::minusZero);
		tc->addComment("largest subnormal  + 0");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 1);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		// Tests with subnormals
			
		tc = new TestCase(this); 
		tc->addInput("A", (mpz_class(1)<<wF) -1);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::plusZero);
		tc->addComment("largest subnormal  + 0");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);
			
		tc = new TestCase(this); 
		tc->addInput("A", (mpz_class(1)<<wF));
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::plusZero);
		tc->addComment("smallest normal + 0");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);
			
			
		tc = new TestCase(this); 
		tc->addInput("A", mpz_class(1));
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::plusZero);
		tc->addComment("smallest subnormal  + 0");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addInput("A", mpz_class(1));
		tc->addInput("B", mpz_class(1));
		tc->addInput("C", mpz_class(1));
		tc->addComment("smallest subnormal * smallest subnormal + smallest subnormal");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addInput("A", mpz_class(1));
		tc->addIEEEInput("B", 1.0);
		tc->addInput("C", mpz_class(1));
		tc->addComment("smallest subnormal * 1.0 + smallest subnormal");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addInput("A", mpz_class(1));
		tc->addIEEEInput("B", -1.0);
		tc->addInput("C", mpz_class(1));
		tc->addComment("smallest subnormal * -1.0 + smallest subnormal");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);


		// Stupid tests 
		tc = new TestCase(this); 
		tc->addIEEEInput("A", 1.0);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", IEEENumber::plusZero);
		tc->addComment("1*1+0");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", 1.0);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", -2.0);
		tc->addComment("1*1+ -2");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", 1.0);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", -1024.0);
		tc->addComment("1*1+ -1024");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", 1.0);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", 1024.0);
		tc->addComment("1*1+ 1024");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", 1.0);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", - (8192.0*4096.0));
		tc->addComment("1*1+ -2^25");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", 1.0);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C",  (8192.0*4096.0));
		tc->addComment("1*1+ 2^25");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::plusZero);
		tc->addIEEEInput("B", -1.0);
		tc->addIEEEInput("C", 1.0);
		tc->addComment("+0*-1 + 1");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", 1.0);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", 1.0);
		tc->addComment("1*1+1");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", 1.0);
		tc->addIEEEInput("B", -1.0);
		tc->addIEEEInput("C", 1.0);
		tc->addComment("1*-1 + 1");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", 1.0);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", -1.0);
		tc->addComment("1*1 + -1");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		//A bit of sign arithmetic
		tc = new TestCase(this); 
		tc->addIEEEInput("A", -1.0);
		tc->addIEEEInput("B", 1.0);
		tc->addIEEEInput("C", 0.0);
		tc->addComment("-1*1 + 0");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", -0.5);
		tc->addIEEEInput("B", 82);
		tc->addIEEEInput("C", 42.0);
		tc->addComment("-0.5*82 + 42 -- a cancellation");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", 17.0);
		tc->addIEEEInput("B", 42.0);
		tc->addIEEEInput("C", -713.0);
		tc->addComment("17*42 + -713 -- p cancellation");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", (double)((1<<wF)-1) / (double)(1<<wF));
		tc->addIEEEInput("B", (double)((1<<wF)-1) / (double)(1<<wF));
		tc->addIEEEInput("C", (double)((1<<wF)-1) / (double)(1<<wF));
		tc->addComment("worst-case p overflow");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);



		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::plusInfty);
		tc->addIEEEInput("B", IEEENumber::plusInfty);
		tc->addIEEEInput("C", IEEENumber::plusInfty);
		tc->addComment("+inf*+inf + +inf");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addIEEEInput("A", IEEENumber::plusInfty);
		tc->addIEEEInput("B", IEEENumber::minusInfty);
		tc->addIEEEInput("C", IEEENumber::plusInfty);
		tc->addComment("+inf*-inf + +inf");
		tc->addInput("negateAB", 0);
		tc->addInput("negateC", 0);
		tc->addInput("RndMode", 0);
		emulate(tc);
		tcl->add(tc);


	}




 
	void IEEEFMA::buildRandomTestCases(TestCaseList* tcl, int n){
		TestCase *tc;
		mpz_class a,b,c, ea, eb, ec, d, negateAB, negateC,rndmode;
		mpz_class negative  = mpz_class(1)<<(wE+wF);
		mpz_class bias = (mpz_class(1)<<(wE-1)) - 1;

		for (int i = 0; i < n; i++) {
			tc = new TestCase(this); 

			/* Fill inputs */
			switch(i&2) {
			case 0: 

				// Marche pas bien
				tc->addComment("ea+eb close to ec");
				ea = getLargeRandom(wE);
				eb = getLargeRandom(wE);
				d = getLargeRandom(3)-4;
				ec = ea+eb+d - bias;
				a = getLargeRandom(wF) + (ea << wF);
				b = getLargeRandom(wF) + (eb << wF);
				c = getLargeRandom(wF) + (ec << wF);
				break;
#if 0
			case 1:
				tc->addComment("ea+eb close to ec, a subnormal");
				ea = 0;
				eb = getLargeRandom(wE-1);
				d = getLargeRandom(3)-4;
				ec = ea+eb+d - bias;
				a = getLargeRandom(wF) + (ea << wF);
				b = getLargeRandom(wF) + (eb << wF);
				c = getLargeRandom(wF) + (ec << wF);
				break;
			case 2:
				tc->addComment("ea+eb close to ec, c subnormal");
				ec = 0;
				eb = getLargeRandom(wE-1);
				d = getLargeRandom(3)-4;
				ea = ec-eb-d+bias;
				a = getLargeRandom(wF) + (ea << wF);
				b = getLargeRandom(wF) + (eb << wF);
				c = getLargeRandom(wF) + (ec << wF);
				break;
			case 3:
				break;
			case 4:
				break;
			case 5:
				break;
			case 6:
				break;
			case 7: 
				break;
#endif
			default:
				tc->addComment("fully random");
				a = getLargeRandom(wE+wF+1);
				b = getLargeRandom(wE+wF+1);
				c = getLargeRandom(wE+wF+1);
				negateAB = getLargeRandom(1);
				negateC = getLargeRandom(1);
				rndmode = getLargeRandom(2);
				break;
			}
			tc->addInput("A", a);
			tc->addInput("B", b);
			tc->addInput("C", c);
			tc->addInput("negateAB", negateAB);
			tc->addInput("negateC", negateC);
			tc->addInput("RndMode", rndmode);

			/* Get correct outputs */
			emulate(tc);
			tcl->add(tc);
		}
	}

	OperatorPtr IEEEFMA::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int wE, wF;
		UserInterface::parseStrictlyPositiveInt(args, "wE", &wE); 
		UserInterface::parseStrictlyPositiveInt(args, "wF", &wF);
		return new IEEEFMA(parentOp, target, wE, wF);
	}

	void IEEEFMA::registerFactory(){
		UserInterface::add("IEEEFMA", // name
											 "A correctly rounded floating-point FMA.",
											 "BasicFloatingPoint",
											 "", //seeAlso
											 "wE(int): exponent size in bits; \
			wF(int): mantissa size in bits;",
											 "",
											 IEEEFMA::parseArguments
											 ) ;
	}


}



