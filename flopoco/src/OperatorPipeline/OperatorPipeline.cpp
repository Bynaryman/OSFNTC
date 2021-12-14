

#include "OperatorPipeline.hpp"

//*
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <math.h>
#include <string.h>
#include <gmp.h>
#include <mpfr.h>
#include <cstdlib>
#include <gmpxx.h>
#include "utils.hpp"
//*/
//#ifdef HAVE_SOLLYA

//*
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sstream>


///TODO I used an implementation of this here so it can compile. Not sure if it realy have to be done here
int FlopocoExpressionlex(void){

}

Program program;


#include "structure_donnees/OPVariable.hpp"
#include "structure_donnees/OPOperation.hpp"
#include "structure_donnees/OPFunction.hpp"
#include "structure_donnees/OPExpression.hpp"
#include "structure_donnees/File.hpp"
#include "structure_donnees/lex.operator_pipeline.hpp"
#include "structure_donnees/operator_pipeline.tab.hpp"


#include <vector>
#include <map>

using namespace OperatorPipeline;
using namespace std;

map<std::string,OPVariable*> variables;
vector<OPVariable*> returns;
vector<OPExpression*> expressions;

void init_test_data(int sample)
{

    map<std::string,OPVariable**> variables_inter;
    vector<OPVariable**> returns_inter;
    vector<OPExpression**> expressions_inter;

    /*OPVariable *v1,*a, *b, *cinq;
    vector<int>* param1=new std::vector<int>();
    vector<int>* param2=new std::vector<int>();

    param1->push_back(5);
    param1->push_back(12);
    param2->push_back(32);

    cinq = new OPVariable("5", SINT, param2, NULL);

    a = new OPVariable("a", SINT, param2, NULL);
    b = new OPVariable("b", SINT, param2, NULL);

    variables_inter["v1"] = &v1;
    variables_inter["b"] = &b;
    variables_inter["a"] = &a;

    returns_inter.push_back(&v1);

    // première opération, à répéter avec les autres
    OPOperation* op1 = new OPOperation(cinq, b, PLUS);
    OPOperation* op2 = new OPOperation(a, op1, MOINS);
    v1 = new OPVariable("", FLOAT, param1, op2);
    expressions_inter.push_back((OPExpression**)&v1);*/


    OPVariable *v1, *v2, *r1, *r2, *a, *b, *five, *six, *seven, *eight;
    std::vector<int>* float_param=new std::vector<int>();;
    std::vector<int>* int_param=new std::vector<int>();;


    float_param->push_back(5);
    float_param->push_back(12);
    int_param->push_back(32);


    switch (sample)
    {
        case 1: // assign
        {

            a = new OPVariable("a", SINT, int_param, NULL);

            variables_inter["v1"] = &v1;
            variables_inter["a"] = &a;

            v1 = new OPVariable("v1", SINT, int_param, a);

            returns_inter.push_back(&v1);
            break;
        }
        case 2: // plus_minus
        {
            five = new OPVariable("5", SINT, int_param, NULL);
            a = new OPVariable("a", SINT, int_param, NULL);
            b = new OPVariable("b", SINT, int_param, NULL);

            variables_inter["v1"] = &v1;
            variables_inter["b"] = &b;
            variables_inter["a"] = &a;

            OPOperation* op1 = new OPOperation(five, b, OP_PLUS);
            OPOperation* op2 = new OPOperation(a, op1, OP_MOINS);
            v1 = new OPVariable("", FLOAT, float_param, op2);
            expressions_inter.push_back((OPExpression**)&v1);

            returns_inter.push_back(&v1);
            break;
        }
        case 3: // long_plus
        {
            five = new OPVariable("5", SINT, int_param, NULL);
            six  = new OPVariable("6", SINT, int_param, NULL);
            seven = new OPVariable("7", SINT, int_param, NULL);
            eight = new OPVariable("8", SINT, int_param, NULL);

            a = new OPVariable("a", SINT, int_param, NULL);
            b = new OPVariable("b", SINT, int_param, NULL);

            variables_inter["v1"] = &v1;
            variables_inter["r1"] = &r1;
            variables_inter["b"] = &b;
            variables_inter["a"] = &a;

            OPOperation* a_5 = new OPOperation(a, five, OP_PLUS);
            OPOperation* a_5_b = new OPOperation(a_5, b, OP_PLUS);
            OPOperation* a_5_b_6 = new OPOperation(a_5_b, six, OP_PLUS);
            OPOperation* a_5_b_6_7 = new OPOperation(a_5_b_6, seven, OP_PLUS);
            r1 = new OPVariable("r1", FLOAT, float_param, a_5_b_6_7);
            expressions_inter.push_back((OPExpression**)&r1);

            OPOperation* r1_6 = new OPOperation(r1, six, OP_PLUS);
            OPOperation* r1_6_7 = new OPOperation(r1_6, seven, OP_PLUS);
            OPOperation* r1_6_7_8 = new OPOperation(r1_6_7, eight, OP_PLUS);
            v1 = new OPVariable("v1", FLOAT, float_param, r1_6_7_8);
            expressions_inter.push_back((OPExpression**)&v1);

            returns_inter.push_back(&v1);
        }
        case 4: // double_return
        {

            a = new OPVariable("a", SINT, int_param, NULL);
            b = new OPVariable("b", SINT, int_param, NULL);

            variables_inter["v1"] = &v1;
            variables_inter["v2"] = &v2;
            variables_inter["r1"] = &r1;
            variables_inter["b"] = &b;
            variables_inter["a"] = &a;

            OPOperation* op_r1 = new OPOperation(a, b, OP_PLUS);
            r1 = new OPVariable("r1", FLOAT, float_param, op_r1);
            expressions_inter.push_back((OPExpression**)&r1);

            OPOperation* op_v1 = new OPOperation(r1, b, OP_PLUS);
            v1 = new OPVariable("v1", FLOAT, float_param, op_v1);
            expressions_inter.push_back((OPExpression**)&v1);

            OPOperation* op_v2 = new OPOperation(r1, a, OP_PLUS);
            v2 = new OPVariable("v2", FLOAT, float_param, op_v2);
            expressions_inter.push_back((OPExpression**)&v2);

            returns_inter.push_back(&v1);
            returns_inter.push_back(&v2);
        }
    }



    //now we put all that into the one * data structure
    for(map<std::string,OPVariable**>::const_iterator iterator = variables_inter.begin(); iterator != variables_inter.end(); ++iterator) {
        variables[iterator->first]=*(iterator->second);
    }

    for (int i=0;i<(int)returns_inter.size();++i){
        returns.push_back(*(returns_inter[i]));
    }


    for (int i=0;i<(int)expressions_inter.size();++i){
        expressions.push_back(*(expressions_inter[i]));
    }

}

void compute_node(Node* node, OPExpression* exp){

    OPFunction* op_func=dynamic_cast<OPFunction*>(exp);
    OPOperation* op_op=dynamic_cast<OPOperation*>(exp);
    OPVariable* op_var=dynamic_cast<OPVariable*>(exp);


    if (op_func!=NULL){
        if (op_func->function()==LOG){
            node->type_id=OpLog;
        }else if (op_func->function()==EXP){
            node->type_id=OpExp;
        }else{
            std::cerr<<"compute_node: this type of function is not yet handled"<<std::endl;
        }

        //a function only have one child node
        Node* child=new Node();

        //we link it
        node->childrens.push_back(child);
        child->parents.push_back(node);

        //and we continue to progress in the tree
        compute_node(child,op_func->operande());


    }else if (op_op!=NULL){


        if (op_op->get_operator()==OP_PLUS){
            node->type_id=OpAdd;
        }else if (op_op->get_operator()==OP_MOINS){
            node->type_id=OpSub;
        }else if (op_op->get_operator()==OP_MUL){
            node->type_id=OpMult;
        }else if (op_op->get_operator()==OP_DIV){
            node->type_id=OpDiv;
        }else{
            std::cerr<<"compute_node: this type of function is not yet handled"<<std::endl;
        }


        //we handle the left child
        Node* child_left=new Node();

        //we link it
        node->childrens.push_back(child_left);
        child_left->parents.push_back(node);

        //and we continue to progress in the tree
        compute_node(child_left,op_op->op_left());



        //we handle the right child
        Node* child_right=new Node();

        //we link it
        node->childrens.push_back(child_right);
        child_right->parents.push_back(node);

        //and we continue to progress in the tree
        compute_node(child_right,op_op->op_right());



    }else if(op_var!=NULL){


        std::string name=op_var->GetValue();

        if (program.variables.count(name)){
            //mean we hav to use another defined variable
            //we just need to link it as a children

            //tw cases are possible here:
            //either we are in the case of an assignment
            //either we reached a point of the chain where we call another variable

            //we check if the current node is a variable (so we will know if we are in the case of an assignment
            bool is_assing=false;
            for(map<std::string,Node*>::iterator iterator = program.variables.begin(); iterator != program.variables.end(); ++iterator) {
                Node* inter=iterator->second;
                if (node==inter){
                    is_assing=true;
                    break;
                }
            }

            if (is_assing){
                node->type_id=OpAssign;
                node->childrens.push_back(program.variables[name]);
                program.variables[name]->parents.push_back(node);
            }else{

                //we have to remove the virtual node that we created first then link the parent to the desired variable
                for (int i=0;i<(int)node->parents.size();++i){
                    Node* parent=node->parents[i];

                    parent->rmv_child(node);
                    parent->childrens.push_back(program.variables[name]);
                    program.variables[name]->parents.push_back(parent);
                }
            }

        }else{

            if (op_var->GetExpression()!=NULL){
                std::cerr<<"compute_node: detected a variable with expression that is not listed in the variable list"<<std::endl;
                return;
            }

            //it means we have found a constant
            //so we create a constant and link it

            OperatorConstant* constant=new OperatorConstant();
            constant->operator_type=OpConst;

            std::vector<int> param=op_var->type_param();

            if (op_var->GetType()==UINT){
                Integer* integ=new Integer();
                integ->is_signed=false;
                integ->size=param[0];

                //I need to read the value
                istringstream iss;
                iss.str(name);
                iss>>integ->value;

                constant->nbr=integ;

            }else if (op_var->GetType()==SINT){
                Integer* integ=new Integer();
                integ->is_signed=false;
                integ->size=param[0];

                //I need to read the value
                istringstream iss;
                iss.str(name);
                iss>>integ->value;

                constant->nbr=integ;
            }else if (op_var->GetType()==FLOAT){
                FloatingPointNumber* fp=new FloatingPointNumber();
                fp->mantissa_size=param[0];
                fp->exponent_size=param[1];

                //I need to read the value
                istringstream iss;
                iss.str(name);
                //iss>>integ->value;
                ///TODO here parse the floating point constant

                constant->nbr=fp;

            }else{
                std::cerr<<"compute_node: this kind of entry variable is not yet handled"<<std::endl;
            }


            for (int i=0;i<(int)node->parents.size();++i){
                Node* parent=node->parents[i];

                parent->rmv_child(node);
                parent->childrens.push_back(constant);
                constant->parents.push_back(parent);
            }
        }


    }else{
        std::cerr<<"compute_node: unknown case in the data structure"<<std::endl;
    }
}


int FlopocoExpressionparse(std::string filename){
    std::string line;
    std::ifstream myfile (filename.c_str());
    /*if (myfile.is_open())
    {
        while ( getline (myfile,line) )
        {
            std::cout << line << '\n';
        }
        myfile.close();
    }

    else{
        std::cout << "Unable to open file";
        throw 5;
    }*/


   //*
    FILE* file;
    File data;
    file=fopen(filename.c_str(), "r");
    operator_pipelinein=file;
    int retour=operator_pipelineparse(data);
    fclose(file);

    //now we put all that into the one * data structure
    for(map<std::string,OPVariable**>::const_iterator iterator = data.variables.begin(); iterator != data.variables.end(); ++iterator) {
        variables[iterator->first]=*(iterator->second);
    }

    for (int i=0;i<(int)data.returns.size();++i){
        returns.push_back((data.returns[i]));
    }


    for (int i=0;i<(int)data.expressions.size();++i){
        expressions.push_back((data.expressions[i]));
    }
    //*/

    //init_test_data(4);//only for the tests

    //here we should have the data structure from aurelien filled
    //now we will convert it to the generic tree data structure.

    //while parsing I have to be carefull for the case where I find one varible that is associated with
    //an experession. That's why while parsing I'll try casting to Opvariable type to check if it's a varible

    //first i'll create all the variables
    for(map<std::string,OPVariable*>::const_iterator iterator = variables.begin(); iterator != variables.end(); ++iterator) {
        program.variables[iterator->first]=new Node();
    }


    std::cout<<"created the variables"<<program.variables.size()<<std::endl;

    //now for each variable i'll create th associated tree (and also initialise the variable parameters btw
    //we stop when we reach another variable
    for(map<std::string,Node*>::iterator iterator = program.variables.begin(); iterator != program.variables.end(); ++iterator) {
        OPVariable* var=variables[iterator->first];
        OPExpression* expr=var->GetExpression();



        Node* node=iterator->second;
        if (expr==NULL){
            //in this case we found an entry variable

            EntryVariable* entry_var=new EntryVariable();

            std::vector<int> param=var->type_param();



            if (var->GetType()==UINT){
                Integer* integ=new Integer();
                integ->is_signed=false;
                integ->size=param[0];
                entry_var->nbr=integ;
            }else if (var->GetType()==SINT){
                Integer* integ=new Integer();
                integ->is_signed=false;
                integ->size=param[0];
                entry_var->nbr=integ;
            }else if (var->GetType()==FLOAT){
                FloatingPointNumber* fp=new FloatingPointNumber();
                fp->mantissa_size=param[0];
                fp->exponent_size=param[1];
                entry_var->nbr=fp;

            }else{
                std::cerr<<"FlopocoExpressionparse: this kind of entry variable is not yet handled"<<std::endl;
            }

            //now we need to put the entry variable in the structure
            //the problem is that it might already have been used (meaning there might be another expression that linked it
            //so i'll iterate on the parents and change that link
            for (int i=0;i<node->nbr_parents();++i){
                Node* parent=node->parents[i];
                for (int j=0;j<parent->nbr_childrens();++j){
                    if (parent->childrens[i]==node){
                        parent->childrens[i]=entry_var;
                    }
                }
            }
            entry_var->parents=node->parents;

            //now we can replace the node with the new object in the map
            program.variables[iterator->first]=entry_var;


        }else{

            //we launch the parsing of the experession of the variable
            compute_node(node,expr);

        }

    }

    //now we tell the system which node is a return node (meaning the to of the tree
    for(map<std::string,OPVariable*>::const_iterator iterator = variables.begin(); iterator != variables.end(); ++iterator) {
        bool found=false;
        for (int i=0;i<(int)returns.size();++i){
            if (iterator->second==returns[i]){
                found=true;
                break;
            }
        }
        if (found){
            program.return_heads.push_back(program.variables[iterator->first]);
        }

    }


    //now i'll initialize the depth of the nodes.
    program.init_depth();


}

/** This functions declares and returns a pointer to an the proper
    operator for a given node.
    It also declares the intermediate signals after the declaration.
    Designed for recursive algorithms only.
  */
//Operator* instanciateOp(node n){
//	Operator* op;
//	if ( !strcmp(n->operation, "add") )
//		op=new Operator();
//		oplist.push_back(op);
//		tmp << "adder" << getNewUId();
//		tmpX << tmp << "X";
//		tmpY << tmp << "Y";
//		tmpR << tmp << "X";
//
//		inPortMap( op1, tmpX.cstr());
//		inPortMap( op1, tmpY.cstr());
//		outPortMap( op1, tmpZ.cstr());
//		ostringstream tmp;
//		vhdl << instance(op1, tmp.str())<<endl;
//	else if ( !strcmp(n->operation, "") )
//		op=new Operator();
//	else if ( !strcmp(n->operation, "") )
//		op=new Operator();
//	else
//		return null;
//
//	return op;
//}


/** This functions performs a DAG walk on the IR, instanciates operators
    and intermediate signals.
    int buildPhysicalDAG(const Dag dag, std::list<Operators>* OpList){
        int err=0;
        while ( !err ){
            err = buildPhysicalDAG(dag->child);
            OpList.insert( instanciateOp(dag) );
            if (OpList.top == null)
                return 1;
        }
    }
 */

//*/


//#define sumeofsquaresNaive
//#define sumeofsquares
//#define polynomial
//#define sqrtx2y2
//#define sqrtx2y2z2

using namespace std;

namespace flopoco{

OperatorPipeline::OperatorPipeline(Target* target, string filename, bool fortran_enabled_i, bool use_multi_entry_enabled_i, bool allow_reordering_i):
    Operator(target),fortran_enabled(fortran_enabled_i) ,use_multi_entry_enabled(use_multi_entry_enabled_i),allow_reordering(allow_reordering_i) {
    // Name HAS to be unique!
    // will cause weird bugs otherwise

    ostringstream complete_name;
    complete_name << "OperatorPipeline" << getNewUId();
    setNameWithFreqAndUID(complete_name.str());
    // r = x^2 + y^2 + z^2 example
    srcFileName = "OperatorPipeline";

    // redirect stdin to the file pointer
    //int my_stdin = dup(0);
    //close(0);
    //int fp = open(filename.c_str(), O_RDONLY, "r");

    //dup2(fp, 0);
    try{
        FlopocoExpressionparse(filename);
    }catch (int e){
        exit(0);
    }

    //close(fp);
    //dup2(0, my_stdin);
    optimise_tree();

    exit(0);
    /*
    REPORT(DEBUG, "-----------------------------------");
    nodeList* head = p->assignList;
    while (head!=NULL){
        printExpression(head->n);
        REPORT(DEBUG,endl);
        head = head->next;
    }
    REPORT(DEBUG, "-----------------------------------");
    varList* headv = p->outVariableList;
    while (headv != NULL){
        REPORT(DEBUG, "out: variable " << headv->name	<< ";");
        headv = headv->next;
    }
    REPORT(DEBUG, "-----------------------------------");
    head = p->assignList;
    // creates the computational tree our of the assignment list, by linking
    //       all variables already declared to their use
    makeComputationalTree(NULL, head, head);

    REPORT(DEBUG, "NEW NODES: ------------------------");
    head = p->assignList;
    while (head!=NULL){
        printExpression(head->n);
        REPORT(DEBUG,endl);
        head = head->next;
    }
    REPORT(DEBUG, "-----------------------------------");

    // create a node output list, where each node is a computational datapath.
    //       if in the user-provided outputList, some of the variables are part of
    //       intermediary computations for one, say larger, node these will not be added
    //       to the new list

    nodeList* outList = createOuputList(p->assignList, p->outVariableList);

    REPORT(DEBUG, "PROPER OUT LIST: ------------------");
    nodeList* outListHead = outList;
    while (outListHead != NULL){
        printExpression( outListHead->n);
        outListHead = outListHead->next;
    }
    REPORT(DEBUG,endl);

    nodeList* oh = outList;





    while (oh!=NULL){
        generateVHDL_c( oh->n, true);
        oh = oh->next;
    }*/
}

OperatorPipeline::~OperatorPipeline() {
}

void OperatorPipeline::optimise_tree(){

}

void OperatorPipeline::generateVHDL_c(Node* n, bool top){
    /* REPORT(DETAILED, "Generating VHDL ... ");

    if (n->type == 0){
        //we start at cycle 0, for now
        setCycle(0);
        //check if inputs are already declared. if not declare the inputs
        if (n->name!=NULL){
            if (!isSignalDeclared(n->name)){
                REPORT(DETAILED, "signal " << n->name << "   declared");
                addFPInput(n->name, wE, wF);
            }
        }else{
            //this is a constant, so it has no name, and is not declared
        }
    }else{
        //iterate on all inputs
        nodeList* lh = n->nodeArray;
        while (lh!=NULL){
            generateVHDL_c(lh->n, false);
            lh=lh->next;
        }
        lh = n->nodeArray;
        while (lh!=NULL){
            if (lh->n->name!=NULL)
                syncCycleFromSignal(lh->n->name);
            lh=lh->next;
        }
        REPORT(DETAILED, "finished with node");
    }

    bool hadNoName = (n->name==NULL);

    if (n->name==NULL){
        //assign a unique name;
        ostringstream t;
        t << "tmp_var_"<<getNewUId();
        string w = t.str();
        char *c  = new char[t.str().length()+1];
        c = strncpy(c, t.str().c_str(), t.str().length() );
        c[t.str().length()]=0;
        REPORT(DETAILED, " new temporary variable created "<< c <<" size="<<t.str().length());
        n->name = c;
        REPORT(DETAILED, " the value was created for the constant " << n->value);
    }

    if ((hadNoName)&&(n->type == 0)){
        //it is a constant_expr
        mpfr_t mpx;
        mpfr_init2 (mpx, wF+1);
        mpfr_set_str (mpx, n->s_value, 10, GMP_RNDN);
        vhdl << tab << declare(n->name, wE+wF+3) << " <= \""<<fp2bin(mpx, wE, wF)<< "\";"<<endl;
    }

    ostringstream t;
    if (n->isOutput){
        t << "out_" << n->name;
        addFPOutput(t.str(), wE, wF);
    }


    Operator* op1;
    //let's instantiate the proper operator
    //here we add the transformations from our tree to vhdl
    //I left the fpipeline case 1 as an exemple

    switch (n->type)
    {
        case 0:{  //input
            break;
        }
        case 1:{ //adder
            if (false){

                REPORT(DETAILED, " instance adder");

                op1 = new FPAddSinglePath(target_, wE, wF, wE, wF, wE, wF);
                oplist.push_back(op1);

                inPortMap( op1, "X", n->nodeArray->n->name);
                inPortMap( op1, "Y", n->nodeArray->next->n->name);
                outPortMap( op1, "R", n->name);

                ostringstream tmp;
                tmp << "adder" << getNewUId();
                vhdl << instance(op1, tmp.str())<<endl;
            }

            break;
        }

        default:{

            cerr << "nothing else implemented yet for operation code: "<<n->type << endl;
            exit(-1);
        }
    }

    if (n->isOutput){
        syncCycleFromSignal(n->name);
        nextCycle();
        vhdl << tab << "out_"<<n->name << " <= " << n->name << ";" << endl;
    }*/

};


}
//#endif //HAVE_SOLLYA


