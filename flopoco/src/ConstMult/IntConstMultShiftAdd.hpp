#ifndef  IntConstMultShiftAdd_HPP
#define IntConstMultShiftAdd_HPP

#include "Operator.hpp"

#if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB) && defined(HAVE_SCALP)

#include "utils.hpp"
#include "pagsuite/adder_graph.h"
#include "IntConstMultShiftAddTypes.hpp"

using namespace std;

namespace flopoco {
    class IntConstMultShiftAdd : public Operator {
    public:
        static ostream nostream;
        int noOfPipelineStages;

        IntConstMultShiftAdd(
				Operator* parentOp,
				Target* target,
				int wIn_,
				string pipelined_realization_str,
				bool pipelined_=true,
				bool syncInOut_=true,
				int syncEveryN_=1,
				bool syncMux_=true,
				int  epsilon_=0,
				string truncations=""
			);

        ~IntConstMultShiftAdd() {}

        void emulate(TestCase * tc);
        void buildStandardTestCases(TestCaseList* tcl);
        struct output_signal_info{
        string signal_name;
        vector<vector<int64_t> > output_factors;
        int wordsize;};
        list<output_signal_info>& GetOutputList();

        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args );
		
        static void registerFactory();

    protected:
        int wIn;
        bool syncInOut;
        bool pipelined;
        int syncEveryN;
        bool syncMux;
        double epsilon;

        bool RPAGused;
        int emu_conf;
        vector<vector<int64_t> > output_coefficients;
        PAGSuite::adder_graph_t pipelined_adder_graph;
        list<string> input_signals;

        list<output_signal_info> output_signals;

        int noOfInputs;
        int noOfConfigurations;
        bool needs_unisim;

        void ProcessIntConstMultShiftAdd(
				Target* target, 
				string pipelined_realization_str,
				string truncations="",
				int epsilon=0
			);


        string generateSignalName(PAGSuite::adder_graph_base_node_t* node);
        IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE* identifyNodeType(PAGSuite::adder_graph_base_node_t* node);

        void identifyOutputConnections(PAGSuite::adder_graph_base_node_t* node, map<PAGSuite::adder_graph_base_node_t *, IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE *> &infoMap);
        void printAdditionalNodeInfo(map<PAGSuite::adder_graph_base_node_t *, IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE *> &infoMap );

    };
}//namespace

#else
namespace flopoco {
    class IntConstMultShiftAdd : public Operator {
    public:
        static void registerFactory();
		};
}
#endif // defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB) && defined(HAVE_SCALP)

#endif
