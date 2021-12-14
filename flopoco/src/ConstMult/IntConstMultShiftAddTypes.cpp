/*  IntConstMultShiftAdd_TYPES.cpp
 *  Version:
 *  Datum: 20.11.2014
 */
#ifdef HAVE_PAGLIB
#include "IntConstMultShiftAddTypes.hpp"

#include <algorithm>
#include <sstream>

//#include "FloPoCo.hpp"
#include "PrimitiveComponents/GenericLut.hpp"
#include "PrimitiveComponents/GenericMux.hpp"
#include "PrimitiveComponents/Xilinx/Xilinx_TernaryAdd_2State.hpp"
#include "utils.hpp"
//#include "PrimitiveComponents/Altera/Altera_TernaryAdd.hpp"

using namespace PAGSuite;

namespace IntConstMultShiftAdd_TYPES {
string IntConstMultShiftAdd_BASE::target_ID = "NONE";
int IntConstMultShiftAdd_BASE::noOfConfigurations = 0;
int IntConstMultShiftAdd_BASE::configurationSignalWordsize = 0;
int IntConstMultShiftAdd_BASE::noOfInputs = 0;

vector<int> IntConstMultShiftAdd_TYPES::TruncationRegister::nullVec_ = {0,0,0};

TruncationRegister::TruncationRegister(map<pair<mpz_class, int>, vector<int> > &truncationVal)
{
	truncationVal_ = truncationVal; //copy truncation values
}

TruncationRegister::TruncationRegister(string truncationList)
{
	static const string fieldDelimiter{";"};
	auto getNextField = [](string& val)->string{
		long unsigned int offset = val.find(fieldDelimiter);
		string ret = val.substr(0, offset);
		if (offset != string::npos) {
			offset += 1;
		}
		val.erase(0, offset);
		return ret;
	};
	while(truncationList.length() > 0)
		parseRecord(getNextField(truncationList));
}


void TruncationRegister::parseRecord(string record)
{
	static const string identDelimiter{':'};
	static const string fieldDelimiter{','};
	
	auto getNextField = [](string& val)->mpz_class{
		long unsigned int offset = val.find(fieldDelimiter);
		string ret = val.substr(0, offset);
		if (offset != string::npos) {
			offset += 1;
		}
		val.erase(0, offset);
		return mpz_class(ret);
	};
	
	long unsigned int  offset = record.find(identDelimiter);
	if (offset == string::npos) {
		throw string{"IntConstMultShiftAdd::TruncationRegister::parseRecord : "
		"wrong format "} + record;
	}
	string recordIdStr = record.substr(0, offset);
	record.erase(0, offset + 1);
	string& valuesStr = record;
	
	mpz_class factor = getNextField(recordIdStr);	
	int stage = getNextField(recordIdStr).get_si();

	vector<int> truncats;

	while(valuesStr.length() > 0) {
		truncats.push_back(getNextField(valuesStr).get_si());
	}

	truncationVal_.insert(make_pair(make_pair(factor, stage), truncats));
}

vector<int> const & TruncationRegister::getTruncationFor(
		mpz_class factor, 
		int stage
	) const
{
	auto iter = truncationVal_.find(make_pair(factor, stage));
	if (iter == truncationVal_.end())
		return nullVec_;
	return iter->second;
}

string TruncationRegister::convertToString()
{
	stringstream truncStr;

	//format: const1,stage:trunc_input_0,trunc_input_1,...;const2,stage:trunc_input_0,trunc_input_1,...;...
	//data structure: map<pair<mpz_class, int>, vector<int> >
	bool firstIter=true;
	for(auto& tv : truncationVal_)
	{
		if(!firstIter) truncStr << ";";
		truncStr << tv.first.first << "," << tv.first.second << ":";
		for(unsigned i=0; i < tv.second.size(); i++)
		{
			truncStr << tv.second[i];
			if(i != tv.second.size()-1) truncStr << ",";
		}
		firstIter=false;
	}

	return truncStr.str();
}


	string IntConstMultShiftAdd_INPUT::get_realisation(map<adder_graph_base_node_t *, IntConstMultShiftAdd_BASE *> &InfoMap)
{
	return "";
}

string IntConstMultShiftAdd_REG::get_realisation(map<adder_graph_base_node_t *, IntConstMultShiftAdd_BASE *> &InfoMap)
{
    register_node_t* t = (register_node_t*)(base_node);
    
    bool negate=false;
    for(unsigned int i=0;i<t->output_factor.size();++i){
        for(unsigned int j=0;j<t->output_factor[i].size();++j){
            if ( t->output_factor[i][j] != DONT_CARE ){
                if ( (t->output_factor[i][j] > 0 && t->input->output_factor[i][j] < 0)
                    || (t->output_factor[i][j] < 0 && t->input->output_factor[i][j] > 0) ){
						negate = true;
                        i = t->output_factor.size();
						break;
				}
			}
		}
	}
    
    base_op->vhdl << "\t" << base_op->declare(outputSignalName,wordsize) << " <= ";
    if(negate) base_op->vhdl << "std_logic_vector(-signed(";
    base_op->vhdl << getShiftAndResizeString(InfoMap[t->input],wordsize,t->input_shift,false);
    if(negate) base_op->vhdl << "))";
    base_op->vhdl << ";" << endl;
    return "";
}

string IntConstMultShiftAdd_ADD2::get_realisation(map<adder_graph_base_node_t *, IntConstMultShiftAdd_BASE *> &InfoMap)
{
    adder_subtractor_node_t* t = (adder_subtractor_node_t*)(base_node);
	base_op->vhdl << "\t-- building binary adder for factor " << 
		t->output_factor[0][0] << endl;
	build_operand_realisation(InfoMap);
	return "";
}

string IntConstMultShiftAdd_ADD3::get_realisation(map<adder_graph_base_node_t *, IntConstMultShiftAdd_BASE *> &InfoMap)
{
    adder_subtractor_node_t* t = (adder_subtractor_node_t*)(base_node);
	base_op->vhdl << "\t-- building ternary adder for factor " << 
		t->output_factor[0][0] << endl;
	build_operand_realisation(InfoMap);
	return "";
}

string IntConstMultShiftAdd_SUB2_1N::get_realisation(map<adder_graph_base_node_t *, IntConstMultShiftAdd_BASE *> &InfoMap)
{
    adder_subtractor_node_t* t = (adder_subtractor_node_t*)(base_node);
	base_op->vhdl << "\t-- building binary subtractor for factor " << 
		t->output_factor[0][0] << endl;
	build_operand_realisation(InfoMap);
	return "";
}

string IntConstMultShiftAdd_SUB3_1N::get_realisation(map<adder_graph_base_node_t *, IntConstMultShiftAdd_BASE *> &InfoMap)
{
    adder_subtractor_node_t* t = (adder_subtractor_node_t*)(base_node);
	base_op->vhdl << "\t-- building ternary subtractor (1 negative input) for factor " << 
		t->output_factor[0][0] << endl;
	build_operand_realisation(InfoMap);
	return "";
}

string IntConstMultShiftAdd_SUB3_2N::get_realisation(map<adder_graph_base_node_t *, IntConstMultShiftAdd_BASE *> &InfoMap)
{
    adder_subtractor_node_t* t = (adder_subtractor_node_t*)(base_node);
	base_op->vhdl << "\t-- building ternary substractor (two negative inputs) for factor " << 
		t->output_factor[0][0] << endl;
	build_operand_realisation(InfoMap);
	return "";
}

string IntConstMultShiftAdd_DECODER::get_realisation(map<adder_graph_base_node_t *, IntConstMultShiftAdd_BASE *> &InfoMap)
{
    map<uint,uint > decoder_content;
    if( node->nodeType == NODETYPE_ADDSUB3_2STATE ){
        int i=0;
        for(map<short,vector<int> >::iterator iter=((IntConstMultShiftAdd_ADDSUB3_2STATE*)node)->adder_states.begin();
            iter!=((IntConstMultShiftAdd_ADDSUB3_2STATE*)node)->adder_states.end();++iter)
        {
            for(vector<int>::iterator iter2=(*iter).second.begin();iter2!=(*iter).second.end();++iter2){
                decoder_content[ (*iter2) ] = i;
            }
            i++;
        }
    }else if( node->nodeType == NODETYPE_MUX ){
        IntConstMultShiftAdd_MUX* t = ((IntConstMultShiftAdd_MUX*)node);
        int i=0;
        for( vector<IntConstMultShiftAdd_muxInput>::iterator iter=t->inputs.begin();iter!=t->inputs.end();++iter ){
            IntConstMultShiftAdd_muxInput inp = *iter;

            for(vector<int>::iterator iter2=inp.configurations.begin();iter2!=inp.configurations.end();++iter2){
                decoder_content[ (*iter2) ] = i;
            }
            i++;
        }
        i=0;
        int c = t->inputs.size()-1;
        while(c>0){
            c = c>>1;
            i++;
        }
        decoder_size = i;
        //cerr << "Mux Decoder size " << i << endl;
    }
    else{
        for(map<short,vector<int> >::iterator iter=((IntConstMultShiftAdd_BASE_CONF*)node)->adder_states.begin();
            iter!=((IntConstMultShiftAdd_BASE_CONF*)node)->adder_states.end();++iter)
        {
            for(vector<int>::iterator iter2=(*iter).second.begin();iter2!=(*iter).second.end();++iter2){
                if(decoder_size >= 3 && (*iter).first==3){
                    decoder_size = 4;
                    decoder_content[ (*iter2) ] = 0x8;
                }
                else{
                    decoder_content[ (*iter2) ] = (*iter).first;
                }
            }
        }
    }

    stringstream outname;
    outname << node->outputSignalName << "_decode";
    outputSignalName = outname.str();

    string dec_name = outputSignalName+"r";
    GenericLut* dec = new GenericLut(base_op, node->target,dec_name,decoder_content,configurationSignalWordsize,decoder_size);
    base_op->addSubComponent(dec);
    base_op->inPortMap("x_in_this_does_not_exist!!","config_no");
    base_op->outPortMap("x_out",outputSignalName);
    base_op->vhdl << base_op->instance(dec,outputSignalName+"r_i") << endl;

    return "";
}

string IntConstMultShiftAdd_ADDSUB2_CONF::get_realisation(map<adder_graph_base_node_t *, IntConstMultShiftAdd_BASE *> &InfoMap)
{
    conf_adder_subtractor_node_t* t = (conf_adder_subtractor_node_t*)(base_node);

    GenericAddSub* addsub = new GenericAddSub(base_op, target,wordsize);
    base_op->addSubComponent(addsub);
    for(int i=0;i<2;i++){
        IntConstMultShiftAdd_BASE* t_in=InfoMap[t->inputs[i]];
        string t_name = t_in->getTemporaryName();
        base_op->declare(t_name,wordsize);
        base_op->vhdl << t_name << " <= std_logic_vector(" << getShiftAndResizeString( t_in,wordsize,t->input_shifts[i]) << ");" << endl;

        base_op->inPortMap(addsub->getInputName(i),t_name);
    }
    base_op->inPortMap(addsub->getInputName(1,true),decoder->outputSignalName + "(1)");
    base_op->inPortMap(addsub->getInputName(0,true),decoder->outputSignalName + "(0)");
    base_op->outPortMap(addsub->getOutputName(),outputSignalName);
    base_op->vhdl << base_op->instance(addsub,outputSignalName+"_addsub") << endl;
    base_op->vhdl << getNegativeShiftString( outputSignalName,wordsize,t ) << endl;
    return "";
}

string IntConstMultShiftAdd_ADDSUB3_2STATE::get_realisation(map<adder_graph_base_node_t *, IntConstMultShiftAdd_BASE *> &InfoMap)
{
    conf_adder_subtractor_node_t* t = (conf_adder_subtractor_node_t*)(base_node);

    short adderStates[2] = {0};
    map<short,vector<int> >::iterator stateIter = adder_states.begin();
    adderStates[0] = ( *stateIter ).first;
    ++stateIter;
    adderStates[1] = ( *stateIter ).first;

    if( IntConstMultShiftAdd_BASE::target_ID == "Virtex5" || IntConstMultShiftAdd_BASE::target_ID == "Virtex6" )
    {
        Xilinx_TernaryAdd_2State* add3 = new Xilinx_TernaryAdd_2State(base_op, target,wordsize,adderStates[0],adderStates[1]);
        base_op->addSubComponent(add3);
        base_op->vhdl << base_op->declare( "add3_" + outputSignalName + "_x",wordsize   ) << " <= "
             << getShiftAndResizeString( InfoMap[ t->inputs[0] ] , wordsize, t->input_shifts[0],false) << ";";
        base_op->vhdl << base_op->declare( "add3_" + outputSignalName + "_y",wordsize   ) << " <= "
             << getShiftAndResizeString( InfoMap[ t->inputs[1] ] , wordsize, t->input_shifts[1],false) << ";";
        base_op->vhdl << base_op->declare( "add3_" + outputSignalName + "_z",wordsize   ) << " <= "
             << getShiftAndResizeString( InfoMap[ t->inputs[2] ] , wordsize, t->input_shifts[2],false) << ";";

        base_op->inPortMap("x_i","add3_" + outputSignalName + "_x");
        base_op->inPortMap("y_i","add3_" + outputSignalName + "_y");
        base_op->inPortMap("z_i","add3_" + outputSignalName + "_z");
        base_op->inPortMap("sel_i",decoder->outputSignalName + "(0)");
        base_op->outPortMap("sum_o",outputSignalName);
        base_op->vhdl << base_op->instance(add3,"ternAdd_"+outputSignalName);
    }
    else
    {
        base_op->vhdl << base_op->declare( "add3_" + outputSignalName + "_x",wordsize   ) << " <= "
             << getShiftAndResizeString( InfoMap[ t->inputs[0] ] , wordsize, t->input_shifts[0],false) << ";";
        base_op->vhdl << base_op->declare( "add3_" + outputSignalName + "_y",wordsize   ) << " <= "
             << getShiftAndResizeString( InfoMap[ t->inputs[1] ] , wordsize, t->input_shifts[1],false) << ";";
        base_op->vhdl << base_op->declare( "add3_" + outputSignalName + "_z",wordsize   ) << " <= "
             << getShiftAndResizeString( InfoMap[ t->inputs[2] ] , wordsize, t->input_shifts[2],false) << ";";
        ///TBD
        cerr << "ADDSUB3_2STATE: NODETYPE NOT SUPPORTED FOR NON VIRTEX TARGETS YET" << endl;
    }
    base_op->vhdl << getNegativeShiftString( outputSignalName,wordsize,t );
    return "";
}


// This is pre-5.0 code. It doesn't work with the new pipeline framework because inPortMap and outPortMap just build a temporary map
// F2D disabled it for now.
// The fix is to separate the code for each GenericAddSub, grouping it with its portMaps
string IntConstMultShiftAdd_ADDSUB3_CONF::get_realisation(map<adder_graph_base_node_t *, IntConstMultShiftAdd_BASE *> &InfoMap)
{
#if 0 // complain to F2D,  but it probably didn't work anyway
	conf_adder_subtractor_node_t* t = (conf_adder_subtractor_node_t*)(base_node);

    GenericAddSub* addsub = new GenericAddSub(base_op, target,wordsize);
    GenericAddSub* addsub2 = new GenericAddSub(base_op, target,wordsize);
    base_op->addSubComponent(addsub);
    base_op->addSubComponent(addsub2);
    for(int i=0;i<3;i++){
        IntConstMultShiftAdd_BASE* t_in=InfoMap[t->inputs[i]];
        string t_name = t_in->getTemporaryName();
        base_op->declare(t_name,wordsize);
        base_op->vhdl << t_name << " <= std_logic_vector(" << getShiftAndResizeString( t_in,wordsize,t->input_shifts[i]) << ");" << endl;
        if(i==0)
            base_op->inPortMap(addsub,"x_i",t_name);
        else if(i==1)
            base_op->inPortMap(addsub,"y_i",t_name);
        else
            base_op->inPortMap(addsub2,"y_i",t_name);
    }
    base_op->outPortMap     (addsub,"sum_o",outputSignalName+"_pre");
    base_op->inPortMap      (addsub,"neg_y_i",decoder->outputSignalName + "(1)");
    base_op->inPortMap      (addsub,"neg_x_i",decoder->outputSignalName + "(0)");
    base_op->inPortMap      (addsub2,"x_i",outputSignalName+"_pre");
    base_op->inPortMap      (addsub2,"neg_y_i",decoder->outputSignalName + "(2)");
    if( decoder->decoder_size == 4 ) base_op->inPortMap   (addsub2,"neg_x_i",decoder->outputSignalName + "(3)");
    else    base_op->inPortMapCst   (addsub2,"neg_x_i","'0'");
    base_op->outPortMap(addsub2,"sum_o",outputSignalName);
    base_op->vhdl << base_op->instance(addsub,outputSignalName+"_addsub_pre") << endl;
    base_op->vhdl << base_op->instance(addsub2,outputSignalName+"_addsub") << endl;

    base_op->vhdl << getNegativeShiftString( outputSignalName,wordsize,t ) << endl;
#endif
    return "";
}

string IntConstMultShiftAdd_AND::get_realisation(map<adder_graph_base_node_t *, IntConstMultShiftAdd_BASE *> &InfoMap)
{
    mux_node_t* t = (mux_node_t*)(base_node);
    short configurationIndex=-1;
    for(unsigned int i=0;i<t->inputs.size();i++)
    {
        if(t->inputs[i]!=NULL)
        {
            configurationIndex = i;
            break;
        }
    }
    base_op->vhdl << "\t" << base_op->declare(outputSignalName,wordsize) << " <= ";
    base_op->vhdl << getShiftAndResizeString(InfoMap[t->inputs[configurationIndex]]
            ,wordsize
            ,t->input_shifts[configurationIndex],false);
    base_op->vhdl << " when config_no=" << getBinary(configurationIndex,IntConstMultShiftAdd_BASE::configurationSignalWordsize);
    base_op->vhdl << " else (others=>'0')";
    base_op->vhdl << ";" << endl;
    return "";
}

string IntConstMultShiftAdd_MUX::get_realisation(map<adder_graph_base_node_t *, IntConstMultShiftAdd_BASE *> &InfoMap)
{
    GenericMux* new_mux = new GenericMux(base_op, target,wordsize,inputs.size());
    base_op->addSubComponent(new_mux);
    int i=0;
    for( vector<IntConstMultShiftAdd_muxInput>::iterator iter=inputs.begin();iter!=inputs.end();++iter ){
        if( iter->node == NULL )
        {
            if( iter->shift == -1 )
                base_op->inPortMapCst(new_mux->getInputName(i),"(others=>'-')");
            else
                base_op->inPortMapCst(new_mux->getInputName(i),"(others=>'0')");
        }
        else{
            IntConstMultShiftAdd_BASE* t_in=InfoMap[ iter->node ];
            string t_name = t_in->getTemporaryName();
            base_op->vhdl << base_op->declare(t_name,wordsize) << " <= " <<  getShiftAndResizeString(t_in
                                                    ,wordsize
                                                    ,iter->shift,false) << ";";
            base_op->inPortMap(new_mux->getInputName(i),t_name);
        }
        i++;
    }
    base_op->inPortMap(new_mux->getSelectName(),decoder->outputSignalName);
    base_op->outPortMap(new_mux->getOutputName(),outputSignalName);
    base_op->vhdl << base_op->instance(new_mux,outputSignalName+"_mux");

/*
    map<int,bool> negateConfigMap;

    base_op->vhdl << "\tWITH config_no SELECT" << endl;
    int outputSize=0;
    if(negateConfigMap.size()>0) outputSize = wordsize+1;
    else outputSize = wordsize;

    base_op->vhdl << "\t\t" << base_op->declare(outputSignalName,outputSize) << " <= \n";

    int muxStatesWritten=0;
    for(int inp=0;inp<(int)inputs.size();inp++)
    {
        IntConstMultShiftAdd_muxInput cur_inp = inputs[inp];
        int configsInMux = cur_inp.configurations.size();
        if( inp == ((int)inputs.size()) - 1 )
            configsInMux=1;
        for(int i=0;i<configsInMux;i++)
        {
            muxStatesWritten++;
            base_op->vhdl << "\t\t\t";
            if( cur_inp.node == NULL )
            {
                if(cur_inp.shift == -1)
                    base_op->vhdl << "(others=>'-'";
                else
                    base_op->vhdl << "(others=>'0'";

            }
            else if( negateConfigMap.find( cur_inp.configurations[i] )!=negateConfigMap.end() )
            {
                base_op->vhdl << "'1' & (";
                base_op->vhdl << getShiftAndResizeString(InfoMap[cur_inp.node]
                        ,wordsize
                        ,cur_inp.shift,false);
                base_op->vhdl << " xor \"";
                for( int x=0;x<wordsize;x++ )  base_op->vhdl << "1";
                base_op->vhdl << "\"";
            }
            else
            {
                if(negateConfigMap.size()>0) base_op->vhdl << "'0' & (";
                else base_op->vhdl << "(";

                base_op->vhdl << getShiftAndResizeString(InfoMap[cur_inp.node]
                        ,wordsize
                        ,cur_inp.shift,false);
            }
            base_op->vhdl << ") WHEN ";
            if( inp < ((int)inputs.size()) - 1 )
            {
                base_op->vhdl << getBinary(cur_inp.configurations[i],IntConstMultShiftAdd_BASE::configurationSignalWordsize) << "," << endl;
            }
            else
            {
                base_op->vhdl << "OTHERS;" << endl;
            }
        }
    }
*/
    return "";
}

	string IntConstMultShiftAdd_BASE::getShiftAndResizeString(IntConstMultShiftAdd_BASE *input_node, int outputWordsize, int inputShift,bool signedConversion)
{
    int neg_shift=0;
    if( is_a<adder_subtractor_node_t>(*base_node) ){
        adder_subtractor_node_t* t = (adder_subtractor_node_t*)base_node;
        for(uint i=0;i<t->input_shifts.size();i++){
            if(t->input_shifts[i]<neg_shift )
                neg_shift = t->input_shifts[i];
        }
    }
    else if( is_a<conf_adder_subtractor_node_t>(*base_node) ){
        conf_adder_subtractor_node_t* t = (conf_adder_subtractor_node_t*)base_node;
        for(uint i=0;i<t->input_shifts.size();i++){
            if(t->input_shifts[i]<neg_shift )
                neg_shift = t->input_shifts[i];
        }
    }

    stringstream tmp;
    if(!signedConversion) tmp << "std_logic_vector(";
    if(input_node->wordsize+inputShift > outputWordsize )
    {
        //log(2) << "WARNING: possible shift overflow @" << base_node->outputSignalName << endl;

        if( (inputShift-neg_shift) > 0)
        {
            tmp << "unsigned(shift_left(resize(signed(" << input_node->outputSignalName <<")," << outputWordsize << ")," << (inputShift-neg_shift) << "))" ;
        }
        else
        {
            tmp << "resize(unsigned(" << input_node->outputSignalName << ")," << outputWordsize << ")";
        }

    }
    else if( input_node->wordsize > outputWordsize )
    {
        if((inputShift-neg_shift) > 0)
        {
            tmp << "resize(unsigned(shift_left(signed(" << input_node->outputSignalName <<")," << (inputShift-neg_shift) << "))," << outputWordsize << ")" ;
        }
        else
        {
            tmp << "resize(unsigned(" << input_node->outputSignalName << ")," << outputWordsize << ")";
        }
    }
    else if( input_node->wordsize < outputWordsize )
    {
        if((inputShift-neg_shift) > 0)
        {
            tmp << "unsigned(shift_left(resize(signed(" << input_node->outputSignalName << ")," << outputWordsize << ")," << (inputShift-neg_shift) << "))";
        }
        else
        {
            tmp << "unsigned(resize(signed(" << input_node->outputSignalName << ")," << outputWordsize << "))";
        }
    }
    else
    {
        if((inputShift-neg_shift) > 0)
        {
            tmp << "unsigned(shift_left(signed(" << input_node->outputSignalName << ")," << (inputShift-neg_shift) << "))";
        }
        else
        {
            tmp << "unsigned(" << input_node->outputSignalName << ")";
        }
    }


    if(!signedConversion) tmp << ")";
    return tmp.str();
}


string IntConstMultShiftAdd_BASE::getNegativeShiftString(string signalName,int outputWordsize, adder_graph_base_node_t* base_node)
{
    int neg_shift=0;
    if( is_a<adder_subtractor_node_t>(*base_node) ){
        adder_subtractor_node_t* t = (adder_subtractor_node_t*)base_node;
        for(uint i=0;i<t->input_shifts.size();i++){
            if(t->input_shifts[i]<neg_shift )
                neg_shift = t->input_shifts[i];
        }
    }
    else if( is_a<conf_adder_subtractor_node_t>(*base_node) ){
        conf_adder_subtractor_node_t* t = (conf_adder_subtractor_node_t*)base_node;
        for(uint i=0;i<t->input_shifts.size();i++){
            if(t->input_shifts[i]<neg_shift )
                neg_shift = t->input_shifts[i];
        }
    }

    if(neg_shift < 0)
    {
        stringstream tmp;
        tmp << signalName << "_ns";
        this->wordsize -= abs(neg_shift);
        base_op->declare( tmp.str(),this->wordsize );
        tmp << " <= std_logic_vector( resize(shift_right(signed(" << signalName << "),"<< abs(neg_shift) << "),"<< this->wordsize <<") );"<<endl;
        outputSignalName += "_ns";
        return "\t" + tmp.str();
    }
    return "";
}

string IntConstMultShiftAdd_BASE::getNegativeResizeString(string signalName,int outputWordsize)
{
    if(outputWordsize != this->wordsize)
    {
        stringstream tmp;
        tmp << signalName << "_nr";
        base_op->declare( tmp.str(),this->wordsize );
        tmp << " <= std_logic_vector( resize(signed(" << signalName << "),"<< this->wordsize <<") );"<<endl;
        outputSignalName += "_nr";
        return "\t" + tmp.str();
    }
    return "";
}

void IntConstMultShiftAdd_BASE::getInputOrder(vector<bool> &inputIsNegative, vector<int> &inputOrder)
{
    for(int i=0;i<(int)inputIsNegative.size();i++)
    {
        if( inputIsNegative[i] )
        {
            inputOrder.push_back(i);
        }
        else
        {
            inputOrder.insert(inputOrder.begin(),i);
        }
    }
}

void IntConstMultShiftAdd_BASE::build_operand_realisation(
			map<PAGSuite::adder_graph_base_node_t*,IntConstMultShiftAdd_BASE*>& InfoMap
		)
{
    adder_subtractor_node_t* t = (adder_subtractor_node_t*)(base_node);
	size_t nb_inputs = t->inputs.size();

    vector<size_t> inputOrder(nb_inputs);
	vector<int> opsize_word(nb_inputs);
	vector<int> opsize_shifted_word(nb_inputs);
	vector<int> known_zeros(nb_inputs);
	vector<IntConstMultShiftAdd_BASE*> inputs_info(nb_inputs);
	vector<string> input_sig_names(nb_inputs);

	for (size_t i = 0 ; i < nb_inputs ; ++i) {
		inputs_info[i] = InfoMap[t->inputs[i]];
		IntConstMultShiftAdd_BASE& currentInput = *(inputs_info[i]);
		int cur_shift = (t->input_shifts[i] < 0) ? 0 : t->input_shifts[i];
		opsize_word[i] = currentInput.wordsize;
		opsize_shifted_word[i] = currentInput.wordsize + cur_shift;
		known_zeros[i] = truncations[i] + cur_shift;
		inputOrder[i] = i;
		input_sig_names[i] = currentInput.outputSignalName;
	}
	
	int finalRightShifts = (t->input_shifts[0] < 0) ? -1 * t->input_shifts[0] : 0;

	auto comp = [known_zeros](const size_t& a, const size_t& b)->bool{
		return known_zeros[a] < known_zeros[b];
	};

	sort(inputOrder.begin(), inputOrder.end(), comp);

	int min_kz = known_zeros[inputOrder[0]];
	int second_min_kz = known_zeros[inputOrder[1]];
	int copy_as_is_boundary = (t->input_is_negative[inputOrder[0]]) ? min_kz : second_min_kz;

	////// Handle the non-added part of the result //////////
	size_t min_sig_idx = inputOrder[0];
	ostringstream copy_as_is;
	if (copy_as_is_boundary != min_kz) {
		// We have some non-zero "free" bits
		int nb_as_is_expand_sign = max(0, copy_as_is_boundary - opsize_shifted_word[min_sig_idx]);
		if (nb_as_is_expand_sign > 0) {
			//We have some sign expansion bit
			copy_as_is << "( " << (nb_as_is_expand_sign - 1) << " downto 0 => " <<
				input_sig_names[min_sig_idx] << of(opsize_word[min_sig_idx] - 1) <<
				") & ";
		}
		//Copy the previously computed bits
		int nb_useful_bits = min(
				opsize_word[min_sig_idx] - truncations[min_sig_idx], 
				copy_as_is_boundary - known_zeros[min_sig_idx]
			);
		copy_as_is << input_sig_names[min_sig_idx] << range(
				truncations[min_sig_idx] + nb_useful_bits - 1,
				truncations[min_sig_idx]
			);
	}

	if (known_zeros[min_sig_idx] > 0) {
		if (copy_as_is.str().length() > 0) {
			copy_as_is << " & ";
		}
		copy_as_is << zg(known_zeros[min_sig_idx]);
	}

	string copy_as_is_signame = outputSignalName + "_as_is";
	if (copy_as_is.str().length() > 0) {
		base_op->vhdl << "\t" << base_op->declare(.0, copy_as_is_signame, copy_as_is_boundary) <<
			" <= " << copy_as_is.str() << ";" << endl;
	}

	string msb_signame = "msb_" + outputSignalName;
	int  adder_word_size = wordsize - copy_as_is_boundary + finalRightShifts;
	//TODO
	vector<string> operands(nb_inputs);
	for (size_t i = 0 ; i < nb_inputs ; ++i) {
		ostringstream cur_sig_msb;
		IntConstMultShiftAdd_BASE& currentInput = *(inputs_info[i]);
		
		int left_boundary = max(copy_as_is_boundary, opsize_shifted_word[i]);
		int right_boundary = max(copy_as_is_boundary, known_zeros[i]);
		int sign_ext_left = wordsize - left_boundary;
		int pad_zeros_right = right_boundary - copy_as_is_boundary;
		int useful_bits = left_boundary - right_boundary;

		if (sign_ext_left > 0) {
			cur_sig_msb << "( " << (sign_ext_left - 1) << " downto 0 => " <<
				currentInput.outputSignalName << of(currentInput.wordsize - 1) << ")";
		}

		if (useful_bits > 0) {
			int start_select_idx = right_boundary - t->input_shifts[i];
			if (cur_sig_msb.str().length() > 0) {
				cur_sig_msb << " & ";	
			}
			cur_sig_msb << currentInput.outputSignalName <<
				range(start_select_idx + useful_bits - 1, start_select_idx);
		}

		if (pad_zeros_right > 0) {
			cur_sig_msb << " & " << zg(pad_zeros_right);
		}
		
		string signal_name = inputs_info[i]->getTemporaryName();
		
		base_op->vhdl << "\t" << base_op->declare(0., signal_name, adder_word_size) <<
			" <= " << cur_sig_msb.str() << ";" << endl;
		operands[i] = signal_name;
	}

	int flag = (nb_inputs == 3) ? GenericAddSub::TERNARY : 0;
	if (t->input_is_negative[0]) {
		flag |= GenericAddSub::SUB_LEFT;
	}

	if (t->input_is_negative[1]) {
		flag |= (nb_inputs == 3) ? GenericAddSub::SUB_MID : GenericAddSub::SUB_RIGHT;
	}

	if (nb_inputs > 2 && t->input_is_negative[2]) {
		flag |= GenericAddSub::SUB_RIGHT;
	}

    GenericAddSub* add = new GenericAddSub(
			base_op,
			target,
			adder_word_size,
			flag
		);

    base_op->addSubComponent(add);
	for (size_t i = 0 ; i < nb_inputs ; ++i) {
		base_op->inPortMap(add->getInputName(i), operands[i]);
	}
	base_op->outPortMap(add->getOutputName(), msb_signame);

    base_op->vhdl << base_op->instance(add,"generic_add_sub_"+outputSignalName);
	
	string leftshiftedoutput = outputSignalName + "_tmp";

	base_op->vhdl << "\t" << declare(leftshiftedoutput, wordsize + finalRightShifts) << " <= " << 
		msb_signame;

	if (copy_as_is.str().size() > 0) {
		base_op->vhdl << " & " << copy_as_is.str();	
	}

	base_op->vhdl << ";" << endl;

	base_op->vhdl << "\t" << declare(outputSignalName, wordsize) << " <= " <<
		leftshiftedoutput;
	if (finalRightShifts > 0) {
		base_op->vhdl << range(finalRightShifts + wordsize - 1, finalRightShifts);
	}
	base_op->vhdl << ";" << endl;
}


string IntConstMultShiftAdd_BASE::getBinary(int value, int Wordsize)
{
    string tmp;
    while(value>0)
    {
        if(value%2) tmp.insert(0,1,'1');
        else tmp.insert(0,1,'0');
        value = (value>>1);
    }
    while((int)tmp.length() < Wordsize) tmp.insert(0,1,'0');

    tmp.insert(0,1,'\"');
    tmp += '\"';
    return tmp;
}

string IntConstMultShiftAdd_BASE::declare(string signalName, int wordsize)
{
    return base_op->declare(signalName,wordsize);
}

string IntConstMultShiftAdd_BASE::getTemporaryName()
{
    stringstream t_name;
    t_name << outputSignalName << "_t" << ++outputSignalUsageCount;
    return t_name.str();
}

}


#endif // HAVE_PAGSUITE
