#ifndef XILINX_COMPARATOR_HPP
#define XILINX_COMPARATOR_HPP

#include "Operator.hpp"
#include "utils.hpp"
#include "Xilinx_LUT6.hpp"

namespace flopoco {
    class Xilinx_Comparator : public Operator {
      public:
        enum ComparatorType {
            ComparatorType_lt,
            ComparatorType_gt,
            ComparatorType_le,
            ComparatorType_ge,
            ComparatorType_eq,
            ComparatorType_ne,
            ComparatorType_invalid
        };

        ComparatorType m_type;

		Xilinx_Comparator(Operator* parentOp, Target *target, int wIn, ComparatorType type );

        void emulate( TestCase *tc );
        void buildStandardTestCases( TestCaseList *tcl );

        string getLUT_lt();
        string getLUT_gt();
        string getLUT_le();
        string getLUT_ge();
        string getLUT_eq();
        string getLUT_ne();

        static ComparatorType getTypeFromString( const std::string &typestr );

        /** Factory method that parses arguments and calls the constructor */
        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args );

        /** Factory register method */
        static void registerFactory();
    };
}
#endif // XILINX_COMPARATOR_H
