#ifndef __LEXER_CONTEXT_HPP__
#define __LEXER_CONTEXT_HPP__

#include <iostream>
#include <vector>

#include "utils.hpp"

using namespace std;
using namespace flopoco;
namespace flopoco {
	class Operator;
}

class LexerContext {
public:
	typedef enum {
		unset,
		signalAssignment, // after a <= 
		conditionalSignalAssignment, // after a when 
		selectedSignalAssignment,
		selectedSignalAssignment2,
		selectedSignalAssignment3,
		variableAssignment, 
        portmap,  // within a port map()
        portmap2, // item but in second parse
        genericmap,  // within a generic map()
        process,
		caseStatement,
		comment
	} LexMode;

	Operator* op;
	void* scanner;
	istream* is;
	ostream* os;

	string *lhsName;
	vector<string> *extraRhsNames;
	vector<triplet<string, string, int>> *dependenceTable;

	LexMode *lexingMode;
	LexMode *lexingModeOld;

	bool *isLhsSet;

public:
	LexerContext(Operator* op_, istream* is, ostream* os,
			string *lhsName_, vector<string> *extraRhsNames_, vector<triplet<string, string, int>> *dependenceTable_,
			LexMode *lexingMode_, LexMode *lexingModeOld_, bool *isLhsSet_) {
		op=op_;
		init_scanner();
		this->is = is;
		this->os = os;

		this->lhsName = lhsName_;
		this->extraRhsNames = extraRhsNames_;
		this->dependenceTable = dependenceTable_;
		this->lexingMode = lexingMode_;
		this->lexingModeOld = lexingModeOld_;
		this->isLhsSet = isLhsSet_;
	}

	//these methods are generated in VHDLLexer.cpp 

	void lex();

	virtual ~LexerContext() { destroy_scanner();}

protected:
	void init_scanner();

	void destroy_scanner();
};



#endif
