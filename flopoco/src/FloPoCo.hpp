#ifndef FLOPOCO_HPP
#define FLOPOCO_HPP

// TODO: I guess we should at some point copy here only the public part of each class,
// to provide a single self-contained include file.

// support the autotools-generated config.h
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Operator.hpp"
#include "UserInterface.hpp"
#include "FlopocoStream.hpp"

//#include "Instance.hpp"

#include "Table.hpp"

/* operator pipeline work* ------------------------------------ */
#include "OperatorPipeline/OperatorPipeline.hpp"

/* resource estimation ---------------------------------------- */
// #include "Tools/ResourceEstimationHelper.hpp"
/* resource estimation ---------------------------------------- */



/* floorplanning ---------------------------------------------- */
#include "Tools/ResourceEstimationHelper.hpp"
/* floorplanning ---------------------------------------------- */


#include "TestBenches/TestBench.hpp"

#include "BitHeap/Bit.hpp"
#include "BitHeap/Compressor.hpp"
#include "BitHeap/BitHeap.hpp"
#include "BitHeap/CompressionStrategy.hpp"
#include "BitHeap/BitHeapPlotter.hpp"

#include "BitHeap/BitHeapTest.hpp"

/* shifters + lzoc ------------------------------------------- */
#include "ShiftersEtc/Shifters.hpp"
#include "ShiftersEtc/LZOC.hpp"
#include "ShiftersEtc/LZOC3.hpp"
#include "ShiftersEtc/LZOCShifterSticky.hpp"

/* FixFilters ------------------------------------------------ */
#include "FixFilters/FixSOPC.hpp"
#include "FixFilters/FixFIR.hpp"
#include "FixFilters/FixHalfSine.hpp"
#include "FixFilters/FixIIR.hpp"

#include "ShiftReg.hpp"

/* regular pipelined integer adder/ adder+subtracter --------- */
#include "IntAddSubCmp/IntAdder.hpp" // includes several other .hpp
#include "IntAddSubCmp/IntDualAddSub.hpp"
//#include "IntAddSubCmp/IntComparator.hpp"
#include "IntAddSubCmp/IntMultiAdder.hpp"

// #include "IntAddSubCmp/IntAdderClassical.hpp"
// #include "IntAddSubCmp/IntAdderAlternative.hpp"
// #include "IntAddSubCmp/IntAdderShortLatency.hpp"

/* fast large adders ----------------------------------------- */
// #include "IntAddSubCmp/LongIntAdderAddAddMuxGen1.hpp"
// #include "IntAddSubCmp/LongIntAdderCmpCmpAddGen1.hpp"
// #include "IntAddSubCmp/LongIntAdderCmpAddIncGen1.hpp"
// #include "IntAddSubCmp/IntAdderSpecific.hpp"
// #include "IntAddSubCmp/LongIntAdderAddAddMuxGen2.hpp"
// #include "IntAddSubCmp/LongIntAdderCmpCmpAddGen2.hpp"
// #include "IntAddSubCmp/LongIntAdderCmpAddIncGen2.hpp"
// #include "IntAddSubCmp/IntComparatorSpecific.hpp"
// #include "IntAddSubCmp/LongIntAdderMuxNetwork.hpp"

/* Integer and fixed-point multipliers ------------------------ */
#include "IntMult/IntMultiplier.hpp"
#include "IntMult/IntMultiplierLUT.hpp"
// #include "IntMult/FixMultAdd.hpp"
// #include "IntMult/IntKaratsuba.hpp"
// #include "IntMult/IntSquarer.hpp"
// #include "IntMult/GenericBinaryPolynomial.hpp"
// #include "IntMult/IntPower.hpp"
// #include "IntMult/IntKaratsubaRectangular.hpp"
#include "IntMult/DSPBlock.hpp"
/* Floating-point adder variants ----------------------------- */
#include "FPAddSub/FPAdd.hpp"
#include "FPAddSub/FPAddDualPath.hpp"
#include "FPAddSub/FPAddSinglePath.hpp"
#include "FPAddSub/FPAddSinglePathIEEE.hpp"
// #include "FPAddSub/FPAdd3Input.hpp"
// #include "FPAddSub/FPAddSub.hpp"

/* Floating-point multiplier variants-------------------------- */
#include "FPMultSquare/FPMult.hpp"
//#include "FPMultKaratsuba.hpp" // Resurrect some day?
//#include "FPMultSquare/FPSquare.hpp"

#include "FPDivSqrt/FPDiv.hpp"
#include "FPDivSqrt/FPSqrt.hpp"
//#include "FPDivSqrt/FPSqrtPoly.hpp" // Resurrect some day?


/* Constant multipliers and dividers ------------------------ */
// #include "ConstMult/IntConstMult.hpp"
//#include "ConstMult/IntConstMCM.hpp"
//#include "ConstMult/IntIntKCM.hpp"
#include "ConstMult/FixRealKCM.hpp"
//#include "ConstMult/FPConstMult.hpp"
//#include "ConstMult/CRFPConstMult.hpp"
//#include "ConstMult/FPRealKCM.hpp"

//#include "ConstMult/IntConstDiv.hpp" // depends on intconstmut
//#include "ConstMult/FPConstDiv.hpp"

/* FP composite operators */
// #include "FPComposite/FPLargeAcc.hpp"
// #include "FPComposite/LargeAccToFP.hpp"
// #include "FPComposite/FPDotProduct.hpp"


/* Fixed-point function generators ---------------------*/

#include "FixFunctions/FixFunction.hpp"
#include "FixFunctions/BasicPolyApprox.hpp"
#include "FixFunctions/FixFunctionByTable.hpp"
//#include "FixFunctions/PiecewisePolyApprox.hpp"
//#include "FixFunctions/FixFunctionBySimplePoly.hpp"
//#include "FixFunctions/FixFunctionByPiecewisePoly.hpp"

#include "FixFunctions/GenericTable.hpp"
//#include "FixFunctions/BipartiteTable.hpp"
// #include "FixFunctions/FixFunctionByMultipartiteTable.hpp"

/*  Various elementary functions in fixed or floating point*/
//#include "Trigs/FixSinCos.hpp"
#include "Trigs/CordicSinCos.hpp"
// #include "Trigs/FixAtan2.hpp"
// #include "Trigs/FixAtan2ByCORDIC.hpp"
//#include "Trigs/FixAtan2ByRecipMultAtan.hpp"
//#include "Trigs/FixAtan2ByBivariateApprox.hpp"
//#include "Trigs/Fix2DNorm.hpp"
// #include "Trigs/FixSinOrCos.hpp"  Replug when poly eval fixed
#include "ExpLog/IterativeLog.hpp"
//#include "ExpLog/FPExp.hpp"
//#include "ExpLog/FPPow.hpp"


#include "Conversions/Posit2FP.hpp"
//#include "Conversions/Fix2FP.hpp"
//#include "Conversions/FP2Fix.hpp"
//#include "Conversions/InputIEEE.hpp"
//#include "Conversions/OutputIEEE.hpp"

// AutoTest
#include "AutoTest/AutoTest.hpp"



/* misc ------------------------------------------------------ */
#include "TestBenches/Wrapper.hpp"
#include "TutorialOperator.hpp"



/* Complex arithmetic */
#include "Complex/FixComplexKCM.hpp"

#if 0
// Old stuff removed from older versions, some of which to bring back to life
#include "Complex/FixedComplexAdder.hpp"
#include "Complex/FixedComplexMultiplier.hpp"



/* multioperand adders --------------------------------------- */
#include "IntMultiAdder.hpp"
#include "IntAddSubCmp/IntNAdder.hpp"
#include "IntAddSubCmp/IntCompressorTree.hpp"
#include "IntAddSubCmp/PopCount.hpp"
#include "IntAddSubCmp/BasicCompressor.hpp"
#include "IntAddSubCmp/NewCompressorTree.hpp"
#include "FP2DNorm.hpp"
#include "FPSqrtPoly.hpp"
/* applications ---------------------------------------------- */

/* Coil Inductance application */
//#include "Apps/CoilInductance/CoordinatesTableX.hpp"
//#include "Apps/CoilInductance/CoordinatesTableZ.hpp"
//#include "Apps/CoilInductance/CoordinatesTableY.hpp"
//#include "Apps/CoilInductance/CoilInductance.hpp"

/* fast evaluation of the possible intrusion of a point within a
spheric enclosure -------------------------------------------- */
#include "Apps/Collision.hpp"

/* a floating-point fused multiply-accumulate operator for the
use withing matrix-multiplication scenarios ------------------ */
#include "Apps/FPFMAcc.hpp"

/* a 1D Jacobi computation kernel ---------------------------- */
#include "Apps/FPJacobi.hpp"

/* logarithmic number system  -------------------------------- */
#include "LNS/LNSAddSub.hpp"
#include "LNS/LNSAdd.hpp"
#include "LNS/CotranTables.hpp"
#include "LNS/Cotran.hpp"
#include "LNS/CotranHybrid.hpp"
#include "LNS/LNSMul.hpp"
#include "LNS/LNSDiv.hpp"
#include "LNS/LNSSqrt.hpp"
#include "LNS/AtanPow.hpp"
#include "LNS/LogSinCos.hpp"
#endif // 0

/* Primitive components -------------------------------- */
#include "PrimitiveComponents/GenericAddSub.hpp"
#include "PrimitiveComponents/GenericMult.hpp"
#include "PrimitiveComponents/GenericMux.hpp"
#include "PrimitiveComponents/GenericLut.hpp"
#include "PrimitiveComponents/Xilinx/Xilinx_GenericAddSub.hpp"
#include "PrimitiveComponents/Xilinx/Xilinx_GenericMux.hpp"
#include "PrimitiveComponents/Xilinx/Xilinx_Comparator.hpp"
#include "PrimitiveComponents/Xilinx/Xilinx_TernaryAdd_2State.hpp"
#include "PrimitiveComponents/Xilinx/XilinxGPC.hpp"
#include "PrimitiveComponents/Xilinx/XilinxFourToTwoCompressor.hpp"

#include "ConstMultPAG/ConstMultPAG.hpp"

/*Internshiping shit making*/
#include "Internshiping/MAC.hpp"


#endif //FLOPOCO_HPP
