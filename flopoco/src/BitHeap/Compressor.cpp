
#include "Compressor.hpp"
#include <string>

using namespace std;

namespace flopoco{

	BasicCompressor::BasicCompressor(Operator* parentOp_, Target * target_, vector<int> heights_, float area_, string type_, bool compactView_): parentOp(parentOp_), target(target_), heights(heights_), area(area_), type(type_), compactView(compactView_)
	{
		//compute the size of the input and of the output
		int wIn = 0;
		int maxVal = 0;
		for(int i=heights.size()-1; i>=0; i--)
		{
			wIn    += heights[i];
			maxVal += intpow2(i) * heights[i];
		}

		wOut = intlog2(maxVal);

		//setting up outHeights
		outHeights.resize(wOut, 1);
		compressor = nullptr;
	}
/*
	BasicCompressor::~BasicCompressor(){
		if(compressor != nullptr){
			delete compressor;
		}
	}
*/
	BasicCompressor::~BasicCompressor(){
	}

	Compressor* BasicCompressor::getCompressor(){
		if(type.compare("combinatorial") == 0){
			if(compressor != nullptr){
				return compressor;
			}
			else{
				compressor = new Compressor(parentOp, target, heights, area, compactView);
				return compressor;
			}
		}
		else{
			//TODO cases variable
			return nullptr;
		}
	}

	double BasicCompressor::getEfficiency(unsigned int middleLength){
		int inputBits = 0;
		int outputBits = 0;
		double ratio = 0.0;
		for(unsigned int j = 0; j < heights.size(); j++){
			inputBits += heights[j];
		}
		for(unsigned int j = 0; j < outHeights.size(); j++){
			outputBits += outHeights[j];
		}
		ratio = (double) (inputBits - outputBits);
		if(area != 0.0){
			ratio /= (double) area;
		}
		else{
			//area (and therefore cost) is zero. Therefore set ratio to zero.
			ratio = 0.0;
		}
		return ratio;
	}

	unsigned int BasicCompressor::getHeights(unsigned int middleLength){
		if(type.compare("variable") != 0){
			return heights.size();
		}
		else{
			//TODO
			return 0;
		}
	}

	unsigned int BasicCompressor::getOutHeights(unsigned int middleLength){
		if(type.compare("variable") != 0){
			return outHeights.size();
		}
		else{
			//TODO
			return 0;
		}
	}

	unsigned int BasicCompressor::getHeightsAtColumn(unsigned int column, bool ilpGeneration, unsigned int middleLength){
		if(type.compare("variable") != 0){
			if(column >= heights.size()){
				return 0;
			}
			else{
				if(!ilpGeneration){
					return heights[column];
				}
				else{	//ilp generation works on the reversed vector
					return heights[heights.size() - (column + 1)];
				}
			}
		}
		else{
			//TODO
			return 0;
		}
	}

	unsigned int BasicCompressor::getOutHeightsAtColumn(unsigned int column, bool ilpGeneration, unsigned int middleLength){
		if(type.compare("variable") != 0){
			if(column >= outHeights.size()){
				return 0;
			}
			else{
				if(!ilpGeneration){
					return outHeights[column];
				}
				else{	//ilp generation works on the reversed vector
					return outHeights[outHeights.size() - (column + 1)];
				}
			}
		}
		else{
			//TODO
			return 0;
		}
	}

	float BasicCompressor::getArea(unsigned int middleLength){
		if(type.compare("variable") != 0){
			return area;
		}
		else{
			//TODO
			return 0.0;
		}
	}


	// returns string of the inputs/outputs of the compressor. E.g. the fulladder would be
	// (3;2), a compressor with six inputs at column i, six outputs at column i+2 and 5 outputs
	// is represented as (6,0,6;5)
	string BasicCompressor::getStringOfIO(){
		if(type.compare("variable") != 0){
			ostringstream out;
			out.str("");
			out << "(";
			for(unsigned int c = heights.size(); c > 0; c--){
				out << heights[c - 1];
				if(c - 1 != 0){
					out << ",";
				}
			}
			out << ";";
			unsigned int outputBits = 0;
			for(unsigned int c = 0; c < outHeights.size(); c++){
				outputBits += outHeights[c];
			}
			out << outputBits << ")";
			return out.str();
		}
		else{
			//TODO
			return "variable_compressor";
		}
	}

	Compressor::Compressor(Operator* parentOp, Target * target) : Operator(parentOp, target)
	{

	}

	Compressor::Compressor(Operator* parentOp, Target * target_, vector<int> heights_, float area_, bool compactView_)
		: Operator(parentOp, target_), // for now, no parent
			heights(heights_), area(area_), compactView(compactView_), compressorUsed(false)
	{
		ostringstream name;

		//compressors are supposed to be combinatorial
		setCombinatorial();
		setShared();

		//remove the zero columns at the lsb
		while(heights[0] == 0)
		{
			heights.erase(heights.begin());
		}

		setWordSizes();
		createInputsAndOutputs();

		//set name:
		name << "Compressor_";
		for(int i=heights.size()-1; i>=0; i--)
			name << heights[i];

		name << "_" << wOut;
		setNameWithFreqAndUID(name.str());

		stringstream xs;
		for(int i=heights.size()-1; i>=0; i--)
		{
			//no need to create a signal for columns of height 0
			if(heights[i] == 0)
				continue;

			xs << "X" << i << " ";
			if(i != 0)
				xs << "& ";
		}
		//getSignalByName("R")->setCriticalPathContribution(getTarget()->logicDelay(wIn));
		double cpDelay;

		if(UserInterface::pipelineActive_) {
			cpDelay = getTarget()->tableDelay(wIn, wOut, true);
		} else {
			cpDelay = 0; //set delay to zero to prevent from being pipelined
		}

				vhdl << tab << declare("X", wIn) << " <= " << xs.str() << ";" << endl << endl;
		vhdl << tab << "with X select " << declare(cpDelay, "R0", wOut) << " <= " << endl;

		vector<vector<mpz_class>> values(1<<wOut);
		//create the compressor
		for(mpz_class i=0; i<(1 << wIn); i++)
		{
			mpz_class ppcnt = 0;
			mpz_class ii = i;

			//compute the compression result for the current input
			for(unsigned j=0; j<heights.size(); j++)
			{
				ppcnt += popcnt( ii - ((ii>>heights[j]) << heights[j]) ) << j;
				ii = ii >> heights[j];
			}

			//output the line, if not in compact mode
			if(!compactView)
			{
				vhdl << tab << tab << "\"" << unsignedBinary(ppcnt, wOut) << "\" when \""
						<< unsignedBinary(i, wIn) << "\", \n";
			}else{
				values[ppcnt.get_ui()].push_back(i);
			}
		}

		//print the compressor, if in compact mode
		if(compactView)
		{
			for(unsigned i=0; i<values.size(); i++)
			{
				if(values[i].size() > 0)
				{
					vhdl << tab << tab << "\"" << unsignedBinary(mpz_class(i), wOut) << "\" when \""
							<< unsignedBinary(mpz_class(values[i][0]), wIn) << "\"";
					for(unsigned j=1; j<values[i].size(); j++)
					{
						vhdl << " | \"" << unsignedBinary(mpz_class(values[i][j]), wIn) << "\"";
					}
					vhdl << "," << endl;
				}
			}
		}

		vhdl << tab << tab << "\"" << std::string(wOut, '-') << "\" when others;" << endl;

		vhdl << tab << "R <= R0;" << endl;
		getSignalByName("R") -> setCriticalPath(cpDelay);
		REPORT(DEBUG, "Generated " << name.str());
	}

	void Compressor::setWordSizes()
	{
		//compute the size of the input and of the output
		wIn = 0;
		maxVal = 0;
		for(int i=heights.size()-1; i>=0; i--)
		{
			wIn    += heights[i];
			maxVal += intpow2(i) * heights[i];
		}

		wOut = intlog2(maxVal);

		//setting up outHeights
		outHeights.resize(wOut, 1);
	}

	void Compressor::createInputsAndOutputs()
	{
		//create the inputs
		//	and the internal signal which concatenates all the inputs
		for(int i=heights.size()-1; i>=0; i--)
		{
			//no need to create a signal for columns of height 0
			if(heights[i] > 0)
			{
				addInput(join("X",i), heights[i]);
			}
		}
		//create the output
		addOutput("R", wOut);
	}

	Compressor::~Compressor(){
	}


	unsigned Compressor::getColumnSize(int column)
	{
		if(column >= (signed)heights.size())
			return 0;
		else
			return heights[column];
	}

	int Compressor::getOutputSize()
	{
		return wOut;
	}

	bool Compressor::markUsed()
	{
		compressorUsed = true;
		return compressorUsed;
	}

	bool Compressor::markUnused()
	{
		compressorUsed = false;
		return compressorUsed;
	}


	void Compressor::emulate(TestCase * tc)
	{
		mpz_class r = 0;

		for(unsigned i=0; i<heights.size(); i++)
		{
			mpz_class sx = tc->getInputValue(join("X", i));
			mpz_class p  = popcnt(sx);

			r += p<<i;
		}

		tc->addExpectedOutput("R", r);
	}


	OperatorPtr Compressor::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		string in;
		bool compactView_;
		vector<int> heights_;

		UserInterface::parseString(args, "columnHeights", &in);
		UserInterface::parseBoolean(args, "compactView", &compactView_);

		// tokenize the string, with ',' as a separator
		stringstream ss(in);
		while(ss.good())
		{
			string substr;

			getline(ss, substr, ',');
			heights_.insert(heights_.begin(), stoi(substr));
		}

		return new Compressor(parentOp, target, heights_, compactView_);
	}

	void Compressor::registerFactory(){
		UserInterface::add("Compressor", // name
				"A basic compressor.",
				"BasicInteger", // categories
				"",
				"columnHeights(string): comma separated list of heights for the columns of the compressor, \
in decreasing order of the weight. For example, columnHeights=\"2,3\" produces a (2,3:4) GPC; \
				compactView(bool)=false: whether the VHDL code is printed in a more compact way, or not",
				"",
				Compressor::parseArguments
		) ;
	}
}
