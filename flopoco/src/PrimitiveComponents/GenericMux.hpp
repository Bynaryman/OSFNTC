#ifndef GENERICMUX_H
#define GENERICMUX_H

#include "Operator.hpp"
#include "utils.hpp"

namespace flopoco {

///
/// \brief The GenericMux class. A junction for primitive implementations.
///
/// The GenericMux desides on the specific Target which primitive implementation to take.
///
    class GenericMux : public Operator {

    public:
        ///
        /// \brief GenericMux constructor
        /// \param target The current Target
        /// \param wIn The wordsize of the generated MUX
        /// \param inputCount The number of inputs of the generated MUX
        ///
        GenericMux(Operator* parentOp, Target* target, const uint32_t &wIn, const uint32_t &inputCount );

        ///
        /// \brief getSelectName returns the name of the select signal
        /// \return Name of the select signal
        ///
        std::string getSelectName() const;

        ///
        /// \brief getInputName returns the name of the input signal at the specific index
        /// \param index which input to return
        /// \return Name of the input signal at the specific index
        ///
        std::string getInputName(const uint32_t &index) const;

        ///
        /// \brief getOutputName returns the name of the output signal
        /// \return
        ///
        std::string getOutputName() const;

        ~GenericMux() {}

		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		static void registerFactory();
    private:
        ///
        /// \brief buildXilinx builds the Xilinx implementation of the primitive MUX
        /// \param target The current Target
        /// \param wIn The wordsize of the generated MUX
        /// \param inputCount The number of inputs of the generated MUX
        ///
        void buildXilinx(Operator *parentOp, Target* target, const uint32_t &wIn, const uint32_t &inputCount);

        ///
        /// \brief buildAltera builds the Altera implementation of the primitive MUX
        /// \param target The current Target
        /// \param wIn The wordsize of the generated MUX
        /// \param inputCount The number of inputs of the generated MUX
        ///
        void buildAltera(Operator *parentOp, Target* target, const uint32_t &wIn, const uint32_t &inputCount);

        ///
        /// \brief buildCommon builds the VHDL-code implementation of the primitive MUX
        /// \param target The current Target
        /// \param wIn The wordsize of the generated MUX
        /// \param inputCount The number of inputs of the generated MUX
        ///
        void buildCommon(Target* target, const uint32_t &wIn, const uint32_t &inputCount);
	};
}//namespace

#endif
