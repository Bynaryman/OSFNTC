#ifndef GENERICADDSUB_H
#define GENERICADDSUB_H

#include "Operator.hpp"
#include "utils.hpp"

namespace flopoco {
///
/// \brief The GenericMult class. A junction for primitive implementations.
///
/// The GenericMult desides on the specific Target which primitive implementation to take.
///
    class GenericMult : public Operator {
    public:

    public:
        GenericMult(Target* target, const int &wX, const int &wY);

		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);

    protected:
        ///
        /// \brief buildXilinx builds the Xilinx implementation of the primitive adder
        /// \param target The current Target
        /// \param wIn The input wordsize
        ///
        void buildXilinx(Target* target, const uint32_t &wIn);

        ///
        /// \brief buildAltera builds the Altera implementation of the primitive adder
        /// \param target The current Target
        /// \param wIn The input wordsize
        ///
        void buildAltera(Target* target, const uint32_t &wIn);

        ///
        /// \brief buildCommon builds the VHDL-code implementation of the primitive adder
        /// \param target The current Target
        /// \param wIn The input wordsize
        ///
        void buildCommon(Target* target, const uint32_t &wIn);
    private:
        int wX, wY, wR;
	};
}//namespace

#endif
