/*
  A generic class for tables of values

  Author : Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.

 */


#include <iostream>
#include <sstream>
#include <cstdlib>
#include "utils.hpp"
#include "Table.hpp"

using namespace std;


namespace flopoco{

#if 0
	mpz_class Table::call_function(int x)
	{
		return function(x);
	}
#endif

	

	Table::Table(OperatorPtr parentOp_, Target* target_, vector<mpz_class> _values, string _name, int _wIn, int _wOut, int _logicTable, int _minIn, int _maxIn) :
		Operator(parentOp_, target_)
	{
		srcFileName = "Table";
		setNameWithFreqAndUID(_name);
		setCopyrightString("Florent de Dinechin, Bogdan Pasca (2007-2018)");
		init(_values, _name, _wIn, _wOut,  _logicTable,  _minIn,  _maxIn); 
	}


	void	Table::init(vector<mpz_class> _values, string _name,
										int _wIn, int _wOut, int _logicTable, int _minIn, int _maxIn)
	{

		values     = _values;
		wIn        = _wIn;
		wOut       = _wOut;
		minIn      = _minIn;
		maxIn      = _maxIn;
		//sanity checks: can't fill the table if there are no values to fill it with
		if(values.size() == 0)
			THROWERROR("Error in Table::init(): the set of values to be written in the table is empty" << endl);

		//set wIn
		if(wIn < 0){
			//set the value of wIn
			wIn = intlog2(values.size());
			REPORT(DEBUG, "WARNING: wIn value was not set, wIn=" << wIn << " was inferred from the vector of values");
		}
		else if(((unsigned)1<<wIn) < values.size()) {
			REPORT(DEBUG, "WARNING: wIn set to a value lower than the number of values which are to be written in the table.");
			//set the value of wIn
			wIn = intlog2(values.size());
		}

#if 0  // debug. Who never needs to print out the content of his Table ?
		for(unsigned int i=0; i<values.size(); i++)		{
			REPORT(0,"value["<<i<<"] = " << values[i]);
	}
#endif
		
		//determine the lowest and highest values stored in the table
		mpz_class maxValue = values[0], minValue = values[0];
		
		//this assumes that the values stored in the values array are all positive
		for(unsigned int i=0; i<values.size(); i++)		{
			if(values[i] < 0)
				THROWERROR("Error in table: value stored in table is negative: " << values[i] << endl);
			if(maxValue < values[i])
				maxValue = values[i];
			if(minValue > values[i])
				minValue = values[i];
		}

		//set wOut
		if(wOut < 0){
			//set the value of wOut
			wOut = intlog2(maxValue);
			REPORT(INFO, "WARNING: wOut value was not set, wOut=" << wOut << " was inferred from the vector of values");
		}
		else if(wOut < intlog2(maxValue))  {
#if 0 // This behaviour is probably a timebomb. Turns out it was fixing FixFunctionByMultipartiteTable, intconstdiv/BTCD, and probably others
			REPORT(INFO, "WARNING: wOut value was set too small. I'm fixing it up but, but I probably shouldn't");
			//set the value of wOut
			wOut = intlog2(maxValue);
#else
			THROWERROR("In Table::init(), some values require " << intlog2(maxValue) << " bits to be stored, but wOut was set to "<< wOut);
#endif
		}

		// if this is just a Table
		if(_name == "")
			setNameWithFreqAndUID(srcFileName+"_"+vhdlize(wIn)+"_"+vhdlize(wOut));

		//checks for logic table
		if(_logicTable == -1)
			logicTable = false;
		else if(_logicTable == 1)
			logicTable = true;
		else
			logicTable = (wIn <= getTarget()->lutInputs())  ||  (wOut * (mpz_class(1) << wIn) < getTarget()->sizeOfMemoryBlock()/2 );
		REPORT(DEBUG, "_logicTable=" << _logicTable << "  logicTable=" << logicTable);

		// Sanity check: the table is built using a RAM, but is underutilized
		if(!logicTable
				&& ((wIn <= getTarget()->lutInputs())  ||  (wOut*(mpz_class(1) << wIn) < 0.5*getTarget()->sizeOfMemoryBlock())))
			REPORT(0, "Warning: the table is built using a RAM block, but is underutilized");

		// Logic tables are shared by default, large tables are unique because they can have a multi-cycle schedule.
		if(logicTable)
			setShared();

		// Set up the IO signals -- this must come after the setShared()
		addInput("X", wIn, true);
		addOutput("Y", wOut, 1, true);

		//set minIn
		if(minIn < 0){
			REPORT(DEBUG, "WARNING: minIn not set, or set to an invalid value; will use the value determined from the values to be written in the table.");
			minIn = 0;
		}
		//set maxIn
		if(maxIn < 0){
			REPORT(DEBUG, "WARNING: maxIn not set, or set to an invalid value; will use the value determined from the values to be written in the table.");
			maxIn = values.size()-1;
		}

		if((1<<wIn) < maxIn){
			cerr << "ERROR: in Table constructor: maxIn too large\n";
			exit(EXIT_FAILURE);
		}

		//determine if this is a full table
		if((minIn==0) && (maxIn==(1<<wIn)-1))
			full=true;
		else
			full=false;

		//user warnings
		if(wIn > 12)
			REPORT(FULL, "WARNING: FloPoCo is building a table with " << wIn << " input bits, it will be large.");


		//create the code for the table
		REPORT(DEBUG,"Table.cpp: Filling the table");

		
		if(logicTable){
			int lutsPerBit;
			if(wIn < getTarget()->lutInputs())
				lutsPerBit = 1;
			else
				lutsPerBit = 1 << (wIn-getTarget()->lutInputs());
			REPORT(DETAILED, "Building a logic table that uses " << lutsPerBit << " LUTs per output bit");
		}

		cpDelay = getTarget()->tableDelay(wIn, wOut, logicTable);
		vhdl << tab << "with X select " << declare(cpDelay, "Y0", wOut) << " <= " << endl;;
		
		for(unsigned int i=minIn.get_ui(); i<=maxIn.get_ui(); i++)
			vhdl << tab << tab << "\"" << unsignedBinary(values[i-minIn.get_ui()], wOut) << "\" when \"" << unsignedBinary(i, wIn) << "\"," << endl;
		vhdl << tab << tab << "\"";
		for(int i=0; i<wOut; i++)
			vhdl << "-";
		vhdl <<  "\" when others;" << endl;

		// TODO there seems to be several possibilities to make a BRAM; the following seems ineffective

		std::string tableAttributes;
		//set the table attributes
		if(getTarget()->getID() == "Virtex6")
			tableAttributes =  "attribute ram_extract: string;\nattribute ram_style: string;\nattribute ram_extract of Y0: signal is \"yes\";\nattribute ram_style of Y0: signal is ";
		else if(getTarget()->getID() == "Virtex5")
			tableAttributes =  "attribute rom_extract: string;\nattribute rom_style: string;\nattribute rom_extract of Y0: signal is \"yes\";\nattribute rom_style of Y0: signal is ";
		else
			tableAttributes =  "attribute ram_extract: string;\nattribute ram_style: string;\nattribute ram_extract of Y0: signal is \"yes\";\nattribute ram_style of Y0: signal is ";

		if((logicTable == 1) || (wIn <= getTarget()->lutInputs())){
			//logic
			if(getTarget()->getID() == "Virtex6")
				tableAttributes += "\"pipe_distributed\";";
			else
				tableAttributes += "\"distributed\";";
		}else{
			//block RAM
			tableAttributes += "\"block\";";
		}
		getSignalByName("Y0") -> setTableAttributes(tableAttributes);

		vhdl << tab << "Y <= Y0;" << endl;
	}

	
	Table::Table(OperatorPtr parentOp, Target* target) :
		Operator(parentOp, target){
		setCopyrightString("Florent de Dinechin, Bogdan Pasca (2007, 2018)");
	}

	mpz_class Table::val(int x){
		if(x<minIn || x>maxIn) {
			THROWERROR("Error in table: input index " << x
								 << " out of range ["<< minIn << " " << maxIn << "]" << endl);	
		}
		return values[x];
	}


	int Table::size_in_LUTs() {
		return wOut*int(intpow2(wIn-getTarget()->lutInputs()));
	}


	OperatorPtr Table::newUniqueInstance(OperatorPtr op,
																			 string actualInput, string actualOutput,
																			 vector<mpz_class> values, string name,
																			 int wIn, int wOut){
		op->schedule();
		op->inPortMap("X", actualInput);
		op->outPortMap("Y", actualOutput);
		Table* t = new Table(op, op->getTarget(), values, name, wIn, wOut); 
		op->vhdl << op->instance(t, name, false);
		return t;
	}
	

	
}
