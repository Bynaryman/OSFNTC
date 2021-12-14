#ifndef GENERICADDSUB_H
#define GENERICADDSUB_H

#include "Operator.hpp"
#include "utils.hpp"

namespace flopoco {
///
/// \brief The GenericAddSub class. A junction for primitive implementations.
///
/// The GenericAddSub desides on the specific Target which primitive implementation to take.
///
    class GenericAddSub : public Operator {
    public:
        ///
        /// \brief The ADD_SUB_FLAGS enum. Properties for primitive adders.
        ///
        enum ADD_SUB_FLAGS{
            TERNARY = 0x1, //!< Adder has three inputs
            SUB_LEFT = 0x2, //!< Subtract left input
            SUB_MID = 0x4, //!< Subtract mid input, if ternary
            SUB_RIGHT = 0x8, //!< Subtract right input
            CONF_LEFT = 0x10, //!< Make left input configurable
            CONF_MID = 0x20, //!< Make mid input configurable, if ternary
            CONF_RIGHT = 0x40, //!< Make right input configurable
            CONF_ALL = 0x70, //!< Make all inputs configurable, if possible
            SIGN_EXTEND = 0x80 //!< Make sign extension, currently not supported, could be removed
        };
    private:
        const uint32_t flags_; //!< Specifies the properties of the adder
    public:
        ///
        /// \brief GenericAddSub constructor
        /// \param target The current Target
        /// \param wIn The wordsize of the generated adder
        /// \param flags The properties of the generated adder
        ///
        GenericAddSub(Operator* parentOp, Target* target, const uint32_t &wIn, const uint32_t &flags = 0);

        ///
        /// \brief getInputName returns the name of the input signal or the conf signal of the specific index
        /// \param index which input to return
        /// \param c_input wether to return the input signal or the configuration signal
        /// \return Name of the input
        ///
        std::string getInputName(const uint32_t &index,const bool &c_input = false) const;
        ///
        /// \brief getOutputName return the name of the output signal
        /// \return Name of the output signal
        ///
        std::string getOutputName() const;

        ///
        /// \brief hasFlags checks if a property flag is set
        /// \param flag Property to check for
        /// \return Property is set
        ///
        bool hasFlags( const uint32_t &flag ) const;

        ///
        /// \brief getInputCount returns the input count of the adder
        /// \return Input count of the adder (3 for ternary otherwise 2)
        ///
        const uint32_t getInputCount() const;

        ///
        /// \brief printFlags returns the properties as a string
        /// \return Property string.
        ///
        std::string printFlags( ) const;

        ~GenericAddSub() {}

		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);

//		static OperatorPtr parseArguments(Target *target , vector<string> &args);
//		static void registerFactory();
    public:
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
	};
}//namespace

#endif
