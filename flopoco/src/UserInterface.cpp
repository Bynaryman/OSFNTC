#include "UserInterface.hpp"
#include "Targets/AllTargetsHeaders.hpp"
#include "TestBenches/TestBench.hpp"

#include "AutoTest/AutoTest.hpp"

#include <algorithm>
#include <sys/stat.h>
#include <iostream>
#include <iomanip>

// TODO check the hard mult threshold

namespace flopoco
{
	// Colors from	https://github.com/Uduse/Escape-Sequence-Color-Header/blob/master/src/Escape_Sequences_Colors.h
	const char COLOR_NORMAL[] = { 0x1b, '[', '0', ';', '3', '9', 'm', 0 };
	const char COLOR_BOLD_BLUE_NORMAL[] = { 0x1b, '[', '1', ';', '3', '4', ';', '4', '9', 'm', 0 };
	const char COLOR_BOLD[] = { 0x1b, '[', '1', 'm', 0 };
	const char COLOR_RED_NORMAL[] = { 0x1b, '[', '3', '1', ';', '4', '9', 'm', 0 };
	const char COLOR_BLUE_NORMAL[] = { 0x1b, '[', '3', '4', ';', '4', '9', 'm', 0 };
	const char COLOR_BOLD_RED_NORMAL[] = { 0x1b, '[', '1', ';', '3', '1', ';', '4', '9', 'm', 0 };
	const char COLOR_REVERSEVIDEO_BLACK_GREEN[] = { 0x1b, '[', '7', ';', '3', '0', ';', '4', '2', 'm', 0 };
	const char COLOR_BOLD_MAGENTA_NORMAL[] = { 0x1b, '[', '1', ';', '3', '5', ';', '4', '9', 'm', 0 };

	//	const char* defaultFPGA="Zynq7000";
	const char* defaultFPGA="kintex7";

	// Allocation of the global objects
	string UserInterface::outputFileName;
	string UserInterface::entityName=""; // used for the -name option
	int    UserInterface::verbose;
	string UserInterface::targetFPGA;
	double UserInterface::targetFrequencyMHz;
	bool   UserInterface::clockEnable;
	bool   UserInterface::useHardMult;
	bool   UserInterface::plainVHDL;
	bool   UserInterface::generateFigures;
	double UserInterface::unusedHardMultThreshold;
	bool   UserInterface::useTargetOptimizations;
	string   UserInterface::compression;
	string   UserInterface::tiling;
	string UserInterface::ilpSolver;
	int    UserInterface::ilpTimeout;
	int    UserInterface::resourceEstimation;
	bool   UserInterface::floorplanning;
	bool   UserInterface::reDebug;
	bool   UserInterface::flpDebug;

	string UserInterface::depGraphDrawing="";


	const vector<pair<string,string>> UserInterface::categories = []()->vector<pair<string,string>>{
		vector<pair<string,string>> v;
		v.push_back(make_pair("ShiftersLZOCs", "Shifters, Leading Zero Counters, etc"));
		v.push_back(make_pair("BasicInteger", "Basic integer operators (pipelined)"));
		//		v.push_back(make_pair("BasicFixPoint", "Basic fixed-point Operators"));
		v.push_back(make_pair("BasicFloatingPoint", "Basic floating-point operators"));
		v.push_back(make_pair("Posit", "Posit operators"));
		v.push_back(make_pair("ConstMultDiv", "Multipliers and dividers by constants"));
		//		v.push_back(make_pair("CompositeFloatingPoint", "Composite floating-point operators"));
		//		v.push_back(make_pair("CompositeFixPoint", "Composite fixed-point operators"));
		v.push_back(make_pair("ElementaryFunctions", "Elementary functions in fixed- or floating-Point"));
		v.push_back(make_pair("FunctionApproximation", "Arbitrary function approximators"));
		//		v.push_back(make_pair("ComplexFixPoint", "Complex arithmetic in fixed point" ));
		//		v.push_back(make_pair("ComplexFloatingPoint", "Complex arithmetic in floating point" ));
		//		v.push_back(make_pair("LNS", "Logarithm Number System" ));
		v.push_back(make_pair("Conversions", "Conversions between number systems" ));
		v.push_back(make_pair("FiltersEtc", "Filters and FFTs"));
		v.push_back(make_pair("TestBenches", "Test Benches"));
		v.push_back(make_pair("AutoTest", "AutoTest"));
        v.push_back(make_pair("Primitives", "Highly target optimized primitive operators"));
        v.push_back(make_pair("Miscellaneous", "Miscellaneous"));
        v.push_back(make_pair("SA", "Systolic Arrays"));
        return v;
	}();

	const vector<string> UserInterface::known_fpgas = []()->vector<string>{
				vector<string> v;
				v.push_back("zynq7000");
				v.push_back("kintex7");
				v.push_back("virtexultrascaleplus");
				v.push_back("virtex4");
				v.push_back("virtex5");
				v.push_back("virtex6");
				v.push_back("spartan3");
				v.push_back("stratix2");
				v.push_back("stratix3");
				v.push_back("stratix4");
				v.push_back("stratix5");
				v.push_back("cyclone2");
				v.push_back("cyclone3");
				v.push_back("cyclone4");
				v.push_back("cyclone5");
				return v;
			}();

	const vector<string> UserInterface::special_targets= []()->vector<string>{
				vector<string> v;
				v.push_back("BuildHTMLDoc");
				v.push_back("BuildAutocomplete");
				return v;
			}();

	// TODO unless I'm mistaken, this vector is never used. It should.

	const vector<option_t> UserInterface::options = []()->vector<option_t>{
				vector<option_t> v;
				vector<string> values;

				// Boolean options
				values.clear();
				values.push_back(std::to_string(0));
				values.push_back(std::to_string(1));
				v.push_back(option_t("clockEnable", values));
				v.push_back(option_t("plainVHDL", values));
				v.push_back(option_t("generateFigures", values));
				v.push_back(option_t("useHardMults", values));
				v.push_back(option_t("useTargetOptimizations", values));
				v.push_back(option_t("ilpSolver", values));
				v.push_back(option_t("ilpTimeout", values));
				v.push_back(option_t("compression", values));
				v.push_back(option_t("tiling", values));

				//free options, using an empty vector of values
				values.clear();
				v.push_back(option_t("name", values));
				v.push_back(option_t("outputFile", values));
				v.push_back(option_t("hardMultThreshold", values));
				v.push_back(option_t("frequency", values));

				//verbosity level
				values.clear();
				for(unsigned int i = 0 ; i < 3 ; ++i) {
					values.push_back(std::to_string(i));
				}
				v.push_back(option_t("verbose", values));

				//target option
				v.push_back(option_t("target", known_fpgas));

				//dot drawing options
				values.clear();
				values.push_back("no");
				values.push_back("full");
				values.push_back("compact");
				v.push_back(option_t("dependencyGraph", values));

				return v;
			}();




	void UserInterface::main(int argc, char* argv[]) {
		try {
			sollya_lib_init();
			initialize();

			// TODO refactor more elegantly

			// This creates all the Operators and the dependency graph.
			buildAll(argc, argv);

			if(depGraphDrawing != "no")
			{
				mkdir("dot", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
				drawDotDiagram(UserInterface::globalOpList);
			}

			outputVHDL();
			finalReport(cerr);
			sollya_lib_close();
		}
		catch (string e) {
			cerr << endl << e;
		}
	}






	void UserInterface::parseGenericOptions(vector<string> &args) {
		parseString(args, "name", &entityName, true); // not sticky: will be used, and reset, after the operator parser
		parsePositiveInt(args, "verbose", &verbose, true); // sticky option
		parseString(args, "outputFile", &outputFileName, true); // not sticky: will be used, and reset, after the operator parser
		parseString(args, "target", &targetFPGA, true); // not sticky: will be used, and reset, after the operator parser
		parseFloat(args, "frequency", &targetFrequencyMHz, true); // sticky option
		parseBoolean(args, "plainVHDL", &plainVHDL, true);
		parseBoolean(args, "clockEnable", &clockEnable, true);
		parseFloat(args, "hardMultThreshold", &unusedHardMultThreshold, true); // sticky option
		parseBoolean(args, "useHardMult", &useHardMult, true);
		parseBoolean(args, "generateFigures", &generateFigures, true);
		parseBoolean(args, "useTargetOptimizations", &useTargetOptimizations, true);
		parseString(args, "ilpSolver", &ilpSolver, true); // sticky option
		parsePositiveInt(args, "ilpTimeout", &ilpTimeout, true); // sticky option
		parseString(args, "compression", &compression, true);
		parseString(args, "tiling", &tiling, true);
		parseBoolean(args, "floorplanning", &floorplanning, true);
		//		parseBoolean(args, "reDebug", &reDebug, true );
		parseString(args, "dependencyGraph", &depGraphDrawing, true);
		//	parseBoolean(args, "", &  );
	}



	// Global objects: factory list
	vector<pair<string,OperatorFactoryPtr>> UserInterface::factoryList;

	vector<OperatorPtr>  UserInterface::globalOpList;  /**< Level-0 operators. Each of these can have sub-operators */

	vector<vector<OperatorPtr>>  UserInterface::globalOpListStack;

	int UserInterface::pipelineActive_;



	void UserInterface::pushAndClearGlobalOpList() {
		globalOpListStack.push_back(globalOpList);
		globalOpList.clear();
	}
	void UserInterface::popGlobalOpList() {
		globalOpList = globalOpListStack.back();
		globalOpListStack.pop_back();
	}


	OperatorPtr UserInterface::addToGlobalOpList(OperatorPtr op) {
		OperatorPtr alreadyPresent=nullptr;
		for (auto i: UserInterface::globalOpList){
			if( op->getName() == i->getName() ) {
				alreadyPresent=i;
				// REPORT(DEBUG,"Operator::addToGlobalOpList(): " << op->getName() <<" already present in globalOpList");
			}
		}
		if(alreadyPresent) {
			return alreadyPresent;
		}
		else {
			UserInterface::globalOpList.push_back(op);
			// If this component was created as a sub-component, we need to set its parentOp to null
			op->setParentOperator(nullptr);
			// make sure it is scheduled
			op->schedule();
			op->applySchedule();
			return op;
		}
	}


	void UserInterface::outputVHDLToFile(ofstream& file){
		set<string> alreadyOutput; // to avoid redundant output
		outputVHDLToFile(UserInterface::globalOpList, file, alreadyOutput);
	}


	/* The recursive method */
	void UserInterface::outputVHDLToFile(vector<OperatorPtr> &oplist, ofstream& file, set<string> &alreadyOutput )
	{

		for(auto i: oplist) {
			try		{
				// check for subcomponents
				if(! i->getSubComponentListR().empty() ){
					//recursively call to print subcomponents
					outputVHDLToFile(i->getSubComponentListR(), file, alreadyOutput);
				}

				//output the vhdl code to file if it was not done already
				if(alreadyOutput.find(i->getName())==alreadyOutput.end()) {
					i->outputVHDL(file);
					alreadyOutput.insert(i->getName());
				}
			}
			catch (std::string &s)	{
				cerr << "Exception while generating '" << i->getName() << "': " << s << endl;
			}
		}
	}


	void UserInterface::finalReport(ostream& s){
		s << endl<<"*** Final report ***"<<endl;
		s << "Output file: " << outputFileName <<endl;
		Operator* op = UserInterface::globalOpList.back();
		s << "Target: " << op->getTarget() -> getID()
			<< " @ "<< op->getTarget() -> frequencyMHz() << " MHz" <<	endl;
		for(auto i: UserInterface::globalOpList) {
			i->outputFinalReport(s, 0);
		}

		// Messages for testbenches. Only works if you have only one TestBench
		if(op->getSrcFileName() == "TestBench"){
			s << "To run the simulation using ModelSim, type the following in 'vsim -c':" <<endl;
			s << tab << "vdel -all -lib work" <<endl;
			s << tab << "vlib work" <<endl;
			s << tab << "vcom " << outputFileName <<endl;
			s << tab << "vsim " << op->getName() <<endl;
			s << tab << "add wave -r *" <<endl;
			s << tab << "run " << ((TestBench*)op)->getSimulationTime() << "ns" << endl;

			s << "To run the simulation using xsim(xilinx vivado soft), type the following in a shell prompt:" <<endl;
			s << tab << "xvhdl " << outputFileName << endl;
			s << tab << "xelab " << op->getName() << " -debug typical" << endl;
			s << tab << "xsim [-gui] work." << op->getName()  << endl;
			s << tab << "add_wave -r *" << endl;
			s << tab << "run " << ((TestBench*)op)->getSimulationTime() << "ns" << endl;

			s << "To run the simulation using gHDL, type the following in a shell prompt:" <<endl;
			string simlibs="--ieee=standard --ieee=synopsys ";
			s <<  "ghdl -a " << simlibs << "-fexplicit "<< outputFileName <<endl;
			s <<  "ghdl -e " << simlibs << "-fexplicit " << op->getName() <<endl;
			s <<  "ghdl -r " << simlibs << op->getName() << " --vcd=" << op->getName() << ".vcd --stop-time=" << ((TestBench*)op)->getSimulationTime() << "ns" <<endl;
			s <<  "gtkwave " << op->getName() << ".vcd" << endl;
			cerr << "To run the simulation using nvc, type the following in a shell prompt:" <<endl;
			cerr <<  "nvc  -a " << outputFileName << " --relax=prefer-explicit  -e " <<  op->getName() << "  -r --exit-severity=failure " << "--wave=" << op->getName() << ".fst --stop-time=" << ((TestBench*)op)->getSimulationTime() << "ns" <<endl;
			cerr <<  "gtkwave " << op->getName() << ".fst" << endl;
		}
	}


	void UserInterface::registerFactory(OperatorFactoryPtr factory)	{
		//		if(factoryList.find(factory->name())!=factoryList.end())
		//			throw string("OperatorFactory - Factory with name '"+factory->name()+" has already been registered.");
		factoryList.push_back(make_pair(factory->name(), factory));
	}

	unsigned UserInterface::getFactoryCount() {
		return factoryList.size();
	}

	OperatorFactoryPtr UserInterface::getFactoryByIndex(unsigned i)
	{
		return UserInterface::factoryList[i].second;
	}

	OperatorFactoryPtr UserInterface::getFactoryByName(string operatorName)	{
		std::transform(operatorName.begin(), operatorName.end(), operatorName.begin(), ::tolower);
		for(auto it: UserInterface::factoryList) {
			string lowerCaseFactoryName = it.first;
			std::transform(lowerCaseFactoryName.begin(), lowerCaseFactoryName.end(), lowerCaseFactoryName.begin(), ::tolower);
			if (lowerCaseFactoryName == operatorName)
				return  it.second;
		}
		throw ("No operator factory for " + operatorName);
	}


	void UserInterface::initialize(){
        registerFactories();  //implemented in Factories.cpp
		// Initialize all the command-line options
		verbose=1;
		outputFileName="flopoco.vhdl";
		targetFPGA=defaultFPGA;
		targetFrequencyMHz=400;
		useHardMult=true;
		unusedHardMultThreshold=0.7;
		compression = "heuristicMaxEff";
		tiling = "heuristicBasicTiling";

		ilpSolver = "Gurobi";
		ilpTimeout = 0; //timeout disabled

		depGraphDrawing = "full";
		pipelineActive_ = true;

	}

	void UserInterface::buildAll(int argc, char* argv[]) {

		// manage trivial cases
		if(argc==1) {
			cerr << getFullDoc();
			exit(EXIT_SUCCESS);
		}
		if(argc==2 && string(argv[1])=="BuildHTMLDoc") {
			buildHTMLDoc();
			exit(EXIT_SUCCESS);
		}
		if(argc==2 && string(argv[1])=="BuildAutocomplete") {
			buildAutocomplete();
			exit(EXIT_SUCCESS);
		}

		// First convert for convenience the input arg list into
		// 1/ a (possibly empty) vector of global args / initial options,
		// 2/ a vector of operator specification, each being itself a vector of strings
		vector<string> initialOptions;
		vector<vector<string>> operatorSpecs;

		vector<string> args;
		// convert all the char* to strings
		for (int i=1; i<argc; i++) // start with 1 to skip executable name
			args.push_back(string(argv[i]));

		// Build the global option list
		initialOptions.push_back("$$initialOptions$$");
		while(args.size() > 0 // there remains something to parse
					&& args[0].find("=") !=string::npos) {// and it is an option
			initialOptions.push_back(args[0]);
			args.erase(args.begin());
		}
		// Now there should be at least one operator specification
		while(args.size() > 0) { // This loop is over the Operators that are passed on the command line
			vector<string> opSpec;
			opSpec.push_back(string(args[0]));  // operator Name
			args.erase(args.begin());
			while(args.size() > 0 // there remains something to parse
						&& args[0].find("=") !=string::npos) {// and it is an option
				opSpec.push_back(args[0]);
				args.erase(args.begin());
			}
			operatorSpecs.push_back(opSpec);
		}


		// Now we have organized our input: do the parsing itself. All the sub-parsers erase the data they consume from the string vectors
		try {
			parseGenericOptions(initialOptions);
			initialOptions.erase(initialOptions.begin());
			if(initialOptions.size()>0){
				ostringstream s;
				s << "Don't know what to do with the following global option(s) :" <<endl ;
				for (auto i : initialOptions)
					s << "  "<<i<<" ";
				s << endl;
				throw s.str();
			}

			for (auto opParams: operatorSpecs) {
				string opName = opParams[0];  // operator Name
				// remove the generic options
				parseGenericOptions(opParams);

				// build the Target for this operator
				Target* target;
				// make this option case-insensitive, too
				std::transform(targetFPGA.begin(), targetFPGA.end(), targetFPGA.begin(), ::tolower);

					// This could also be a factory but it is less critical
				if (targetFPGA=="zynq7000")  target=new Zynq7000();
				//					else if(targetFPGA=="virtex4") target=new Virtex4();
				//				else if (targetFPGA=="virtex5") target=new Virtex5();
				else if (targetFPGA=="kintex7") target=new Kintex7();
				else if (targetFPGA=="virtexultrascaleplus") target=new VirtexUltrascalePlus();
				else if (targetFPGA=="virtex6") target=new Virtex6();
				//					else if (targetFPGA=="spartan3") target=new Spartan3();
				//					else if (targetFPGA=="stratixii" || targetFPGA=="stratix2") target=new StratixII();
				//					else if (targetFPGA=="stratixiii" || targetFPGA=="stratix3") target=new StratixIII();
				//				else if (targetFPGA=="stratixiv" || targetFPGA=="stratix4") target=new StratixIV();
				else if (targetFPGA=="stratixv" || targetFPGA=="stratix5") target=new StratixV();
				//					else if (targetFPGA=="cycloneii" || targetFPGA=="cyclone2") target=new CycloneII();
				//					else if (targetFPGA=="cycloneiii" || targetFPGA=="cyclone3") target=new CycloneIII();
				//					else if (targetFPGA=="cycloneiv" || targetFPGA=="cyclone4") target=new CycloneIV();
				//				else if (targetFPGA=="cyclonev" || targetFPGA=="cyclone5") target=new CycloneV();
				else {
					throw("ERROR: unknown target: " + targetFPGA);
				}
				target->setClockEnable(clockEnable);
				target->setFrequency(1e6*targetFrequencyMHz);
				target->setUseHardMultipliers(useHardMult);
				target->setUnusedHardMultThreshold(unusedHardMultThreshold);
				target->setPlainVHDL(plainVHDL);
				target->setGenerateFigures(generateFigures);
				target->setUseTargetOptimizations(useTargetOptimizations);
				target->setCompressionMethod(compression);
				target->setILPSolver(ilpSolver);
				target->setILPTimeout(ilpTimeout);
				target->setTilingMethod(tiling);

				// Now build the operator
				OperatorFactoryPtr fp = getFactoryByName(opName);
				if (fp==NULL){
					throw( "Can't find the operator factory for " + opName) ;
				}
				// Call the constructor at last (through the factory)
				OperatorPtr op = fp->parseArguments(nullptr, target, opParams);
				if(op!=NULL)	{// Some factories don't actually create an operator
					if(entityName!="") {
						op->changeName(entityName);
						entityName="";
					}
					UserInterface::globalOpList.push_back(op);
					// Schedule it
					op->schedule();
					op->applySchedule();
				}
			}
		}catch(std::string &s){
			std::cerr<<"Error : "<<s<<"\n";
			//factory->Usage(std::cerr);
			exit(EXIT_FAILURE);
		}catch(std::exception &s){
			std::cerr<<"Exception : "<<s.what()<<"\n";
			//factory->Usage(std::cerr);
			exit(EXIT_FAILURE);
		}
	}

	void UserInterface::drawDotDiagram(vector<OperatorPtr> & oplist) {
		ofstream file;
		for(auto i: oplist) {
			try {
				if (i->isOperatorDrawn()) {
					continue;
				}
				file.open(string{"dot/"}+(i->getName()+".dot").c_str(), ios::out);
				i->drawDotDiagram(file, 1, depGraphDrawing);
				i->markOperatorDrawn();
				file.close();
				// check for subcomponents
				if(! i->getSubComponentListR().empty() ){
				//recursively call to print subcomponents
					drawDotDiagram(i->getSubComponentListR());
				}

				//output the dot code to file
				//	for global operators, this is done only once
			} catch (std::string &s) {
				cerr << "Exception while generating '" << i->getName() << "': " << s << endl;
			}
		}
	}


	void UserInterface::outputVHDL() {
	  ofstream file;
	  file.open(outputFileName.c_str(), ios::out);
	  outputVHDLToFile(file);
	  file.close();
	}





	// Get the value corresponding to a key, case-insensitive
	string getVal(vector<string>& args, string keyArg){
		// convert to lower case. not efficient to do this each time but hey, this is a user interface.
		std::transform(keyArg.begin(), keyArg.end(), keyArg.begin(), ::tolower);
		vector<string>::iterator i = args.begin();
		// string opName=*i;
		i++;
		while (i != args.end()){
			size_t eqPos = i->find('=');
			if(string::npos==eqPos || 0==eqPos)
				throw ("This doesn't seem to be a key=value pair: " + *i);
			string key= i->substr(0,eqPos);
			// convert to lower case
			std::transform(key.begin(), key.end(), key.begin(), ::tolower);
			if(key==keyArg) {
				//cerr <<"  found " << key << endl;
				string val= i->substr(eqPos+1, string::npos);
				//cerr <<"  val= " << val << endl;
				// now remove this parameter from the args
				args.erase(i);
				return val;
				cerr <<"  val= " << val << endl;
			}
			i++;
		}
		return ""; // not found
	}

	// The following are helper functions to make implementation of factory parsers trivial
	// The code is not efficient but who cares: it is simple to maintain.
	// Beware, args[0] is the operator name, so that we may look up the doc in the factories etc.


	void UserInterface::throwMissingArgError(string opname, string key){
				throw (opname +": argument " + key + " not provided, and there doesn't seem to be a default value."
							 +"\n" +  getFactoryByName(opname) -> getFullDoc());

	}

	void UserInterface::parseString(vector<string> &args, string key, string* variable, bool genericOption){
		string val=getVal(args, key);
		if(val=="") {
			if(genericOption)
				return; // do nothing
			// key not given, use default value
			val = getFactoryByName(args[0])->getDefaultParamVal(key);
			if (val=="")
				throwMissingArgError(args[0], key);
		}
		*variable = val;
	}

	void UserInterface::parseBoolean(vector<string>& args, string key, bool* variable, bool genericOption){
		string val=getVal(args, key);
		if(val=="") {
			if(genericOption)
				return; // do nothing
			// key not given, use default value
			val = getFactoryByName(args[0])->getDefaultParamVal(key);
			if (val=="")
				throwMissingArgError(args[0], key);
		}
		if(val=="1" || val=="yes" || val=="true" || val=="Yes" || val=="True")
			*variable= true;
		else if(val=="0" || val=="no" || val=="false" || val=="No" || val=="False")
			*variable= false;
		else
				throw (args[0] +": expected boolean for argument " + key + ", got " + val);
	}

	void UserInterface::parseFloat(vector<string>& args, string key, double* variable, bool genericOption){
		string val=getVal(args, key);
		if(val=="") {
			if(genericOption)
				return; // do nothing
			// key not given, use default value
			val = getFactoryByName(args[0])->getDefaultParamVal(key);
			if (val=="")
				throwMissingArgError(args[0], key);
		}
		size_t end;
		double dval=stod(val, &end);
		if (val.length() == 0 || val.length() != end)
			throw (args[0] +": expecting a float for parameter " + key + ", got "+val);
		*variable= dval;
	}


	void UserInterface::parseInt(vector<string>& args, string key, int* variable, bool genericOption){
		string val=getVal(args, key);
		if(val=="") {
			if(genericOption)
				return; // do nothing
			// key not given, use default value
			val = getFactoryByName(args[0])->getDefaultParamVal(key);
			if (val=="")
				throwMissingArgError(args[0], key);
		}
		size_t end;
		int intval=stoi(val, &end);
		if (val.length() == 0 || val.length() != end)
			throw (args[0] +": expecting an int for parameter " + key + ", got "+val);
		*variable= intval;
	}

	void UserInterface::parseColonSeparatedStringList(vector<string> &args, string key, vector<string>* variable, bool genericOption){
		string val=getVal(args, key);
		if(val=="") {
			if(genericOption)
				return; // do nothing
			// key not given, use default value
			val = getFactoryByName(args[0])->getDefaultParamVal(key);
			if (val=="")
				throwMissingArgError(args[0], key);
		}
		std::stringstream ss;
    ss.str(val);
    std::string item;
		try {
			while (std::getline(ss, item, ':')) {
				variable->push_back(item);
			}
		}
		catch(std::exception &s){
			std::cerr << "Problem in parseColonSeparatedStringList for "<< val << endl << "got exception : "<<s.what()<<"\n";
			//factory->Usage(std::cerr);
			exit(EXIT_FAILURE);
		}
	}


	void UserInterface::parseColonSeparatedIntList(vector<string> &args, string key, vector<int>* variable, bool genericOption){
		string val=getVal(args, key);
		if(val=="") {
			if(genericOption)
				return; // do nothing
			// key not given, use default value
			val = getFactoryByName(args[0])->getDefaultParamVal(key);
			if (val=="")
				throwMissingArgError(args[0], key);
		}
		std::stringstream ss;
    ss.str(val);
    std::string item;
		try {
			while (std::getline(ss, item, ':')) {
				variable->push_back(std::stoi(item));
			}
		}
		catch(std::exception &s){
			std::cerr << "Problem in parseColonSeparatedIntList for "<< val << endl << "got exception : "<<s.what()<<"\n";
			//factory->Usage(std::cerr);
			exit(EXIT_FAILURE);
		}
	}



	void UserInterface::parsePositiveInt(vector<string> &args, string key, int* variable, bool genericOption){
		string val=getVal(args, key);
		if(val=="") {
			if(genericOption) {
				return; // option not found, but it was an option, so do nothing
			}
			else {			// key not given, use default value
				val = getFactoryByName(args[0])->getDefaultParamVal(key);
				if (val=="")
					throwMissingArgError(args[0], key);
 			}
		}
		size_t end;

		int intval=stoi(val, &end);
		if (val.length() == 0 || val.length() != end)
			throw (args[0] +": expecting an int for parameter " + key + ", got "+val);
		if(intval>=0)
			*variable = intval;
		else
			throw (args[0] +": expecting strictly positive value for " + key + ", got " + val );

	}


	void UserInterface::parseStrictlyPositiveInt(vector<string> &args, string key, int* variable, bool genericOption){
		string val=getVal(args, key);
		if(val=="") {
			if(genericOption)
				return; // do nothing
			// key not given, use default value (except if it is an initial option)
			if(args[0] != "$$initialOptions$$") {
					val = getFactoryByName(args[0])->getDefaultParamVal(key);
					if (val=="")
						throwMissingArgError(args[0], key);
			}
		}
		size_t end;
		int intval=stoi(val, &end);
		if (val.length() == 0 || val.length() != end)
			throw (args[0] +": expecting an int for parameter " + key + ", got "+val);
		if(intval>0)
			*variable = intval;
		else
			throw (args[0] +": expecting strictly positive value for " + key + ", got " + val );
	}


	void UserInterface::add( string name,
													 string description, /**< for the HTML doc and the detailed help */
													 string category,
													 string seeAlso,
													 string parameterList, /**< semicolon-separated list of parameters, each being name(type)[=default]:short_description  */
													 string extraHTMLDoc, /**< Extra information to go to the HTML doc, for instance links to articles or details on the algorithms */
													 parser_func_t parser,
													 unitTest_func_t unitTest	 ) {
		OperatorFactoryPtr factory(new OperatorFactory(name, description, category, seeAlso, parameterList, extraHTMLDoc, parser, unitTest));
		UserInterface::registerFactory(factory);
	}


#if 0
	const int outputToHTML=1;
	const int outputToConsole=2;

	string colorParameter(string s, int techno, bool optional) {
		string o
		if (techno==outputToHTML)
			o = "<code class=\"parametername\">" + s + "</code>";
		else 	if (techno==outputToConsole)
			o = (optional?COLOR_BOLD_BLUE_NORMAL:COLOR_BOLD) + s + COLOR_NORMAL;
		return o;
	}
#endif


	// TODO there is a lot of redundancy in the way global options are managed: look for all the occurences of "dependencyGraph" in this file
	string UserInterface::getFullDoc(){
		ostringstream s;
		s << "Usage: " << COLOR_BOLD << "flopoco  [options]  OperatorName parameters  [OperatorName parameters]..." << COLOR_NORMAL << endl;
		s << "  Both options and parameters are lists of " << COLOR_BOLD << "name=value" << COLOR_NORMAL << " pairs (with case-insensitive name)" << endl;
		s << COLOR_BLUE_NORMAL<< "Example: " << COLOR_NORMAL << "flopoco  frequency=300 target=Virtex5   FPExp  wE=8 wF=23 name=SinglePrecisionFPExp" << endl;
		s << "Generic options include:" << endl;
		s << "  " << COLOR_BOLD << "name" << COLOR_NORMAL << "=<string>:                override the the default entity name "<<endl;
		s << "  " << COLOR_BOLD << "outputFile" << COLOR_NORMAL << "=<string>:          override the the default output file name " << COLOR_RED_NORMAL << "(sticky option)" << COLOR_NORMAL <<endl;
		s << "  " << COLOR_BOLD << "target" << COLOR_NORMAL << "=<string>:              target FPGA (default " << defaultFPGA << ") " << COLOR_RED_NORMAL << "(sticky option)" << COLOR_NORMAL<<endl;
		s << "     Supported targets: Kintex7, StratixV, Virtex6, Zynq7000, VirtexUltrascalePus"<<endl;
		s << "  " << COLOR_BOLD << "frequency" << COLOR_NORMAL << "=<float>:            target frequency in MHz (default 400, 0 means: no pipeline) " << COLOR_RED_NORMAL << "(sticky option)" << COLOR_NORMAL<<endl;
		s << "  " << COLOR_BOLD << "plainVHDL" << COLOR_NORMAL << "=<0|1>:              use plain VHDL (default), or not " << COLOR_RED_NORMAL << "(sticky option)" << COLOR_NORMAL << endl;
		s << "  " << COLOR_BOLD << "useHardMult" << COLOR_NORMAL << "=<0|1>:            use hardware multipliers " << COLOR_RED_NORMAL << "(sticky option)" << COLOR_NORMAL<<endl;
		s << "  " << COLOR_BOLD << "useTargetOptimizations" << COLOR_NORMAL << "=<0|1>: use target specific optimizations (e.g., using primitives) " << COLOR_RED_NORMAL << "(sticky option)" << COLOR_NORMAL<<endl;
		s << "  " << COLOR_BOLD << "ilpSolver" << COLOR_NORMAL << "=<string>:           override ILP solver for operators optimized by ILP, has to match a solver name known by the ScaLP library" << COLOR_RED_NORMAL << "(sticky option)" << COLOR_NORMAL<<endl;
		s << "  " << COLOR_BOLD << "ilpTimeout" << COLOR_NORMAL << "=<int>:             sets the timeout in seconds for the ILP solver for operators optimized by ILP (default=3600)" << COLOR_RED_NORMAL << "(sticky option)" << COLOR_NORMAL<<endl;
		s << "  " << COLOR_BOLD << "compression" << COLOR_NORMAL << "=<heuristicMaxEff,heuristicPA,heuristicFirstFit,optimal,optimalMinStages>:        compression method (default=heuristicMaxEff)" << COLOR_RED_NORMAL << "(sticky option)" << COLOR_NORMAL<<endl;
		s << "  " << COLOR_BOLD << "tiling" << COLOR_NORMAL << "=<heuristicBasicTiling,optimal,heuristicGreedyTiling,heuristicXGreedyTiling,heuristicBeamSearchTiling>:        tiling method (default=heuristicBasicTiling)" << COLOR_RED_NORMAL << "(sticky option)" << COLOR_NORMAL<<endl;
        s << "  " << COLOR_BOLD << "hardMultThreshold" << COLOR_NORMAL << "=<float>: unused hard mult threshold (O..1, default 0.7) " << COLOR_RED_NORMAL << "(sticky option)" << COLOR_NORMAL<<endl;
		s << "  " << COLOR_BOLD << "generateFigures" << COLOR_NORMAL << "=<0|1>:generate SVG graphics (default off) " << COLOR_RED_NORMAL << "(sticky option)" << COLOR_NORMAL << endl;
		s << "  " << COLOR_BOLD << "verbose" << COLOR_NORMAL << "=<int>:        verbosity level (0-4, default=1)" << COLOR_RED_NORMAL << "(sticky option)" << COLOR_NORMAL<<endl;
		s << "  " << COLOR_BOLD << "dependencyGraph" << COLOR_NORMAL << "=<no|compact|full>: generate data dependence drawing of the Operator (default no) " << COLOR_RED_NORMAL << COLOR_NORMAL<<endl;
		s << "Sticky options apply to the rest of the command line, unless changed again" <<endl;
		s <<endl;
		s <<  COLOR_BOLD << "List of operators with command-line interface"<< COLOR_NORMAL << " (a few more are hidden inside FloPoCo)" <<endl;
		// The following is an inefficient double loop to avoid duplicating the data structure: nobody needs efficiency here
		for(auto catIt: UserInterface::categories) {
			string cat =  catIt.first;
			string catDesc =  catIt.second;
			s <<COLOR_BOLD_MAGENTA_NORMAL << "========"<< catDesc << "========"<< COLOR_NORMAL << endl;
			for(auto it: UserInterface::factoryList) {
				OperatorFactoryPtr f =  it.second;
				if(cat == f->m_category)
					s << f -> getFullDoc();
			}
		}
		return s.str();
	}


	void UserInterface::buildHTMLDoc(){
		ifstream versionfile;
		versionfile.open("VERSION", ios::in);
		string version;
		versionfile >> version;
		versionfile.close();
		ofstream file;
		file.open("doc/web/operators_" + version + ".html", ios::out);
		file << "<!DOCTYPE html>" << endl;
		file << "<html>" << endl;
		file << "<head>" << endl;
		file << "<link rel=\"stylesheet\" href=\"flopoco.css\">" << endl;
		file << "<meta charset=\"utf-8\"> " << endl;
		file << "<title>FloPoCo user manual</title>" << endl;
		file << "</head>" << endl;
		file << "<body>" << endl;
		file << "<h1> Operator list for FloPoCo version " << version << "</h1>" << endl;

		// The following is an inefficient double loop to avoid duplicating the data structure: nobody needs efficiency here
		for(auto catIt: UserInterface::categories) {
			string cat =  catIt.first;
			string catDesc =  catIt.second;
			file << "<h3>" << catDesc << "</h3>" << endl;
			for(auto it: UserInterface::factoryList) {
				OperatorFactoryPtr f =  it.second;
				if(cat == f->m_category)
				 file << f -> getHTMLDoc();
			}
		}
		file << "</body>" << endl;
		file << "</html>" << endl;
		file.close();
	}

	void UserInterface::buildAutocomplete()
	{
		ofstream file;
		file.open("flopoco_autocomplete", ios::out);
		file << "#\t\t\t\t Flopoco autocomplete file" << endl;
		file << "#" << endl;
		file << "#\tIf you don't know what to do with this file, please delete it and run" << endl;
		file << "#\t\t\tflopoco BuildAutocomplete" << endl;
		file <<endl;
		size_t indent_level = 0;
		const auto tabber = [&file, &indent_level](string content){
				for(size_t i = 0 ; i < indent_level; i++)
				{
					file << "\t";
				}
				file << content << endl;
			};

		string operatorList;
		{
		for(auto it: UserInterface::factoryList) {
			OperatorFactoryPtr f =  it.second;

				file << f->getOperatorFunctions();
				file << endl;
				operatorList += f->name();
				//				if(it + 1 != factoryList.end())
				operatorList += " ";
			}
		}

		string specialtargetList;
		{
			vector<string>::const_iterator it;
			for( it = special_targets.begin() ;
				 it != special_targets.end() ;
				 it++ )
			{

				specialtargetList += *it;
				if(it + 1 != special_targets.end())
				{
					specialtargetList+= " ";
				}
			}
		}

		//Parsing string to know last operator name
		tabber("# echo the name of the last operator name on the line");
		tabber("_getLastOp ()");
		tabber("{");
		indent_level++;
		tabber("local opList reponse pipedOpList");
		tabber("opList=\""+operatorList+" "+specialtargetList+"\";");
		tabber("pipedOpList=\"@(${opList// /|})\"");
		tabber("reponse=\"\"");
		file << endl;
		tabber("for i in $1; do");
		indent_level++;
		tabber("case $i in");
		indent_level++;
		tabber("$pipedOpList ) reponse=$i;;");
		indent_level--;
		tabber("esac"); // fin case
		indent_level--;
		tabber("done"); //fin for
		tabber("echo $reponse");
		indent_level--;
		tabber("}");
		file << endl;

		//programm completion function
		tabber("_mandatoryoptions_()");
		tabber("{");
		indent_level++;
		tabber("echo ''");
		indent_level--;
		tabber("}");
		file << endl;

		tabber("_nonmandatoryoptions_()");
		tabber("{");
		indent_level++;
		stringstream buf;
		for(option_t option : options)
		{
			buf << option.first << " " ;
		}
		tabber("echo ' "+buf.str()+"'");
		indent_level--;
		tabber("}");
		file << endl;

		tabber("_optionvalues_()");
		tabber("{");
		indent_level++;
		tabber("declare -A optionmap");
		for (option_t option : options) {
			if(!option.second.empty())
			{
				buf.str(string());
				for(string value : option.second)
				{
					buf << value << " ";
				}
				tabber("optionmap+=(['"+option.first+"']=\""+buf.str()+"\")");
			}
		}
		tabber("echo ${optionmap[$1]}");
		indent_level--;
		tabber("}");
		file << endl ;

		tabber("_exclude_lists()"); //exlude from $1 content of $2
		tabber("{");
		indent_level++;
		tabber("local response=\"\" tmpbuf tmpbuf2 len1 len2");
		tabber("tmpbuf2=\" $2 \"");
		tabber("len2=${#tmpbuf2}");
		tabber("for item in $1 ; do");
		tabber("tmpbuf=${tmpbuf2/ $item /}");
		tabber("len1=${#tmpbuf}");
		indent_level++;
		tabber("if (( $len1 == $len2 )) ; then");
		indent_level++;
		tabber("response=\"$response $item\"");
		indent_level--;
		tabber("fi");
		indent_level--;
		tabber("done");
		tabber("echo $response");
		indent_level--;
		tabber("}");
		file << endl;

		//control function
		tabber("_flopoco()");
		tabber("{");
		indent_level++;
		tabber("shopt -s extglob");
		tabber("local saisie lastOp=\"\"");
		tabber("");
		tabber("#récupération de la ligne");
		tabber("saisie=$COMP_LINE");
		file << endl;
		tabber("lastOp=`_getLastOp \"$saisie\"`");
		file << endl;
		tabber("#Si l'opérateur est une cible finale on s'arrête");
		tabber("spetrgtlst=\""+specialtargetList+"\"");
		tabber("case $lastOp in");
		indent_level++;
		tabber("@(${spetrgtlst// /|}) ) return ;;");
		indent_level--;
		tabber("esac");
		file << endl;
		tabber("#On ne s'intéresse qu'à ce qui se situe après l'opérateur'");
		tabber("if [ -z \"$lastOp\" ] ; then");
		indent_level++;
		tabber("saisie=${saisie##*${1}}");
		indent_level--;
		tabber("else");
		indent_level++;
		tabber("saisie=${saisie##*${lastOp}}");
		indent_level--;
		tabber("fi");
		tabber("");
		tabber("local optionset=\"\"");
		tabber("local currentOpt=\"\"");
		tabber("local currentValue=\"\"");
		tabber("local remaininglength=\"1\"");
		tabber("local -i beforeEqual=0");
		file << endl;
		tabber("for wordgroup in $saisie ; do");
		indent_level++;
		tabber("beforeEqual=0");
		tabber("currentOpt=${wordgroup%%=*}");
		tabber("currentValue=${wordgroup#*=}");
		tabber("optionset=\"$optionset $currentOpt\"");
		tabber("local buf=${saisie##*$wordgroup}");
		tabber("remaininglength=${#buf}");
		tabber("if [ \"$currentOpt\" != \"$wordgroup\" ] ; then");
		indent_level++;
		tabber("beforeEqual=1");
		indent_level--;
		tabber("fi");
		indent_level--;
		tabber("done");
		file << endl;;
		tabber("if [ \"$remaininglength\" -gt 0 ] ; then");
		indent_level++;
		tabber("beforeEqual=0");
		tabber("currentOpt=\"\"");
		indent_level--;
		tabber("fi");
		file << endl;
		tabber("# possible set");
		tabber("local mopts nmopts nextOps");
		file << endl;
		tabber("# allowed set");
		tabber("local amopts anmopts anextOps");
		file << endl;
		tabber("amopts=`_mandatoryoptions_$lastOp`");
		tabber("anmopts=`_nonmandatoryoptions_$lastOp`");
		tabber("anextOps=\""+operatorList+"\"");
		file << endl;
		tabber("if [ -z \"$lastOp\" ] ; then");
		indent_level++;
		tabber("anextOps=\"$anextOps $spetrgtlst\"");
		indent_level--;
		tabber("fi");
		file << endl;
		tabber("mopts=`_exclude_lists \"$amopts\" \"$optionset\" `");
		tabber("case $currentOpt in");
		tabber("@(${amopts// /|})) mopts=\"$mopts $currentOpt\";;");
		tabber("esac");
		file << endl;
		tabber("nmopts=`_exclude_lists \"$anmopts\" \"$optionset\" `");
		tabber("case $currentOpt in");
		tabber("@(${anmopts// /|})) nmopts=\"$nmopts $currentOpt\";;");
		tabber("esac");
		tabber("local optionslist=\"$mopts $nmopts\"");
		tabber("optionslist=${optionslist// /|}");
		file << endl;
		tabber("#if we have no mandatory options left");
		tabber("if [ -z \"${mopts// /}\" ] ; then");
		indent_level++;
		tabber("nextOps=$anextOps");
		indent_level--;
		tabber("fi");
		file << endl;
		tabber("#if we are beginning a new word or completing name");
		tabber("if [ \"$beforeEqual\" -eq 0 ] ; then");
		indent_level++;
		tabber("#echo \"Completing new word\"");
		tabber("local -i candidatecounter=0");
		tabber("for candidate in $mopts $nmopts $nextOps ; do");
		indent_level++;
		tabber("case $candidate in");
		tabber("$currentOpt*)");
		indent_level++;
		tabber("COMPREPLY+=(\"$candidate\")");
		tabber("((candidatecounter++))");
		tabber(";;");
		indent_level--;
		tabber("esac");
		indent_level--;
		tabber("done");
		tabber("if (( candidatecounter == 1 )) ; then");
		indent_level++;
		tabber("#echo \"only one candidate\"");
		tabber("case ${COMPREPLY[0]} in");
		tabber("@($optionslist))");
		indent_level++;
		tabber("#echo \"candidate is option\"");
		tabber("compopt -o nospace");
		tabber("COMPREPLY[0]=\"${COMPREPLY[0]}=\";;");
		indent_level--;
		tabber("esac");
		indent_level--;
		tabber("fi");
		tabber("return");
		indent_level--;
		tabber("fi");
		file << endl;
		tabber("#we are completing option value");
		tabber("#echo \"completing option value\"");
		tabber("local availableValues=`_optionvalues_$lastOp $currentOpt`");
		tabber("for value in $availableValues ; do");
		indent_level++;
		tabber("case $value in");
		tabber("$currentValue*) COMPREPLY+=(\"$value\");;");
		tabber("esac");
		indent_level--;
		tabber("done");
		indent_level--;
		tabber("}");
		file << endl;

		tabber("# Actual completion");
		tabber("complete -F _flopoco flopoco");
		file.close();

		cout << "Bash autocomplete successfully generated !!" << endl;
		cout << "(this is in no case a warranty nor a guarantee that the script will work)" << endl;
		cout << "In order to make it work, we recommend that you link this file in your ~/.bash_completion.d directory" <<
			endl << "you might have to create the ~/.bash_completion.d directory" << endl << endl;
		cout << "\t\tmv flopoco_autocomplete ~/.bash_completion.d/flopoco" << endl << endl;
		cout << "and then add the following line to your .bashrc :"<< endl << endl;
		cout << "\t\t. ~/.bash_completion.d/flopoco" << endl;
	}





	////////////////// Operator factory /////////////////////////
	// Currently very rudimentary

	string OperatorFactory::getFullDoc(){
		ostringstream s;
		s <<COLOR_BOLD_RED_NORMAL << name() << COLOR_NORMAL <<": " << m_description << endl;
		for (unsigned i=0; i<m_paramNames.size(); i++) {
			string pname = m_paramNames[i];
			s << "  " << ("" != m_paramDefault[pname]?COLOR_BOLD_BLUE_NORMAL:COLOR_BOLD) << pname <<COLOR_NORMAL<< " (" << m_paramType[pname] << "): " << m_paramDoc[pname] << "  ";
			if("" != m_paramDefault[pname])
				s << COLOR_RED_NORMAL << "  (optional, default value is " << m_paramDefault[pname] <<")"<< COLOR_NORMAL;
			s<< endl;
		}
		return s.str();
	}


	string OperatorFactory::getHTMLDoc(){
		ostringstream s;
		s << "<dl>"<<endl;
		s << "<dt class=\"operatorname\">" <<  name() << "</dt>"<< endl
			<< "<dd class=\"operatordescription\">"<< m_description << "</dd>" << endl
			<< "<dd><em>Parameters:</em> <dl>" << endl;
		for (unsigned i=0; i<m_paramNames.size(); i++) {
			string pname = m_paramNames[i];
			if("" != m_paramDefault[pname])
				s << "<span class=\"optionalparam\"> " ;
			s << "<dt> <code class=\"parametername\">" << pname << "</code>  (<code class=\"parametertype\">" << m_paramType[pname] << "</code>) " ;
			if("" != m_paramDefault[pname])
				s << "  (optional, default value is " << m_paramDefault[pname] <<")";
			s << "</dt>";
			s << "<dd>" << m_paramDoc[pname] << "</dd>";
			if("" != m_paramDefault[pname])
				s << " </span>";
			s<< endl;
		}
		s << "</dl></dd>"<<endl;
		if("" != m_extraHTMLDoc)
			s << "<dd>" << m_extraHTMLDoc << "</dd>"<<endl;
		s << "</dl>"<<endl;
		return s.str();
	}

	string OperatorFactory::getOperatorFunctions(void)
	{
		stringstream s;
		stringstream buf;
		size_t indent_level = 0;
		const auto tabber = [&s, &indent_level](string content){
				for(size_t i = 0 ; i < indent_level; i++)
				{
					s << "\t";
				}
				s << content << endl;
			};

		vector<string> mandatoryOptions;
		vector<string> nonMandatoryOptions;

		for (string optionName : m_paramNames) {
			if (getDefaultParamVal(optionName) == "") {
				mandatoryOptions.push_back(optionName);
			} else {
				nonMandatoryOptions.push_back(optionName);
			}
		}

		tabber("_mandatoryoptions_"+m_name+"()");
		tabber("{");
		indent_level++;
		buf.str(string());
		buf << "echo \" ";
		for (string optionName : mandatoryOptions) {
			buf << optionName << " ";
		}
		buf << "\";";
		tabber(buf.str());
		indent_level--;
		tabber("}");
		s << endl;
		tabber("_nonmandatoryoptions_"+m_name+"()");
		tabber("{");
		indent_level++;
		buf.str(string());
		buf << "echo \" ";
		for (string optionName : nonMandatoryOptions) {
			buf << optionName << " ";
		}
		buf << "\";";
		tabber(buf.str());
		indent_level--;
		tabber("}");
		s << endl;
		tabber("_optionvalues_"+m_name+"()");
		tabber("{");
		indent_level++;
		tabber("declare -A valueList");
		for(string option : m_paramNames)
		{
			map<string, string>::iterator it = m_paramType.find(option);
			if(it != m_paramType.end() && (*it).second == "bool")
			{
				tabber("valueList+=(["+option+"]=\"0 1\")");
			}
		}
		tabber("echo ${valueList[\"$1\"]}");
		indent_level--;
		tabber("}");
		return s.str();
	}


	string OperatorFactory::getDefaultParamVal(const string& key){
		return  m_paramDefault[key];
	}

	OperatorFactory::OperatorFactory(
																	 string name,
																	 string description, /* for the HTML doc and the detailed help */
																	 string category,
																	 string seeAlso,
																	 string parameters, /*  semicolon-separated list of parameters, each being name(type)[=default]:short_description  */
																	 string extraHTMLDoc, /* Extra information to go to the HTML doc, for instance links to articles or details on the algorithms */
																	 parser_func_t parser ,
																	 unitTest_func_t unitTest )
		: m_name(name), m_description(description), m_category(category), m_seeAlso(seeAlso), m_extraHTMLDoc(extraHTMLDoc), m_parser(parser), m_unitTest(unitTest)
	{
		int start;
		// Parse the parameter description
		// The internet says: this will remove newlines
		parameters.erase (remove (parameters.begin(), parameters.end(), '\n'), parameters.end());

		start=0;
		while(start<(int)parameters.size()){
			int end=parameters.find(';', start);
			string part;
			if(end==-1)
				part=parameters.substr(start, end);
			else
				part=parameters.substr(start, end-start);
			if(part.size()!=0) {
				int nameEnd = part.find('(', 0);
				string name=part.substr(0, nameEnd);
				// cout << "Found parameter: {" << name<<"}";
				m_paramNames.push_back(name);

				int typeEnd = part.find(')', 0);
				string type=part.substr(nameEnd+1, typeEnd-nameEnd-1);
				//cout << " of type  {" << type <<"}";
				if(type=="bool" || type=="int" || type=="real" || type=="string" || type=="intlist")
					m_paramType[name] = type;
				else {
					ostringstream s;
					s << "OperatorFactory: Type (" << type << ")  not a supported type.";
					throw s.str();
				}
				int j = typeEnd+1;
				m_paramDefault[name]="";
				if (part[j]=='=') {
					//parse default value
					j++;
					int defaultValEnd = part.find(':', 0);
					string defaultVal=part.substr(j, defaultValEnd-j);
					//cout << " default to {" <<  defaultVal << "}  ";
					m_paramDefault[name]=defaultVal;
					j=defaultValEnd;
				}
				if(part[j]==':') {
					// description
					j++;
					while (part[j]==' ' || part[j]=='\t') j++; // remove leading spaces and tabs
					string description = part.substr(j, -1);
					m_paramDoc[name] = description;
					// cout << " :  {" << description <<"}" << endl;
				}
				else throw("OperatorFactory: Error parsing parameter description");
			}
			if(end==-1)
				break;
			// else prepare to parse next parameter
			start=end+1;
			while (parameters[start]==' ' || parameters[start]=='\t') start++;
		}
	}

	const vector<string>& OperatorFactory::param_names(void) const{
		return m_paramNames;
	}


	OperatorPtr OperatorFactory::parseArguments(OperatorPtr parentOp, Target* target, vector<string> &args	)	{
		return m_parser(parentOp, target, args);
	}


	TestList OperatorFactory::unitTestGenerator(int index)	{
		TestList testList;
		if(m_unitTest != nullptr)				{
			testList = m_unitTest(index);
		}
		return testList;
	}

}; // flopoco
