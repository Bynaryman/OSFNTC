#ifndef IntConstMultShiftAdd_TYPES_HPP
#define IntConstMultShiftAdd_TYPES_HPP
/*  IntConstMultShiftAdd_TYPES.hpp
 *  Version:
 *  Datum: 20.11.2014
 */

#ifdef HAVE_PAGLIB

#include "pagsuite/adder_graph.h"
#include "utils.hpp"
#include "Target.hpp"
#include "Operator.hpp"
#include "PrimitiveComponents/GenericAddSub.hpp"

#include <string>
#include <sstream>
#include <list>
#include <map>
#include <vector>
#include <gmp.h>

using namespace std;
using namespace flopoco;

namespace IntConstMultShiftAdd_TYPES {
	class TruncationRegister {
		public:
			TruncationRegister(string truncationList);
			TruncationRegister(map<pair<mpz_class, int>, vector<int> > &truncationVal);
			vector<int> const & getTruncationFor(mpz_class factor, int stage) const;
			vector<int> const & getTruncationFor(int factor, int stage) const{
				return getTruncationFor(mpz_class(factor), stage);
			}

			string convertToString();

	private:
			void parseRecord(string record);
			map<pair<mpz_class, int>, vector<int> > truncationVal_;
			static vector<int> nullVec_;
	};

    enum NODETYPE{
        NODETYPE_INPUT=1,
        NODETYPE_REG=12, // Standart register

        NODETYPE_ADD2=21, // Adder with 2 inputs
        NODETYPE_SUB2_1N=22, // Subtractor with 2 inputs
        NODETYPE_ADDSUB2_CONF=23, // Configurable adder/subtractor with 2 inputs

        NODETYPE_ADD3=31, // Adder with 3 Inputs
        NODETYPE_SUB3_1N=32, // Subtractor with 3 inputs; one fixed negative input
        NODETYPE_SUB3_2N=33, // Subtractor with 3 inputs; two fixed negative inputs
        NODETYPE_ADDSUB3_2STATE=34, // Switchable adder/subtractor with 3 inputs; two states
        NODETYPE_ADDSUB3_CONF=35, // Configurable adder/subtractor with 2 inputs

        NODETYPE_AND=51, // Multiplexer with one not zero input
        NODETYPE_MUX=52, // Standart multiplexer
        NODETYPE_MUX_Z=53, // Multiplexer with zero input

        NODETYPE_DECODER=61, // Decoder for configuration inputs

        IntConstMultShiftAdd_NODETYPE_UNKNOWN=0
    };

    struct IntConstMultShiftAdd_muxInput{
        PAGSuite::adder_graph_base_node_t* node;
        int shift;
        vector<int> configurations;
    };


class IntConstMultShiftAdd_BASE{
public:
    Target* target;
    Operator* base_op;
    PAGSuite::adder_graph_base_node_t* base_node;
    NODETYPE nodeType;
    int wordsize;
    string outputSignalName;
    int outputSignalUsageCount;
    list<PAGSuite::adder_graph_base_node_t*> outputConnectedTo;
    list<pair<string,int> > declare_list;
    list<Operator*> opList;
	vector<int> truncations;

    static string target_ID;
    static int noOfConfigurations;
    static int configurationSignalWordsize;
    static int noOfInputs;

    virtual string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap )
    { return ""; }

    virtual string get_name()
    { return ""; }

    IntConstMultShiftAdd_BASE(Operator* base){
        base_op = base;
        outputSignalUsageCount = 0;
        wordsize = 0;
    };

    void addSubComponent( Operator* op ){
        opList.push_back(op);
    }

    string declare(string signalName,int wordsize = 1 );
    void getInputOrder(vector<bool>& inputIsNegative,vector<int>& inputOrder );
    string getNegativeShiftString(string signalName , int wordsize, PAGSuite::adder_graph_base_node_t *base_node );
    string getNegativeResizeString(string signalName,int outputWordsize);

	string getShiftAndResizeString(IntConstMultShiftAdd_BASE *input_node, int wordsize, int inputShift, bool signedConversion=true);
    static string getBinary(int value, int wordsize);
    string getTemporaryName();

protected:
	//TODO class hierarchy is a bit wrong. All the "real" operand types should 
	//inherit from a derived class which provides this method
	void build_operand_realisation(
			map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap
		);
			
};

class IntConstMultShiftAdd_INPUT : public IntConstMultShiftAdd_BASE{
public:
    IntConstMultShiftAdd_INPUT(Operator* base):IntConstMultShiftAdd_BASE(base){nodeType=NODETYPE_INPUT;}
    string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap);
    string get_name(){return "INPUT";}
};

class IntConstMultShiftAdd_REG : public IntConstMultShiftAdd_BASE{
public:
    IntConstMultShiftAdd_REG(Operator* base):IntConstMultShiftAdd_BASE(base){nodeType=NODETYPE_REG;}
    string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap);
    string get_name(){return "REG";}
};

class IntConstMultShiftAdd_ADD2 : public IntConstMultShiftAdd_BASE{
public:
    IntConstMultShiftAdd_ADD2(Operator* base):IntConstMultShiftAdd_BASE(base){nodeType=NODETYPE_ADD2;}
    string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap);
    string get_name(){return "ADD2";}
};

class IntConstMultShiftAdd_SUB2_1N : public IntConstMultShiftAdd_BASE{
public:
    IntConstMultShiftAdd_SUB2_1N(Operator* base):IntConstMultShiftAdd_BASE(base){nodeType=NODETYPE_SUB2_1N;}
    string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap);
    string get_name(){return "SUB2_1N";}
};

class IntConstMultShiftAdd_ADD3 : public IntConstMultShiftAdd_BASE{
public:
    IntConstMultShiftAdd_ADD3(Operator* base):IntConstMultShiftAdd_BASE(base){nodeType=NODETYPE_ADD3;}
    string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap);
    string get_name(){return "ADD3";}
};

class IntConstMultShiftAdd_SUB3_1N : public IntConstMultShiftAdd_BASE{
public:
    IntConstMultShiftAdd_SUB3_1N(Operator* base):IntConstMultShiftAdd_BASE(base){nodeType=NODETYPE_SUB3_1N;}
    string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap);
    string get_name(){return "SUB3_1N";}
};

class IntConstMultShiftAdd_SUB3_2N : public IntConstMultShiftAdd_BASE{
public:
    IntConstMultShiftAdd_SUB3_2N(Operator* base):IntConstMultShiftAdd_BASE(base){nodeType=NODETYPE_SUB3_2N;}
    string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap);
    string get_name(){return "SUB3_2N";}
};
class IntConstMultShiftAdd_BASE_CONF;
class IntConstMultShiftAdd_DECODER : public IntConstMultShiftAdd_BASE{
public:
    int decoder_size;
    IntConstMultShiftAdd_BASE *node;
    IntConstMultShiftAdd_DECODER(Operator* base):IntConstMultShiftAdd_BASE(base){nodeType=NODETYPE_DECODER;}
    string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap);
    string get_name(){return "DECODER";}
};

class IntConstMultShiftAdd_BASE_CONF: public IntConstMultShiftAdd_BASE{
public:
    IntConstMultShiftAdd_BASE_CONF(Operator* base):IntConstMultShiftAdd_BASE(base){}
    IntConstMultShiftAdd_DECODER *decoder;
    int highCountState;
    map<short,vector<int> > adder_states;
};

class IntConstMultShiftAdd_ADDSUB2_CONF : public IntConstMultShiftAdd_BASE_CONF{
public:
    IntConstMultShiftAdd_ADDSUB2_CONF(Operator* base):IntConstMultShiftAdd_BASE_CONF(base){nodeType=NODETYPE_ADDSUB2_CONF;}
    string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap);
    string get_name(){return "ADDSUB2_CONF";}
};

class IntConstMultShiftAdd_ADDSUB3_2STATE : public IntConstMultShiftAdd_BASE_CONF{
public:
    IntConstMultShiftAdd_ADDSUB3_2STATE(Operator* base):IntConstMultShiftAdd_BASE_CONF(base){nodeType=NODETYPE_ADDSUB3_2STATE;}
    string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap);
    string get_name(){return "ADDSUB3_2STATE";}
};

class IntConstMultShiftAdd_ADDSUB3_CONF : public IntConstMultShiftAdd_BASE_CONF{
public:
    IntConstMultShiftAdd_ADDSUB3_CONF(Operator* base):IntConstMultShiftAdd_BASE_CONF(base){nodeType=NODETYPE_ADDSUB3_CONF;}
    string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap);
    string get_name(){return "ADDSUB3_CONF";}
};

class IntConstMultShiftAdd_BASE_MUX: public IntConstMultShiftAdd_BASE{
public:
    IntConstMultShiftAdd_BASE_MUX(Operator* base):IntConstMultShiftAdd_BASE(base){}
    vector<IntConstMultShiftAdd_muxInput> inputs;
};

class IntConstMultShiftAdd_AND : public IntConstMultShiftAdd_BASE_MUX{
public:
    IntConstMultShiftAdd_AND(Operator* base):IntConstMultShiftAdd_BASE_MUX(base){nodeType=NODETYPE_AND;}
    string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap);
    string get_name(){return "AND";}
};

class IntConstMultShiftAdd_MUX : public IntConstMultShiftAdd_BASE_MUX{
public:
    IntConstMultShiftAdd_DECODER *decoder;
    IntConstMultShiftAdd_MUX(Operator* base):IntConstMultShiftAdd_BASE_MUX(base){nodeType=NODETYPE_MUX;}
    string get_realisation(map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap);
    string get_name(){return "MUX";}
};
}
#endif // HAVE_PAGLIB
#endif
