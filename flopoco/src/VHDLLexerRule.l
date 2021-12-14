%option reentrant
%option prefix="Flopoco_"
%option noyywrap
%option yylineno


%{
	#include "LexerContext.hpp"
	#include "Operator.hpp"
	#include <stdio.h>
	#include <iostream>
	#include <string.h>
	#include <utility>
	#include <vector>
	#include <ctype.h>
	#include <map>
	
	using namespace flopoco;

	#define YY_EXTRA_TYPE LexerContext*
	#define YY_INPUT(buf, result, max_size) {\
		char c; \
		(*yyextra->is).get(c); \
		if (yyextra->is->eof()) \
			result = YY_NULL; \
		else { \
			buf[0] = c; \
			result = 1; \
		}\
	}


%}

a		[aA]
b		[bB]
c		[cC]
d		[dD]
e		[eE]
f		[fF]
g		[gG]
h		[hH]
i		[iI]
j		[jJ]
k		[kK]
l		[lL]
m		[mM]
n		[nN]
o		[oO]
p		[pP]
q		[qQ]
r		[rR]
s		[sS]
t		[tT]
u		[uU]
v		[vV]
w		[wW]
x		[xX]
y		[yY]
z		[zZ]


upper_case_letter         [A-Z]
lower_case_letter         [a-z]
digit                     [0-9]
special_character         [\#\&\'\(\)\*\+\,\-\.\/\:\;\<\=\>\_\|]
space_character           [ \t]
format_effector           [\t\v\r\l\f]
end_of_line               \n
other_special_character   [\!\$\@\?\[\\\]\^\`\{\}\~]

graphic_character         ({basic_graphic_character}|{lower_case_letter}|{other_special_character})
basic_graphic_character   ({upper_case_letter}|{digit}|{special_character}|{space_character})
letter                    ({upper_case_letter}|{lower_case_letter})
letter_or_digit           ({letter}|{digit})
decimal_literal           (\+|\-)?{integer}(\.{integer})?({exponent})?
integer                   {digit}(_?{digit})*
exponent                  ([eE][-+]?{integer})
base                      {integer}
based_integer             {extended_digit}(_?{extended_digit})*
extended_digit            ({digit}|[a-fA-F])
base_specifier            (B|b|O|o|X|x)

identifier                ({letter}(_?{letter_or_digit})*)
delayed_identifier        ({identifier}\^({digit})+)
delayed_identifier2       ({identifier}\^^({digit})+)
label					  ({identifier}{space_character}*":")


%%

({a}{b}{s}) |
({a}{c}{c}{e}{s}{s}) |
({a}{f}{t}{e}{r}) |
({a}{l}{i}{a}{s}) |
({a}{l}{l}) |
({a}{n}{d}) |
({a}{r}{c}{h}{i}{t}{e}{c}{t}{u}{r}{e}) |
({a}{r}{r}{a}{y}) |
({a}{s}{s}{e}{r}{t}) |
({a}{t}{t}{r}{i}{b}{u}{t}{e}) |
({b}{e}{g}{i}{n}) |
({b}{l}{o}{c}{k}) |
({b}{o}{d}{y}) |
({b}{u}{f}{f}{e}{r}) |
({b}{u}{s}) |
({c}{o}{m}{p}{o}{n}{e}{n}{t}) |
({c}{o}{n}{f}{i}{g}{u}{r}{a}{t}{i}{o}{n}) |
({c}{o}{n}{s}{t}{a}{n}{t}) |
({d}{i}{s}{c}{o}{n}{n}{e}{c}{t}) |
({d}{o}{w}{n}{t}{o}) |
({e}{l}{s}{e}) |
({e}{l}{s}{i}{f}) |
({e}{n}{d}) |
({e}{n}{t}{i}{t}{y}) |
({e}{x}{i}{t}) |
({f}{i}{l}{e}) |
({f}{o}{r}) |
({f}{u}{n}{c}{t}{i}{o}{n}) |
({g}{e}{n}{e}{r}{a}{t}{e}) |
({g}{e}{n}{e}{r}{i}{c}) |
({g}{r}{o}{u}{p}) |
({g}{u}{a}{r}{d}{e}{d}) |
({i}{f}) |
({i}{m}{p}{u}{r}{e}) |
({i}{n}) |
({i}{n}{e}{r}{t}{i}{a}{l}) |
({i}{n}{o}{u}{t}) |
({i}{s}) |
({l}{a}{b}{e}{l}) |
({l}{i}{b}{r}{a}{r}{y}) |
({l}{i}{n}{k}{a}{g}{e}) |
({l}{i}{t}{e}{r}{a}{l}) |
({l}{o}{o}{p}) |
({m}{a}{p}) |
({m}{o}{d}) |
({n}{a}{n}{d}) |
({n}{e}{w}) |
({n}{e}{x}{t}) |
({n}{o}{r}) |
({n}{o}{t}) |
({n}{u}{l}{l}) |
({o}{f}) |
({o}{n}) |
({o}{p}{e}{n}) |
({o}{r}) |
({o}{t}{h}{e}{r}{s}) |
({o}{u}{t}) |
({p}{a}{c}{k}{a}{g}{e}) |
({p}{o}{r}{t}) |
({p}{o}{s}{t}{p}{o}{n}{e}{d}) |
({p}{r}{o}{c}{e}{d}{u}{r}{e}) |
({p}{r}{o}{c}{e}{s}{s}) |
({p}{u}{r}{e}) |
({r}{a}{n}{g}{e}) |
({r}{e}{c}{o}{r}{d}) |
({r}{e}{g}{i}{s}{t}{e}{r}) |
({r}{e}{m}) |
({r}{e}{p}{o}{r}{t}) |
({r}{e}{t}{u}{r}{n}) |
({r}{o}{l}) |
({r}{o}{r}) |
({s}{e}{v}{e}{r}{i}{t}{y}) |
({s}{i}{g}{n}{a}{l}) |
({s}{h}{a}{r}{e}{d}) |
({s}{l}{a}) |
({s}{l}{l}) |
({s}{r}{a}) |
({s}{r}{l}) |
({s}{u}{b}{t}{y}{p}{e}) |
({t}{h}{e}{n}) |
({t}{o}) |
({t}{r}{a}{n}{s}{p}{o}{r}{t}) |
({t}{y}{p}{e}) |
({u}{n}{a}{f}{f}{e}{c}{t}{e}{d}) |
({u}{n}{i}{t}{s}) |
({u}{n}{t}{i}{l}) |
({u}{s}{e}) |
({v}{a}{r}{i}{a}{b}{l}{e}) |
({w}{a}{i}{t}) |
({w}{h}{i}{l}{e}) |
({x}{n}{o}{r}) |
({x}{o}{r}) |
({s}{t}{d}_{l}{o}{g}{i}{c}_{v}{e}{c}{t}{o}{r}) { 
    (*yyextra->os) << yytext;
}

"=>" { 
	(*yyextra->os) << yytext;
}

"<=" { 
	(*yyextra->os) << yytext;
	
	//check for possible errors in the VHDL code
	//	to be in this case, the ';' was probably omitted
	if(*yyextra->lexingMode == LexerContext::signalAssignment)
	{
		throw(string("something went wrong during the first parsing of the VHDL code!\nPlease check your VHDL code!"));
	}
	
	if(*yyextra->lexingMode == LexerContext::unset)
	{
		*yyextra->lexingMode = LexerContext::signalAssignment;
	}
	
	if(*yyextra->lexingMode == LexerContext::selectedSignalAssignment2)
	{
		*yyextra->lexingMode = LexerContext::selectedSignalAssignment3;
	}
}

":=" {
	(*yyextra->os) << yytext;
	
	yyextra->extraRhsNames->clear();
	*yyextra->lexingMode = LexerContext::variableAssignment;
	*yyextra->lhsName = "";
}


";" {
    //the left-hand side signal itself is marked, instead of putting it at the end of the statement
	for(int i=0; (unsigned)i<yyextra->extraRhsNames->size(); i++)
	{
		triplet<string, string, int> tempTriplet;

		tempTriplet.first = *yyextra->lhsName;
		tempTriplet.second = (*yyextra->extraRhsNames)[i];
		tempTriplet.third = 0;

		yyextra->dependenceTable->push_back(tempTriplet);
	}
	yyextra->extraRhsNames->clear();
	*yyextra->lexingMode = LexerContext::unset;
	*yyextra->lexingModeOld = LexerContext::unset;
	*yyextra->lhsName = "";
	
	*yyextra->isLhsSet = false;
	
	(*yyextra->os) << yytext;

}


({w}{h}{e}{n}) {
// When can appear in the following situations:
//   1/  with X select Y <=  toto when tata,
//                     where we arive in selectedSignalAssignment mode
//   2/  Y <=  toto when tata = '1' else    titi;
//  where we arrive in signalAssignment
	(*yyextra->os) << yytext;
	
	if(*yyextra->lexingMode != LexerContext::selectedSignalAssignment)
	{
		*yyextra->lexingMode = LexerContext::conditionalSignalAssignment;
	}
}


({w}{i}{t}{h}) {
	(*yyextra->os) << yytext;
	
	*yyextra->lexingMode = LexerContext::selectedSignalAssignment;
}


({s}{e}{l}{e}{c}{t}) {
	(*yyextra->os) << " select "; // we add two spaces and force lowercase to make the select keyword easier to identify from c++
	
	*yyextra->lexingMode = LexerContext::selectedSignalAssignment2;
}


({p}{o}{r}{t}({space_character})+{m}{a}{p}) {
	(*yyextra->os) << yytext;
	
	*yyextra->lexingMode = LexerContext::portmap;
	*yyextra->lhsName = "";
	yyextra->extraRhsNames->clear();
}

({g}{e}{n}{e}{r}{i}{c}({space_character})+{m}{a}{p}) {
	(*yyextra->os) << yytext;
	
	*yyextra->lexingMode = LexerContext::genericmap;
	*yyextra->lhsName = "";
	yyextra->extraRhsNames->clear();
}


({c}{a}{s}{e}) {
	(*yyextra->os) << yytext;
	
	*yyextra->lexingMode = LexerContext::caseStatement;
	*yyextra->lhsName = "";
	yyextra->extraRhsNames->clear();
}


{identifier} |
{delayed_identifier} |
{delayed_identifier2} {
	//identified either "signal_name"
  // REMOVED BY FLORENT, it was parsing as $$(signal)$$ and I don't see the point
  // or "(signal_name, signal_name...)" there was this rule
  // "("({space_character})*{identifier}(({space_character})*","({space_character})*{identifier})*({space_character})*")"
	//this can be either the left or the right hand side
	//if the mode is unset, then this is the new left hand side
	//	the mode itself is going to be set when encountering the "<=" symbol
	//if the mode is already set, then act accordingly
	//	if in signalAssignment mode, then this is a signal on the right hand side; store a new pair in the dependenceTable, mark the symbol for the second pass
	//	if in conditionalSignalAssignment mode, c.f. signalAssignment mode
	//	if in selectedSignalAssignment mode, then if the mode is unset this signal is stored and added later to the dependence table, else c.f. signalAssignment mode
	//	for all other cases, just copy the code to the output stream and don't perform any other action
	
  try {yyextra->op->getSignalByName(yytext);}
  catch(string &e){ 
     //cerr << "identifier '" << yytext << "' is not a declared signal" << endl;
			}
	if(*yyextra->lexingMode == LexerContext::unset)
	{
		// this is the left hand side of the assignment
		(*yyextra->os) << "??" << yytext << "??";
		
		if(*yyextra->lhsName == "")
		{
			*yyextra->lhsName = yytext;
		}
		
		if(*yyextra->isLhsSet == true)
		{
			throw(string("Error: something went wrong during the first parsing of the VHDL code!"));
		}
		*yyextra->isLhsSet = true;
	}else if(*yyextra->lexingMode == LexerContext::signalAssignment)
	{
		// this is the right hand side of the assignment
		(*yyextra->os) << "$$" << yytext << "$$";
		
		triplet<string, string, int> tempTriplet;

		tempTriplet.first = *yyextra->lhsName;
		tempTriplet.second = yytext;
		tempTriplet.third = 0;

		yyextra->dependenceTable->push_back(tempTriplet);
	}else if(*yyextra->lexingMode == LexerContext::conditionalSignalAssignment)
	{
		// this is the right hand side of the assignment
		(*yyextra->os) << "$$" << yytext << "$$";
		
		triplet<string, string, int> tempTriplet;

		tempTriplet.first = *yyextra->lhsName;
		tempTriplet.second = yytext;
		tempTriplet.third = 0;

		yyextra->dependenceTable->push_back(tempTriplet);
	}else if(*yyextra->lexingMode == LexerContext::selectedSignalAssignment)
	{
		// this signal is on the left side of the assignment, but belongs to the right side
		(*yyextra->os) << "$$" << yytext << "$$";
		
		yyextra->extraRhsNames->push_back(yytext);
		
		//switch the parsing mode to the next stage, selectedSignalAssignment2
		*yyextra->lexingMode = LexerContext::selectedSignalAssignment2;
		
		//save the left-hand side name for later to create the dependence
		yyextra->extraRhsNames->push_back(yytext);
	}else if(*yyextra->lexingMode == LexerContext::selectedSignalAssignment2)
	{
		// this signal is on the left side of the assignment
		(*yyextra->os) << "??" << yytext << "??";
		
		*yyextra->lhsName = yytext;
		
		if(*yyextra->isLhsSet == true)
		{
			throw(string("Error: something went wrong during the first parsing of the VHDL code!"));
			//throw("Error: something went wrong during the first parsing of the VHDL code!");
		}
		*yyextra->isLhsSet = true;
		
		//switch the parsing mode to the next stage, selectedSignalAssignment3
        *yyextra->lexingMode = LexerContext::selectedSignalAssignment3;
	}else if(*yyextra->lexingMode == LexerContext::selectedSignalAssignment3)
	{
		// this signal is on the right side of the assignment
		(*yyextra->os) << "$$" << yytext << "$$";
		
		triplet<string, string, int> tempTriplet;

		tempTriplet.first = *yyextra->lhsName;
		tempTriplet.second = yytext;
		tempTriplet.third = 0;

		yyextra->dependenceTable->push_back(tempTriplet);
	}else if(*yyextra->lexingMode == LexerContext::variableAssignment)
	{
		(*yyextra->os) << yytext;
	}else if(*yyextra->lexingMode == LexerContext::portmap)
	{
		(*yyextra->os) << "??" << yytext << "??";
		*yyextra->lexingMode = LexerContext::portmap2;
	}else if(*yyextra->lexingMode == LexerContext::portmap2)
	{
		(*yyextra->os) << "$$" << yytext << "$$";
		*yyextra->lexingMode = LexerContext::portmap;
	}else if(*yyextra->lexingMode == LexerContext::caseStatement)
	{
		(*yyextra->os) << yytext;
	}else if(*yyextra->lexingMode == LexerContext::process)
	{
		(*yyextra->os) << yytext;
	}else if(*yyextra->lexingMode == LexerContext::comment)
	{
		//this is a comment, so just output the input
		(*yyextra->os) << yytext;
	}else
	{
		(*yyextra->os) << yytext;
	}
}
									

{label} {
	(*yyextra->os) << yytext;
}
 

[ \t] {
	(*yyextra->os) << yytext;
}

[\n] {
	(*yyextra->os) << yytext;
	
	if(*yyextra->lexingMode == LexerContext::comment)
	{
		*yyextra->lexingMode = *yyextra->lexingModeOld;
	}
}

\&|\'|\(|\)|"**"|\*|\+|\-|\:|">="|\<|\>|=|\/=|"<>"|\||!|\.|\/ { 
	(*yyextra->os) << yytext;
}

(\,) {
	(*yyextra->os) << yytext;
	
	//if, while parsing a component instance, there is a constant signal connected to
	//	the port, then just reinitialize the state count
	if(*yyextra->lexingMode == LexerContext::portmap2)
	{
		*yyextra->lexingMode = LexerContext::portmap;
	}
}


({decimal_literal})|({base}#{based_integer}(\.{based_integer})?#({exponent})?)|({base}:{based_integer}(\.{based_integer})?:({exponent})?) {
	(*yyextra->os) << yytext; 
}

'({graphic_character}|\"|\%)' { 
	(*yyextra->os) << yytext;
}

(\"({graphic_character}|(\"\")|\%)*\")|(\%({graphic_character}|(\%\%)|\")*\%) {
	(*yyextra->os) << yytext;
}

{base_specifier}(\"{extended_digit}(_?{extended_digit})*\"|\%{extended_digit}(_?{extended_digit})*\%) { 
	(*yyextra->os) << yytext;
}

("--".*) {
	// this is a comment
	(*yyextra->os) << yytext;
	
	*yyextra->lexingModeOld = *yyextra->lexingMode;
	*yyextra->lexingMode = LexerContext::comment;
}

(.) {
	(*yyextra->os) << yytext;
}



%%

void LexerContext::init_scanner() {
	yylex_init(&scanner);
	yyset_extra(this, scanner);
}

void LexerContext::destroy_scanner() {
	yylex_destroy(scanner);
}

void LexerContext::lex() {
	yylex(scanner);
}
