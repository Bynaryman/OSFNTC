/*
  THIS FILE IS AUTOMATICALLY GENERATED FROM src/Factories/Interfaced.txt! DON'T MODIFY DIRECTLY!
  
  the FloPoCo command-line interface

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Authors : Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr
			Bogdan Pasca, Bogdan.Pasca@ens-lyon.org

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA-Lyon
  2008-2014.
  All rights reserved.

*/
#include "UserInterface.hpp"

#define DEF(op)	\
class op {	\
public:\
	static void registerFactory();\
}

namespace flopoco{
	DEF(AutoTest);
	DEF(Compressor);
	DEF(Shifter);
	DEF(LZOC);
	DEF(LZOC3);
	DEF(LZOCShifterSticky);
	DEF(ShiftReg);
	DEF(IntAdder);
	DEF(IntDualAddSub);
	DEF(IntMultiAdder);
	DEF(IntConstMult);
	DEF(FPConstMult);
	DEF(IntConstDiv);
	DEF(DSPBlock);
	DEF(IntMultiplier);
	DEF(IntMultiplierLUT);
	DEF(IntKaratsubaRectangular);
	DEF(FixFunctionByTable);
	DEF(FixFunctionByMultipartiteTable);
	DEF(FixFunctionBySimplePoly);
	DEF(FixFunctionByPiecewisePoly);
	DEF(PiecewisePolyApprox);
	DEF(FixRealKCM);
	DEF(FixRealShiftAdd);
	DEF(FixRealConstMult);
	DEF(FixFixConstMult);
	DEF(GenericMux);
	DEF(GenericLut);
	DEF(FPAdd);
	DEF(FPDiv);
	DEF(FPSqrt);
	DEF(FPExp);
	DEF(FP2Fix);
	DEF(FPMult);
	DEF(OutputIEEE);
	DEF(IEEEAdd);
	DEF(IEEEFMA);
	DEF(FixSOPC);
	DEF(FixFIR);
	DEF(FixHalfSine);
	DEF(FixRootRaisedCosine);
	DEF(FixIIR);
	DEF(Posit2FP);
	DEF(FixSinCos);
	DEF(Xilinx_GenericAddSub);
	DEF(Xilinx_Comparator);
	DEF(Xilinx_TernaryAdd_2State);
	DEF(XilinxGPC);
	DEF(XilinxFourToTwoCompressor);
	DEF(TutorialOperator);
	DEF(BaseMultiplierDSPSuperTilesXilinx);
	DEF(BaseMultiplierXilinx2xk);
	DEF(BaseMultiplierIrregularLUTXilinx);
	DEF(BaseMultiplierDSPKaratsuba);
	DEF(TestBench);
	DEF(Wrapper);
	DEF(FixComplexKCM);
	DEF(FixComplexAdder);
	DEF(FixComplexR2Butterfly);
	DEF(FixFFTFullyPA);
	DEF(IntConstMultShiftAdd);
	DEF(IntConstMultShiftAddOpt);
	DEF(IntConstMultShiftAddRPAG);
	DEF(IntConstMultShiftAddOptTernary);
	DEF(PIF2Posit);
	DEF(Posit2PIF);
	DEF(PIFAdd);
	DEF(PositAdd);
	DEF(PIF2Fix);
	DEF(PositExp);
	DEF(PositFunctionByTable);
	DEF(Posit2Posit);
	DEF(FixFunctionByVaryingPiecewisePoly);
	DEF(PositMult);
	DEF(Posit2PD);
	DEF(PDFDP);
	DEF(PE);
	DEF(SystolicArrayKernel);
	DEF(SystolicArray);
	DEF(Quire2Posit);
	DEF(IEEE_to_S3);
	DEF(Posit_to_S3);
	DEF(S3FDP);
	DEF(PE_S3);
	DEF(LAICPT2_to_arith);
	void UserInterface::registerFactories()
	{
		try {
			AutoTest::registerFactory();
			Compressor::registerFactory();
			Shifter::registerFactory();
			LZOC::registerFactory();
			LZOC3::registerFactory();
			LZOCShifterSticky::registerFactory();
			ShiftReg::registerFactory();
			IntAdder::registerFactory();
			IntDualAddSub::registerFactory();
			IntMultiAdder::registerFactory();
			IntConstMult::registerFactory();
			FPConstMult::registerFactory();
			IntConstDiv::registerFactory();
			DSPBlock::registerFactory();
			IntMultiplier::registerFactory();
			IntMultiplierLUT::registerFactory();
			IntKaratsubaRectangular::registerFactory();
			FixFunctionByTable::registerFactory();
			FixFunctionByMultipartiteTable::registerFactory();
			FixFunctionBySimplePoly::registerFactory();
			FixFunctionByPiecewisePoly::registerFactory();
			PiecewisePolyApprox::registerFactory();
			FixRealKCM::registerFactory();
			FixRealShiftAdd::registerFactory();
			FixRealConstMult::registerFactory();
			FixFixConstMult::registerFactory();
			GenericMux::registerFactory();
			GenericLut::registerFactory();
			FPAdd::registerFactory();
			FPDiv::registerFactory();
			FPSqrt::registerFactory();
			FPExp::registerFactory();
			FP2Fix::registerFactory();
			FPMult::registerFactory();
			OutputIEEE::registerFactory();
			IEEEAdd::registerFactory();
			IEEEFMA::registerFactory();
			FixSOPC::registerFactory();
			FixFIR::registerFactory();
			FixHalfSine::registerFactory();
			FixRootRaisedCosine::registerFactory();
			FixIIR::registerFactory();
			Posit2FP::registerFactory();
			FixSinCos::registerFactory();
			Xilinx_GenericAddSub::registerFactory();
			Xilinx_Comparator::registerFactory();
			Xilinx_TernaryAdd_2State::registerFactory();
			XilinxGPC::registerFactory();
			XilinxFourToTwoCompressor::registerFactory();
			TutorialOperator::registerFactory();
			BaseMultiplierDSPSuperTilesXilinx::registerFactory();
			BaseMultiplierXilinx2xk::registerFactory();
			BaseMultiplierIrregularLUTXilinx::registerFactory();
			BaseMultiplierDSPKaratsuba::registerFactory();
			TestBench::registerFactory();
			Wrapper::registerFactory();
			FixComplexKCM::registerFactory();
			FixComplexAdder::registerFactory();
			FixComplexR2Butterfly::registerFactory();
			FixFFTFullyPA::registerFactory();
			IntConstMultShiftAdd::registerFactory();
			IntConstMultShiftAddOpt::registerFactory();
			IntConstMultShiftAddRPAG::registerFactory();
			IntConstMultShiftAddOptTernary::registerFactory();
			PIF2Posit::registerFactory();
			Posit2PIF::registerFactory();
			PIFAdd::registerFactory();
			PositAdd::registerFactory();
			PIF2Fix::registerFactory();
			PositExp::registerFactory();
			PositFunctionByTable::registerFactory();
			Posit2Posit::registerFactory();
			FixFunctionByVaryingPiecewisePoly::registerFactory();
			PositMult::registerFactory();
			Posit2PD::registerFactory();
			PDFDP::registerFactory();
			PE::registerFactory();
			SystolicArrayKernel::registerFactory();
			SystolicArray::registerFactory();
			Quire2Posit::registerFactory();
			IEEE_to_S3::registerFactory();
			Posit_to_S3::registerFactory();
			S3FDP::registerFactory();
			PE_S3::registerFactory();
			LAICPT2_to_arith::registerFactory();
		} catch (const std::exception &e) {
			cerr << "Error while registering factories: " << e.what() <<endl;
			exit(EXIT_FAILURE);
		}
	}
}