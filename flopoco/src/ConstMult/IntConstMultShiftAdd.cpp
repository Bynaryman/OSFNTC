#include "IntConstMultShiftAdd.hpp"

#if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB) && defined(HAVE_SCALP)

#include <iostream>
#include <sstream>
#include <string>

#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>

#include <map>
#include <vector>
#include <set>

#include "adder_cost.hpp"

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "PrimitiveComponents/Primitive.hpp"
//#include "rpag/rpag.h"

#include "WordLengthCalculator.hpp"

using namespace std;
using namespace PAGSuite;


namespace flopoco {


IntConstMultShiftAdd::IntConstMultShiftAdd(
		Operator* parentOp, 
		Target* target, 
		int wIn_, 
		string pipelined_realization_str, 
		bool pipelined_, 
		bool syncInOut_, 
		int syncEveryN_, 
		bool syncMux_,
		int epsilon_,
		string truncations
	)
    : Operator(parentOp, target),
      wIn(wIn_),
      syncInOut(syncInOut_),
      pipelined(pipelined_),
      syncEveryN(syncEveryN_),
      epsilon(epsilon_)
{
    syncMux=syncMux_;

    ostringstream name;
    name << "IntConstMultShiftAdd_" << wIn;
    setName(name.str());

    if(pipelined_realization_str.empty()) return; //in case the realization string is not defined, don't further process it.

	ProcessIntConstMultShiftAdd(target, pipelined_realization_str, truncations, epsilon);
};

void IntConstMultShiftAdd::ProcessIntConstMultShiftAdd(
		Target* target, 
		string pipelined_realization_str,
        string truncations,
        int epsilon
	)
{
    REPORT( DETAILED, "IntConstMultShiftAdd started with syncoptions:")
	REPORT( DETAILED, "\tsyncInOut: " << (syncInOut?"enabled":"disabled"))
	REPORT( DETAILED, "\tsyncMux: " << (syncMux?"enabled":"disabled"))
	REPORT( DETAILED, "\tsync every " << syncEveryN << " stages" << std::endl )

    needs_unisim = false;
    emu_conf = 0;

    bool validParse;
    srcFileName="IntConstMultShiftAdd";

    useNumericStd();

//    setCopyrightString(UniKs::getAuthorsString(UniKs::AUTHOR_MKLEINLEIN|UniKs::AUTHOR_MKUMM|UniKs::AUTHOR_KMOELLER));

    if(UserInterface::verbose >= 3)
        pipelined_adder_graph.quiet = false; //enable debug output
    else
        pipelined_adder_graph.quiet = true; //disable debug output, except errors

    REPORT( DETAILED, "parse graph...")
    validParse = pipelined_adder_graph.parse_to_graph(pipelined_realization_str);

    if(validParse)
    {
        REPORT( DETAILED,  "check graph...")
		pipelined_adder_graph.check_and_correct(pipelined_realization_str);

		if(UserInterface::verbose >= DETAILED)
			pipelined_adder_graph.print_graph();
        pipelined_adder_graph.drawdot("pag_input_graph.dot");

		IntConstMultShiftAdd_TYPES::TruncationRegister truncationReg(truncations);

		if(truncations.empty() && (epsilon > 0))
		{
			REPORT(INFO,  "Found non-zero epsilon=" << epsilon << ", computing word sizes of truncated MCM");

			map<pair<mpz_class, int>, vector<int> > wordSizeMap;

			WordLengthCalculator wlc = WordLengthCalculator(pipelined_adder_graph, wIn, epsilon, target);
			wordSizeMap = wlc.optimizeTruncation();
			REPORT(INFO, "Finished computing word sizes of truncated MCM");
			if(UserInterface::verbose >= INFO)
			{
				for(auto & it : wordSizeMap) {
					std::cout << "(" << it.first.first << ", " << it.first.second << "): ";
					for(auto & itV : it.second)
						std::cout << itV << " ";
					std::cout << std::endl;
				}
			}

			truncationReg = IntConstMultShiftAdd_TYPES::TruncationRegister(wordSizeMap);
		}

		REPORT( DETAILED, "truncationReg is " << truncationReg.convertToString());

		int noOfFullAdders = IntConstMultShiftAdd_TYPES::getGraphAdderCost(pipelined_adder_graph,wIn,false);
		REPORT( INFO, "adder graph without truncation requires " << noOfFullAdders << " full adders");

        noOfFullAdders = IntConstMultShiftAdd_TYPES::getGraphAdderCost(pipelined_adder_graph,wIn,false,truncationReg);
		REPORT( INFO, "truncated adder graph requires " << noOfFullAdders << " full adders");


        noOfConfigurations = (*pipelined_adder_graph.nodes_list.begin())->output_factor.size();
        noOfInputs = (*pipelined_adder_graph.nodes_list.begin())->output_factor[0].size();
        noOfPipelineStages = 0;
        int configurationSignalWordsize = ceil(log2(noOfConfigurations));
		for(auto nodePtr : pipelined_adder_graph.nodes_list) {
            if (nodePtr->stage > noOfPipelineStages) {
			   	noOfPipelineStages=nodePtr->stage;
			}
		}

        REPORT( DETAILED, "noOfInputs: " << noOfInputs)
		REPORT( DETAILED, "noOfConfigurations: " << noOfConfigurations)
		REPORT( DETAILED, "noOfPipelineStages: " << noOfPipelineStages)

		if(noOfConfigurations > 1) {
			addInput("config_no", configurationSignalWordsize);
		}

        IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE::target_ID = target->getID();
        IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE::noOfConfigurations = noOfConfigurations;
        IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE::noOfInputs = noOfInputs;
        IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE::configurationSignalWordsize = configurationSignalWordsize;

        //IDENTIFY NODE
        REPORT(DETAILED,"identifying nodes...")

		map<adder_graph_base_node_t*,IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE*> additionalNodeInfoMap;
        map<int,list<adder_graph_base_node_t*> > stageNodesMap;
        for (auto nodePtr : pipelined_adder_graph.nodes_list)
        {

            stageNodesMap[nodePtr->stage].push_back(nodePtr);

            IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE* t = identifyNodeType(nodePtr);
            t->outputSignalName = generateSignalName(nodePtr);
            t->target = target;

            if      (  t->nodeType == IntConstMultShiftAdd_TYPES::NODETYPE_ADDSUB2_CONF
                       || t->nodeType == IntConstMultShiftAdd_TYPES::NODETYPE_ADDSUB3_2STATE
                       || t->nodeType == IntConstMultShiftAdd_TYPES::NODETYPE_ADDSUB3_CONF
                       ){
                REPORT(DEBUG,"has decoder")

                conf_adder_subtractor_node_t* cc = new conf_adder_subtractor_node_t();
                cc->stage = nodePtr->stage-1;
                stageNodesMap[nodePtr->stage-1].push_back( cc );
                ((IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE_CONF*)t)->decoder->outputSignalName = t->outputSignalName + "_decode";
                additionalNodeInfoMap.insert(
                            make_pair<adder_graph_base_node_t*,IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE*>(cc,((IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE_CONF*)t)->decoder)
                            );
            }else if(t->nodeType == IntConstMultShiftAdd_TYPES::NODETYPE_MUX){
                REPORT(DEBUG,"has decoder")

                mux_node_t* cc = new mux_node_t();
                cc->stage = nodePtr->stage-1;
                stageNodesMap[nodePtr->stage-1].push_back( cc );
                ((IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_MUX*)t)->decoder->outputSignalName = t->outputSignalName + "_decode";
                additionalNodeInfoMap.insert(
                            make_pair<adder_graph_base_node_t*,IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE*>(cc,((IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_MUX*)t)->decoder)
                            );
            } else if(
					t->nodeType == IntConstMultShiftAdd_TYPES::NODETYPE_ADD2 ||
					t->nodeType == IntConstMultShiftAdd_TYPES::NODETYPE_SUB2_1N ||
					t->nodeType == IntConstMultShiftAdd_TYPES::NODETYPE_ADD3 ||
					t->nodeType == IntConstMultShiftAdd_TYPES::NODETYPE_SUB3_1N ||
					t->nodeType == IntConstMultShiftAdd_TYPES::NODETYPE_SUB3_2N
				) {
				//Store the truncation if it exists
				int computedConstant = nodePtr->output_factor[0][0];
				t->truncations = truncationReg.getTruncationFor(
						computedConstant,
						nodePtr->stage
					);
			}
            t->wordsize  = computeWordSize( nodePtr,wIn );
            t->base_node = nodePtr;
            //additionalNodeInfoMap.insert( std::make_pair<adder_graph_base_node_t*,IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE*>(nodePtr,t) );
            additionalNodeInfoMap.insert( {nodePtr,t} );
        }
        //IDENTIFY CONNECTIONS
        REPORT(DETAILED,"identifiing node connections..")
		for (auto nodePtr : pipelined_adder_graph.nodes_list)
        {
            identifyOutputConnections(nodePtr ,additionalNodeInfoMap);
        }
        printAdditionalNodeInfo(additionalNodeInfoMap);

        //START BUILDING NODES
        REPORT(DETAILED,"building nodes..")
                int unpiped_stage_count=0;
        bool isMuxStage = false;
        for (
				unsigned int currentStage=0 ;
				currentStage <= (unsigned int) noOfPipelineStages ;
				currentStage++
		) {
            isMuxStage = false;
            for (auto operationNode : stageNodesMap[currentStage])
            {
                IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE* op_node = additionalNodeInfoMap[operationNode];
                REPORT( DETAILED, op_node->outputSignalName << " as " << op_node->get_name())

                if(op_node->nodeType == IntConstMultShiftAdd_TYPES::NODETYPE_INPUT)
                {
                    input_node_t* t = (input_node_t*)op_node->base_node;

                    stringstream inputSignalName;
                    int id=0;
                    while( t->output_factor[0][id]==0 && id < noOfInputs )
                    {
                        id++;
                    }

                    inputSignalName << "x_in" << id;
                    addInput(inputSignalName.str(), wIn);
                    vhdl << "\t" << declare(op_node->outputSignalName,wIn) << " <= " << inputSignalName.str() << ";" << endl;
                    input_signals.push_back(inputSignalName.str());
                }
                else
                vhdl << op_node->get_realisation(additionalNodeInfoMap);

                if(op_node->nodeType == IntConstMultShiftAdd_TYPES::NODETYPE_MUX ||
                        op_node->nodeType == IntConstMultShiftAdd_TYPES::NODETYPE_AND)
                    isMuxStage = true;

                for(list<pair<string,int> >::iterator declare_it=op_node->declare_list.begin();declare_it!=op_node->declare_list.end();++declare_it  )
                    declare( (*declare_it).first,(*declare_it).second );

                for(list<Operator* >::iterator declare_it=op_node->opList.begin();declare_it!=op_node->opList.end();++declare_it  )
                    addSubComponent(*declare_it);
            }


            if( !(syncEveryN>1 && !syncMux && isMuxStage) )
                unpiped_stage_count++;

            bool doPipe=false;
            if( pipelined && (!isMuxStage || (isMuxStage && syncMux) ) )
            {
                if( (currentStage == 0 || (int)currentStage == noOfPipelineStages) )
                {
                    //IN OUT STAGE
                    if( syncInOut )
                    {
                        doPipe = true;
                    }
                }
                else if( (unpiped_stage_count%syncEveryN)==0 )
                {
                    doPipe = true;
                }
            }
            else if( !pipelined )
            {
                if( syncInOut && (currentStage == 0 || (int)currentStage == noOfPipelineStages) )
                {
                    doPipe = true;
                }
                else if( (unpiped_stage_count%syncEveryN)==0 )
                {
                    doPipe = true;
                }
            }

            if(doPipe)
            {
                REPORT( DETAILED, "----pipeline----")
//                        nextCycle(); //!!!
                unpiped_stage_count = 0;
            }

        }

        output_signal_info sig_info;
        short realizedOutputNodes = 0;
        for (auto operationNode : stageNodesMap[noOfPipelineStages])
        {
            if (is_a<output_node_t>(*operationNode)){
                IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE* op_node = additionalNodeInfoMap[operationNode];
                stringstream outputSignalName;
                outputSignalName << "x_out" << realizedOutputNodes;
                realizedOutputNodes++;
                for(int j=0; j < noOfConfigurations; j++) {
                    outputSignalName << "_c";
                    for(int i=0; i < noOfInputs; i++)
                    {
                        if(i>0) outputSignalName << "v" ;
                        outputSignalName << ((operationNode->output_factor[j][i] < 0) ? "m" : "");
                        outputSignalName << abs(operationNode->output_factor[j][i]);
                    }
                }
                addOutput(outputSignalName.str(), op_node->wordsize);

                sig_info.output_factors = operationNode->output_factor;
                sig_info.signal_name = outputSignalName.str();
                sig_info.wordsize = op_node->wordsize;
                output_signals.push_back( sig_info );
                vhdl << "\t" << outputSignalName.str() << " <= " << op_node->outputSignalName << ";" << endl;
            }
        }
    }

    //cout << endl;

    pipelined_adder_graph.clear();
}

list<IntConstMultShiftAdd::output_signal_info> &IntConstMultShiftAdd::GetOutputList()
{
    return output_signals;
}

void IntConstMultShiftAdd::emulate(TestCase * tc)
{
    vector<mpz_class> input_vec;

    unsigned int confVal=emu_conf;

    if( noOfConfigurations > 1 )
        tc->addInput("config_no",emu_conf);

    for(int i=0;i<noOfInputs;i++ )
    {
        stringstream inputName;
        inputName << "x_in" << i;

        mpz_class inputVal = tc->getInputValue(inputName.str());

        mpz_class big1 = (mpz_class(1) << (wIn));
        mpz_class big1P = (mpz_class(1) << (wIn-1));

        if ( inputVal >= big1P)
            inputVal = inputVal - big1;

        input_vec.push_back(inputVal);
    }

    mpz_class expectedResult;


    for(list<output_signal_info>::iterator out_it= output_signals.begin();out_it!=output_signals.end();++out_it  )
    {
		expectedResult = 0;
        stringstream comment;
        for(int i=0; i < noOfInputs; i++)
        {
            mpz_class output_factor;
            if ((int)confVal<noOfConfigurations)
                output_factor= (long signed int) (*out_it).output_factors[confVal][i];
            else
                output_factor= (long signed int) (*out_it).output_factors[noOfConfigurations-1][i];

            expectedResult += input_vec[i] * output_factor;
            if(i != 0) comment << " + ";
            else comment << "\t";
            comment << input_vec[i] << " * " << output_factor;
        }
        comment << " == " << expectedResult << endl;

        int output_ws = computeWordSize((*out_it).output_factors,wIn);
        if ( expectedResult < mpz_class(0) )
        {
            mpz_class min_val = -1 * (mpz_class(1) << (output_ws-1));
            if( expectedResult < min_val )
            {
                std::stringstream err;
                err << "ERROR in testcase <" << comment.str() << ">" << std::endl;
                err << "Wordsize of outputfactor ("<< output_ws << ") does not match given (" << computeWordSize((*out_it).output_factors,wIn) << ")";
                THROWERROR( err.str() )
            }
        }
        else
        {
            mpz_class max_val = (mpz_class(1) << (output_ws-1)) -1;
            if( expectedResult > max_val )
            {
                std::stringstream err;
                err << "ERROR in testcase <" << comment.str() << ">\n";
                err << "Outputfactor does not fit in given wordsize" << endl;
                THROWERROR( err.str() )
            }
        }

        try
        {
            tc->addComment(comment.str());
            tc->addExpectedOutput((*out_it).signal_name,expectedResult);
        }
        catch(string errorstr)
        {
            cout << errorstr << endl;
        }
    }

    if(emu_conf < noOfConfigurations-1 && noOfConfigurations!=1)
        emu_conf++;
    else
        emu_conf=0;
}

void IntConstMultShiftAdd::buildStandardTestCases(TestCaseList * tcl)
{
    TestCase* tc;

    int min_val = -1 * (1 << (wIn-1));
    int max_val = (1 << (wIn-1)) -1;

    stringstream max_str;
    max_str << "Test MAX: " << max_val;

    stringstream min_str;
    min_str << "Test MIN: " << min_val;

    for(int i=0;i<noOfConfigurations;i++)
    {
        tc = new TestCase (this);
        tc->addComment("Test ZERO");
        if( noOfConfigurations > 1 )
        {
            tc->addInput("config_no",i);
        }
        for(list<string>::iterator inp_it = input_signals.begin();inp_it!=input_signals.end();++inp_it )
        {
            tc->addInput(*inp_it,0 );
        }
        emulate(tc);
        tcl->add(tc);
    }

    for(int i=0;i<noOfConfigurations;i++)
    {
        tc = new TestCase (this);
        tc->addComment("Test ONE");
        if( noOfConfigurations > 1 )
        {
            tc->addInput("config_no",i);
        }
        for(list<string>::iterator inp_it = input_signals.begin();inp_it!=input_signals.end();++inp_it )
        {
            tc->addInput(*inp_it,1 );
        }
        emulate(tc);
        tcl->add(tc);
    }

    for(int i=0;i<noOfConfigurations;i++)
    {
        tc = new TestCase (this);
        tc->addComment(min_str.str());
        if( noOfConfigurations > 1 )
        {
            tc->addInput("config_no",i);
        }
        for(list<string>::iterator inp_it = input_signals.begin();inp_it!=input_signals.end();++inp_it )
        {
            tc->addInput(*inp_it,min_val );
        }
        emulate(tc);
        tcl->add(tc);
    }

    for(int i=0;i<noOfConfigurations;i++)
    {
        tc = new TestCase (this);
        tc->addComment(max_str.str());
        if( noOfConfigurations > 1 )
        {
            tc->addInput("config_no",i);
        }
        for(list<string>::iterator inp_it = input_signals.begin();inp_it!=input_signals.end();++inp_it )
        {
            tc->addInput(*inp_it,max_val );
        }
        emulate(tc);
        tcl->add(tc);
    }
}

string IntConstMultShiftAdd::generateSignalName(adder_graph_base_node_t *node)
{
    stringstream signalName;
    signalName << "x";
    for( int i=0;i<noOfConfigurations;i++ )
    {
        signalName << "_c";
        for( int j=0;j<noOfInputs;j++)
        {
            if(j>0) signalName << "v";
            if( node->output_factor[i][j] != DONT_CARE )
            {
                if( node->output_factor[i][j] < 0 )
                {
                    signalName << "m";
                }
                signalName << abs( node->output_factor[i][j] );
            }
            else
                signalName << "d";
        }
    }
    signalName << "_s" << node->stage;
    if (is_a<output_node_t>(*node))
        signalName << "_o";
    return signalName.str();
}

IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE* IntConstMultShiftAdd::identifyNodeType(adder_graph_base_node_t *node)
{
    if(is_a<adder_subtractor_node_t>(*node))
    {
        adder_subtractor_node_t* t = (adder_subtractor_node_t*)node;
        if( t->inputs.size() == 2)
        {
            if(t->input_is_negative[0] || t->input_is_negative[1])
            {
                IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_SUB2_1N* new_node = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_SUB2_1N(this);
                return new_node;
            }
            else
            {
                IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_ADD2* new_node = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_ADD2(this);
                return new_node;
            }
        }
        else if( t->inputs.size() == 3)
        {
            needs_unisim = true;
            int negative_count=0;
            if(t->input_is_negative[0]) negative_count++;
            if(t->input_is_negative[1]) negative_count++;
            if(t->input_is_negative[2]) negative_count++;

            if(negative_count == 0)
            {
                IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_ADD3* new_node = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_ADD3(this);
                return new_node;
            }
            else if(negative_count == 1)
            {
                IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_SUB3_1N* new_node = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_SUB3_1N(this);
                return new_node;
            }
            else if(negative_count == 2)
            {
                IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_SUB3_2N* new_node = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_SUB3_2N(this);
                return new_node;
            }
        }
    }
    else if(is_a<conf_adder_subtractor_node_t>(*node))
    {
        conf_adder_subtractor_node_t* t = (conf_adder_subtractor_node_t*)node;
        int highCountState;
        map<short,vector<int> > adder_states;

        highCountState = -1;
        int highCount=0;
        for( unsigned int i = 0;i<t->input_is_negative.size();i++)
        {
            short cur_state=0;
            for( unsigned int j = 0;j<t->input_is_negative[i].size();j++)
            {
                if(t->input_is_negative[i][j])
                    cur_state |= (1<<j);
            }
            map<short,vector<int> >::iterator found = adder_states.find(cur_state);
            if( found == adder_states.end())
            {
                vector<int> newvec;
                newvec.push_back(i);
                adder_states.insert( {cur_state,newvec}  );
            }
            else
            {
                (*found).second.push_back(i);
                if((int)(*found).second.size()>highCount)
                {
                    highCount = (*found).second.size();
                    highCountState = cur_state;
                }
            }
        }
        if(highCountState==-1)
        {
            highCountState = (*adder_states.rbegin()).first;
        }

        if(t->inputs.size() == 2)
        {
            IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_ADDSUB2_CONF* new_node = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_ADDSUB2_CONF(this);
            IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_DECODER* new_dec = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_DECODER(this);
            new_dec->decoder_size = 2;
            new_node->decoder = new_dec;
            new_dec->node = new_node;
            new_node->adder_states = adder_states;
            new_node->highCountState = highCountState;
            return new_node;
        }
        else if(t->inputs.size() == 3)
        {
            needs_unisim = true;
            if(adder_states.size() == 2 && this->getTarget()->getVendor() == "Xilinx" )
            {
                IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_ADDSUB3_2STATE* new_node = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_ADDSUB3_2STATE(this);
                IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_DECODER* new_dec = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_DECODER(this);
                new_dec->decoder_size = 1;
                new_node->decoder = new_dec;
                new_dec->node = new_node;
                new_node->adder_states = adder_states;
                new_node->highCountState = highCountState;
                return new_node;
            }
            else
            {
                IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_ADDSUB3_CONF* new_node = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_ADDSUB3_CONF(this);
                IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_DECODER* new_dec = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_DECODER(this);
                new_dec->decoder_size = 3;
                new_node->decoder = new_dec;
                new_dec->node = new_node;
                new_node->adder_states = adder_states;
                new_node->highCountState = highCountState;
                return new_node;
            }
        }
    }
    else if(is_a<register_node_t>(*node) || is_a<output_node_t>(*node) )
    {
        IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_REG* new_node = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_REG(this);
        return new_node;
    }
    else if(is_a<mux_node_t>(*node))
    {
        mux_node_t* t = (mux_node_t*)node;
        vector<int> dontCareConfig;
        IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_muxInput zeroInput;
        zeroInput.node = NULL;
        zeroInput.shift = 0;
        IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_muxInput dontCareInput;
        dontCareInput.node = NULL;
        dontCareInput.shift = -1;
        vector<IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_muxInput> muxInfoMap;
        for(unsigned int i=0;i<t->inputs.size();i++)
        {
            bool found=false;
            if(t->output_factor[i][0] == DONT_CARE)
            {
                dontCareInput.configurations.push_back(i);
                continue;
            }

            if( t->output_factor[i] == vector<int64_t>(t->output_factor[i].size(),0) )
            {
                zeroInput.configurations.push_back(i);
                continue;
            }

            for( vector<IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_muxInput>::iterator iter = muxInfoMap.begin();
                 iter!=muxInfoMap.end();
                 ++iter)
            {
                if( (*iter).node == t->inputs[i] && (*iter).shift == t->input_shifts[i] )
                {
                    found = true;
                    (*iter).configurations.push_back(i);
                }
            }

            if( !found )
            {
                IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_muxInput newInfo;
                newInfo.node = t->inputs[i];
                newInfo.shift = t->input_shifts[i];
                newInfo.configurations.push_back(i);
                muxInfoMap.push_back(newInfo);
            }
        }
        if(zeroInput.configurations.size() > 0)
            muxInfoMap.push_back(zeroInput);

        if(dontCareInput.configurations.size() > 0)
        {
            muxInfoMap.push_back(dontCareInput);
        }

        for (unsigned int n=muxInfoMap.size(); n>1; n=n-1){
            for (unsigned int i=0; i<n-1; i=i+1){
                if (muxInfoMap.at(i).configurations.size() > muxInfoMap.at(i+1).configurations.size()){
                    IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_muxInput tmpInfo = muxInfoMap[i];
                    muxInfoMap[i] = muxInfoMap[i+1];
                    muxInfoMap[i+1] = tmpInfo;
                }
            }
        }

        short nonZeroOutputCount=0;
        for( int i=0;i<noOfConfigurations;i++ )
        {
            if( t->output_factor[i] != vector<int64_t>(t->output_factor[i].size(),0) && t->output_factor[i][0] != DONT_CARE )
            {
                nonZeroOutputCount++;
            }
        }

        if( nonZeroOutputCount == 1 )
        {
            IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_AND* new_node = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_AND(this);
            new_node->inputs = muxInfoMap;
            return new_node;
        }
        else
        {
            IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_MUX* new_node = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_MUX(this);
            IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_DECODER* new_dec = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_DECODER(this);
            new_node->decoder = new_dec;
            new_dec->node = new_node;
            new_node->inputs = muxInfoMap;
            return new_node;
        }
    }
    else if(is_a<input_node_t>(*node))
    {
        IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_INPUT* new_node = new IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_INPUT(this);
        return new_node;
    }
    else
    {
        return NULL;
    }
    return NULL;
}

void IntConstMultShiftAdd::identifyOutputConnections(adder_graph_base_node_t *node, map<adder_graph_base_node_t *, IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE*> &infoMap)
{
    if(is_a<adder_subtractor_node_t>(*node))
    {
        adder_subtractor_node_t* t = (adder_subtractor_node_t*)node;
        for(int i=0;i<(int)t->inputs.size();i++)
        {
            if( t->inputs[i] != NULL )
            {
                infoMap[t->inputs[i]]->outputConnectedTo.push_back(node);
            }
        }
    }
    else if(is_a<conf_adder_subtractor_node_t>(*node))
    {
        conf_adder_subtractor_node_t* t = (conf_adder_subtractor_node_t*)node;
        for(int i=0;i<(int)t->inputs.size();i++)
        {
            if( t->inputs[i] != NULL )
            {
                infoMap[t->inputs[i]]->outputConnectedTo.push_back(node);
            }
        }
    }
    else if(is_a<register_node_t>(*node) || is_a<output_node_t>(*node))
    {
        register_node_t* t = (register_node_t*)node;
        infoMap[t->input]->outputConnectedTo.push_back(node);
    }
    else if(is_a<mux_node_t>(*node))
    {
        mux_node_t* t = (mux_node_t*)node;
        for(int i=0;i<(int)t->inputs.size();i++)
        {
            if( t->inputs[i] != NULL )
            {
                infoMap[t->inputs[i]]->outputConnectedTo.push_back(node);
            }
        }
    }
}

void IntConstMultShiftAdd::printAdditionalNodeInfo(map<adder_graph_base_node_t *, IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE*> &infoMap)
{
    stringstream nodeInfoString;
    for( map<adder_graph_base_node_t *, IntConstMultShiftAdd_TYPES::IntConstMultShiftAdd_BASE*>::iterator nodeInfo = infoMap.begin();
         nodeInfo != infoMap.end();
         ++nodeInfo)
    {
        nodeInfoString << "Node (" << (*nodeInfo).first << ") identified as <"<< (*nodeInfo).second->nodeType << ":" << (*nodeInfo).second->get_name() <<"> in stage "<< (*nodeInfo).first->stage <<endl;
        nodeInfoString << "\tOutputsignal: "<<(*nodeInfo).second->outputSignalName<< endl;
        nodeInfoString << "\tOutputvalues: ";
        for(int i=0;i<(int)(*nodeInfo).first->output_factor.size();i++)
        {

            if(i>0) nodeInfoString << ";";
            for(int j=0;j<(int)(*nodeInfo).first->output_factor[i].size();j++)
            {

                if(j>0)
                    nodeInfoString << ",";
                if( (*nodeInfo).first->output_factor[i][j]!=DONT_CARE )
                    nodeInfoString<<(*nodeInfo).first->output_factor[i][j];
                else
                    nodeInfoString << "-";
            }
        }

        nodeInfoString << " (ws:" << (*nodeInfo).second->wordsize << ")" << endl;

        if( (*nodeInfo).second->outputConnectedTo.size() > 0 )
        {
            list<adder_graph_base_node_t*>::iterator iter = (*nodeInfo).second->outputConnectedTo.begin();
            nodeInfoString << "\tOutputConnectedTo: " << endl << "\t\t" << *iter  << "("<< infoMap[*iter ]->outputSignalName << ")";
            for( ++iter;
                 iter!= (*nodeInfo).second->outputConnectedTo.end();
                 ++iter)
            {
                nodeInfoString << endl << "\t\t" << *iter  << "("<< infoMap[*iter ]->outputSignalName << ")";
            }

            nodeInfoString << endl;
        }
    }

    nodeInfoString << endl;

    REPORT( DETAILED, nodeInfoString.str())
}

	OperatorPtr flopoco::IntConstMultShiftAdd::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args )
{
	if (target->getVendor() != "Xilinx")
		throw std::runtime_error("Can't build xilinx primitive on non xilinx target");

	int wIn, sync_every = 0;
	std::string graph, truncations;
	bool sync_inout, sync_muxes, pipeline;
	int epsilon;

	UserInterface::parseInt(args, "wIn", &wIn);
	UserInterface::parseString(args, "graph", &graph);
	UserInterface::parseString( args, "truncations", &truncations);
	UserInterface::parseBoolean(args, "pipeline", &pipeline);
	UserInterface::parseInt(args, "epsilon", &epsilon);
	UserInterface::parseBoolean(args, "sync_inout", &sync_inout);
	UserInterface::parseBoolean(args, "sync_muxes", &sync_muxes);
	UserInterface::parseInt(args, "sync_every", &sync_every);

	if (truncations == "\"\"") {
		truncations = "";
	}

	return new IntConstMultShiftAdd(parentOp, target, wIn, graph, pipeline, sync_inout, sync_every, sync_muxes, epsilon, truncations);
}


}//namespace
#endif // HAVE_PAGLIB


namespace flopoco {
    void flopoco::IntConstMultShiftAdd::registerFactory()
    {
#if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB) && defined(HAVE_SCALP)
      UserInterface::add( "IntConstMultShiftAdd", // name
                          "A component for building constant multipliers based on pipelined adder graphs (PAGs).", // description, string
                          "ConstMultDiv", // category, from the list defined in UserInterface.cpp
                          "",
                          "wIn(int): Wordsize of pag inputs; \
                          graph(string): Realization string of the pag; \
                          epsilon(int)=0: Allowable error for truncated constant multipliers; \
                          pipeline(bool)=true: Enable pipelining of the pag; \
                          sync_inout(bool)=true: Enable pipeline registers for input and output stage; \
                          sync_muxes(bool)=true: Enable counting mux-only stages as full stage; \
                          sync_every(int)=1: Count of stages after which will be pipelined;"
						  "truncations(string)=\"\": provides the truncations for subvalues", //format: const1,stage:trunc_input_0,trunc_input_1,...;const2,stage:trunc_input_0,trunc_input_1,...;...",
                          "",
                          IntConstMultShiftAdd::parseArguments
      );
#endif // HAVE_SCALP and HAVE_PAGLIB
    }
}//namespace
