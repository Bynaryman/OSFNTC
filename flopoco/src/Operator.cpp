/*
	The base Operator class, every operator should inherit it

	Author : Florent de Dinechin, Bogdan Pasca, Matei Istoan

	Initial software.
	Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
	2008-2010.
  All rights reserved.

*/


/*


	In FloPoCo, each instance is associated with a unique Operator.
	The same Operator can not be reused in two instances.
	The scheduler works on the operator graph assuming this, and it makes life easier.

	One exception is "shared" or "atomic" Operators.
	- they are meant for very simple, unpipelined operators: compressors, small tables in IntConstDiv or FPDiv;
	- unfortunately they tend to have quite large VHDL, especially the tables.
	- the scheduler shouldn't even need to know if an instance is shared or not.
	- the Operator itself is replicated in the operator graph, flagged as "shared".
	- this means the VHDL is replicated, too. It would be nice if it was lexed only once, though, and replicated thereafter.
	- eventually, the VHDL of atomic operators will be generated only once, in non-pipelined mode.


	As the signal graph is built in-place to be scheduled, what we want to replicate in shared instance is _their signals (with timing labels)_, not the Operator itself with its VHDL.


	TODO the op input of inPortMap is no longer used.



*/

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <set>
#include "Operator.hpp"  // Useful only for reporting. TODO split out the REPORT and THROWERROR #defines from Operator to another include.
#include "utils.hpp"
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/uniform_int.hpp>


namespace flopoco{


	// global variables used through most of FloPoCo,
	// to be encapsulated in something, someday?
	int Operator::uid = 0; 										//init of the uid static member of Operator
	int verbose=0;

	Operator::Operator(Target* target): Operator(nullptr, target){
		REPORT(INFO, "Operator  constructor without parentOp is deprecated");
	}

	Operator::Operator(Operator* parentOp, Target* target){
		vhdl.setOperator(this);
		stdLibType_                 = 0;						// unfortunately this is the historical default.
		target_                     = target;
		pipelineDepth_              = 0;

		myuid                       = getNewUId();
		architectureName_			= "arch";
		indirectOperator_           = NULL;
		hasDelay1Feedbacks_         = false;

		isShared_                   = false;
		isTopLevelDotDrawn_ 		= false;
		isLibraryComponent_         = false;
		noParseNoSchedule_          = false;
		isOperatorScheduled_        = false;

 		parentOp_                   = parentOp;
		isOperatorApplyScheduleDone_= false;

		// Currently we set the pipeline and clock enable from the global target.
		// This is relatively safe from command line, in the sense that they can only be changed by the command-line,
		// so each sub-component of an operator will share the same target.
		// It also makes the subcomponent calls easier: pass clock and ce without thinking about it.
		// It is not very elegant because if the operator is eventually combinatorial, it will nevertheless have a clock signal.
		if(target_->isPipelined())
			setSequential();
		else
			setCombinatorial();

		setClockEnable(target_->useClockEnable());

		//------- Resource estimation ----------------------------------
		resourceEstimate << "Starting Resource estimation report for entity: " << uniqueName_ << " --------------- " << endl;
		resourceEstimateReport << "";

		reHelper = new ResourceEstimationHelper(target_, this);
		reHelper->initResourceEstimation();

		reActive = false;
		//--------------------------------------------------------------

		//------- Floorplanning ----------------------------------------
		floorplan << "";

		flpHelper = new FloorplanningHelper(target_, this);
		flpHelper->initFloorplanning(0.75); 							// ratio (to what degree will the sub-components' bounding boxes
		// be filled), see Tools/FloorplanningHelper
		//--------------------------------------------------------------
	}





	void Operator::addSubComponent(OperatorPtr op) {
		// Check it is already present
		OperatorPtr alreadyPresent=nullptr;
		for (auto i: subComponentList_){
			if( op->getName() == i->getName() ) {
				alreadyPresent=i;
				// REPORT(DEBUG,"Operator::addToGlobalOpList(): " << op->getName() <<" already present in globalOpList");
			}
		}

		if(alreadyPresent) {
			if( op->isShared() )
				return;
			else
				THROWERROR("addSubComponent(): an operator named " << op->getName() << " already exists" );
		}
		else{
			subComponentList_.push_back(op);
			if(op->isShared() )
				UserInterface::addToGlobalOpList(op);
		}
	}


	OperatorPtr Operator::getSubComponent(string name){
		for (auto op: subComponentList_) {
			if (op->getName()==name)
				return op;
		}
		return NULL;
	}


	bool Operator::isOperatorDrawn()
	{
		return isTopLevelDotDrawn_;
	}

	void Operator::markOperatorDrawn()
	{
		isTopLevelDotDrawn_ = true;
	}

	void Operator::addInput(const std::string name, const int width, const bool isBus) {
		//search if the signal has already been declared
		if (isSignalDeclared(name)) {
			//if yes, signal the error
			THROWERROR("In addInput, signal " << name << " seems to already exist");
		}

		//create a new signal for the input
		// initialize its members
		Signal *s = new Signal(this, name, Signal::in, width, isBus) ; // default TTL and cycle OK

		//add the signal to the input signal list and increase the number of inputs
		ioList_.push_back(s);
		//add the signal to the signal dictionary
		signalMap_[name] = s;

		//connect the signal just created, if this is a subcomponent
		connectIOFromPortMap(s);

		// add its lowercase version to the global list for sanity check
		allSignalsLowercased.insert(name);
}

	void Operator::addInput(const std::string name) {
		addInput(name, 1, false);
	}

	/*ambiguous to addInput(const std::string name) !!!
		void Operator::addInput(const char* name) {
		addInput(name, 1, false);
		}
	*/

	void Operator::addOutput(const std::string name, const int width, const int numberOfPossibleOutputValues, const bool isBus) {
		//search if the signal has already been declared
		if (isSignalDeclared(name)) {
			//if yes, signal the error
			THROWERROR("In addOutput, signal " << name<< " seems to already exist");
		}

		//create a new signal for the input
		// initialize its members
		Signal *s = new Signal(this, name, Signal::out, width, isBus);
		s -> setNumberOfPossibleValues(numberOfPossibleOutputValues);
		//add the signal to the output signal list and increase the number of inputs
		ioList_.push_back(s);
		//add the signal to the global signal list
		signalMap_[name] = s ;

		//connect the signal just created, if this is a subcomponent
		connectIOFromPortMap(s);

		for(int i=0; i<numberOfPossibleOutputValues; i++)
			testCaseSignals_.push_back(s);

		// add its lowercase version to the global list for sanity check
		allSignalsLowercased.insert(name);
}

	void Operator::addOutput(const std::string name) {
		addOutput (name, 1, 1, false);
	}

	/*ambiguous to addOutput(const std::string name)!!!
		void Operator::addOutput(const char* name) {
		addOutput (name, 1, 1, false);
		}
	*/

	void Operator::addFixInput(const std::string name, const bool isSigned, const int msb, const int lsb) {
		//search if the signal has already been declared
		if (isSignalDeclared(name)) {
			//if yes, signal the error
			THROWERROR("In addFixInput, signal " << name<< " seems to already exist");
		}

		//create a new signal for the input
		// initialize its members
		Signal *s = new Signal(this, name, Signal::in, isSigned, msb, lsb);

		//add the signal to the input signal list and increase the number of inputs
		ioList_.push_back(s);

		//add the signal to the signal dict
		signalMap_[name] = s ;

		//connect the signal just created, if this is a subcomponent
		connectIOFromPortMap(s);

		// add its lowercase version to the global list for sanity check
		allSignalsLowercased.insert(name);
	}

	void Operator::addFixOutput(const std::string name, const bool isSigned, const int msb, const int lsb, const int numberOfPossibleOutputValues) {
		//search if the signal has already been declared
		if (isSignalDeclared(name)) {
			//if yes, signal the error
			THROWERROR("In addFixOutput, signal " << name<< " seems to already exist");
		}

		//create a new signal for the input
		// initialize its members
		Signal *s = new Signal(this, name, Signal::out, isSigned, msb, lsb) ;
		s->setNumberOfPossibleValues(numberOfPossibleOutputValues);
		//add the signal to the output signal list and increase the number of outputs
		ioList_.push_back(s);

		//add the signal to the global signal list
		signalMap_[name] = s ;

		//connect the signal just created, if this is a subcomponent
		connectIOFromPortMap(s);

		for(int i=0; i<numberOfPossibleOutputValues; i++)
		  testCaseSignals_.push_back(s);

		// add its lowercase version to the global list for sanity check
		allSignalsLowercased.insert(name);
	}

	void Operator::addFPInput(const std::string name, const int wE, const int wF) {
		//search if the signal has already been declared
		if (isSignalDeclared(name)) {
			//if yes, signal the error
			THROWERROR("In addFPInput, signal " << name<< " seems to already exist");
		}

		//create a new signal for the input
		// initialize its members
		Signal *s = new Signal(this, name, Signal::in, wE, wF);
		//add the signal to the input signal list and increase the number of inputs
		ioList_.push_back(s);
		//add the signal to the global signal list
		signalMap_[name] = s ;
		//connect the signal just created, if this is a subcomponent
		connectIOFromPortMap(s);

		// add its lowercase version to the global list for sanity check
		allSignalsLowercased.insert(name);

	}

	void Operator::addFPOutput(const std::string name, const int wE, const int wF, const int numberOfPossibleOutputValues) {
		//search if the signal has already been declared
		if (isSignalDeclared(name)) {
			//if yes, signal the error
			THROWERROR("In addFPOutput, signal " << name<< " seems to already exist");
		}

		//create a new signal for the input
		// initialize its members
		Signal *s = new Signal(this, name, Signal::out, wE, wF);
		s -> setNumberOfPossibleValues(numberOfPossibleOutputValues);
		//add the signal to the output signal list and increase the number of outputs
		ioList_.push_back(s);
		//add the signal to the global signal list
		signalMap_[name] = s ;
		//connect the signal just created, if this is a subcomponent
		connectIOFromPortMap(s);

		for(int i=0; i<numberOfPossibleOutputValues; i++)
			testCaseSignals_.push_back(s);

		// add its lowercase version to the global list for sanity check
		allSignalsLowercased.insert(name);
	}


	void Operator::addIEEEInput(const std::string name, const int wE, const int wF) {
		//search if the signal has already been declared
		if (isSignalDeclared(name)) {
			//if yes, signal the error
			THROWERROR("In addIEEEInput, signal " << name<< " seems to already exist");
		}

		//create a new signal for the input
		// initialize its members
		Signal *s = new Signal(this, name, Signal::in, wE, wF, true);
		//add the signal to the input signal list and increase the number of inputs
		ioList_.push_back(s);
		//add the signal to the global signal list
		signalMap_[name] = s ;

		//connect the signal just created, if this is a subcomponent
		connectIOFromPortMap(s);

		// add its lowercase version to the global list for sanity check
		allSignalsLowercased.insert(name);
	}

	void Operator::addIEEEOutput(const std::string name, const int wE, const int wF, const int numberOfPossibleOutputValues) {
		//search if the signal has already been declared
		if (isSignalDeclared(name)) {
			//if yes, signal the error
			THROWERROR("In addIEEEOutput, signal " << name<< " seems to already exist");
		}

		//create a new signal for the input
		// initialize its members
		Signal *s = new Signal(this, name, Signal::out, wE, wF, true) ;
		s -> setNumberOfPossibleValues(numberOfPossibleOutputValues);
		//add the signal to the output signal list and increase the number of outputs
		ioList_.push_back(s);
		//add the signal to the global signal list
		signalMap_[name] = s ;

		//connect the signal just created, if this is a subcomponent
		connectIOFromPortMap(s);

		for(int i=0; i<numberOfPossibleOutputValues; i++)
			testCaseSignals_.push_back(s);

		// add its lowercase version to the global list for sanity check
		allSignalsLowercased.insert(name);
	}



	void Operator::connectIOFromPortMap(Signal *portSignal)
	{
		REPORT(DEBUG, "Entering connectIOFromPortMap for signal " <<  portSignal->getName() << " parentOp=" << parentOp_);

		//TODO: add more checks here
		//if this is a global operator or a shared instance, then there is nothing to be done
		if(parentOp_ == nullptr || this->isShared() ) {
			return;
		}

		Signal *connectionSignal = nullptr; // connectionSignal is the actual signal connected to portSignal
		map<std::string, string>::iterator itStart, itEnd;


		//check that portSignal is really an I/O signal
		if((portSignal->type() != Signal::in) && (portSignal->type() != Signal::out))
			THROWERROR("Signal " << portSignal->getName() << " is not an input or output signal");
		//select the iterators according to the signal type
		if(portSignal->type() == Signal::in){
			REPORT(FULL, "connectIOFromPortMap(" << portSignal->getName() <<") : this is an input ");
			itStart = parentOp_->tmpInPortMap_.begin();
			itEnd = parentOp_->tmpInPortMap_.end();
		}else{
			REPORT(FULL, "connectIOFromPortMap(" << portSignal->getName() <<") : this is an output ");
			itStart = parentOp_->tmpOutPortMap_.begin();
			itEnd = parentOp_->tmpOutPortMap_.end();
		}

		//check that portSignal exists on the parent operator's port map
		for(map<std::string, string>::iterator it=itStart; it!=itEnd; ++it)
			{
				if(it->first == portSignal->getName()){
					connectionSignal = parentOp_->getSignalByName(it->second);
					break;
				}
			}

		//check if any match was found in the port mappings
		if(connectionSignal == nullptr)
			THROWERROR("I/O port " << portSignal->getName() << " of operator " << getName()
								 << " is not connected to any signal of parent operator " << parentOp_->getName());
		//sanity check: verify that the signal that was found is actually part of the parent operator
		try{
			parentOp_->getSignalByName(connectionSignal->getName());
		}catch(string &e){
			THROWERROR("Signal " << connectionSignal->getName()
								 << " cannot be found in what is supposed to be its parent operator: " << parentOp_->getName());
		}

		REPORT(FULL, portSignal->getName() << "   connected to " << connectionSignal->getName() << " whose timing is cycle=" << connectionSignal->getCycle() << " CP=" << connectionSignal->getCriticalPath() );
		//now we can connect the two signals
		if(portSignal->type() == Signal::in)
			{
				//this is an input signal
				portSignal->addPredecessor(connectionSignal, 0);
				connectionSignal->addSuccessor(portSignal, 0);
			}else{
			//this is an output signal
			portSignal->addSuccessor(connectionSignal, 0);
			connectionSignal->addPredecessor(portSignal, 0);
		}

#if 0
		// TODO Check this is useful
		//if the port was connected to a signal automatically created,
		//	then copy the details of the port to the respective signal
		if(connectionSignal->incompleteDeclaration())
			{
				connectionSignal->copySignalParameters(portSignal);
				connectionSignal->setIncompleteDeclaration(false);
				connectionSignal->setCriticalPathContribution(0.0);
			}
#endif
	}



	Signal* Operator::getSignalByName(string name)	{
		//in case of a bit of a vector, get rid of the range (...)
		if( name.find("(")!=string::npos ){
			name = name.substr(0,name.find("("));
		}

		//search for the signal in the list of signals
		if(!isSignalDeclared(name)) {
			//signal not found, throw an error
			THROWERROR("In getSignalByName, signal " << name << " not declared (operator " << this << ").");
		}

		//signal found, return the reference to it
		return signalMap_[name];
	}

	bool Operator::isSignalDeclared(string name) {
		//in case of a bit of a vector, get rid of the range (...)
		if( name.find("(")!=string::npos ){
			name = name.substr(0,name.find("("));
		}

		return (signalMap_.find(name) != signalMap_.end());
	}


	vector<Signal*> Operator::getSignalList(){
		return signalList_;
	}

	void Operator::addHeaderComment(std::string comment){
		headerComment_ += "-- " + comment + "\n";
	}


	void Operator::setName(std::string operatorName){
		uniqueName_ = operatorName;
	}

	void Operator::setNameWithFreqAndUID(std::string operatorName){
		std::ostringstream o;
		o <<  operatorName <<  "_" ;
		o << "F"<<target_->frequencyMHz() ;
		o << "_uid" << getNewUId();
		uniqueName_ = o.str();
	}

	void  Operator::changeName(std::string operatorName){
		commentedName_ = uniqueName_;
		uniqueName_ = operatorName;
	}

	string Operator::getName() const{
		return uniqueName_;
	}

	int Operator::getNewUId(){
		Operator::uid++;
		return Operator::uid;
	}

	OperatorPtr Operator::setParentOperator(OperatorPtr parentOp){
		if(parentOp_ != nullptr && parentOp != nullptr) // The second test is to allow reset to nullptr
			THROWERROR("Parent operator already set for operator " << getName());
		parentOp_ = parentOp;

		return parentOp_;
	}


	// TODO remove probably the three following methods
	int Operator::getIOListSize() const{
		return ioList_.size();
	}

	vector<Signal*>* Operator::getIOList(){
		return &ioList_;
	}

	Signal* Operator::getIOListSignal(int i){
		return ioList_[i];
	}

	void Operator::setNoParseNoSchedule(){
		noParseNoSchedule_          = true;
	}
	bool Operator::noParseNoSchedule(){
		return noParseNoSchedule_;
	}

	void  Operator::outputVHDLSignalDeclarations(std::ostream& o) {
		for (unsigned int i=0; i < this->signalList_.size(); i++){
			Signal* s = this->signalList_[i];
			o << tab << s->toVHDL() << ";" << endl;
		}
	}



	void Operator::outputVHDLComponent(std::ostream& o, std::string name) {
		if(!isLibraryComponent()) //don't generate component declaration for library components
			{
				unsigned int i;

				o << tab << "component " << name << " is" << endl;
				if( !generics_.empty() ) {
					o << tab << tab << "generic ( ";
					std::map<string, string>::iterator it = generics_.begin();
					o << getSignalByName(it->first)->toVHDL();

					for( ++it; it != generics_.end(); ++it  ) {
						o << ";" << endl << tab << tab << it->first << " : " << getSignalByName(it->first)->toVHDL();
					}

					o << ");" << endl;
				}

				if (ioList_.size() > 0)
					{
						o << tab << tab << "port ( ";
						if(isSequential()) {
							// add clk, rst, etc. signals which are not member of iolist
							o << "clk";
							if(hasReset())
								o << ", rst";
							if(hasClockEnable())
								o << ", ce";
							o << " : in std_logic;" <<endl;
						}

						for (i=0; i<this->ioList_.size(); i++){
							Signal* s = this->ioList_[i];

							if (i>0 || isSequential()) // align signal names
								o << tab << "          ";
							o <<  s->toVHDL();
							if(i < this->ioList_.size()-1)
								o << ";" << endl;
						}
						o << tab << ");"<<endl;
					}
				o << tab << "end component;" << endl;
			}
	}

	void Operator::outputVHDLComponent(std::ostream& o) {
		this->outputVHDLComponent(o,  this->uniqueName_);
	}



	void Operator::outputVHDLEntity(std::ostream& o) {
		unsigned int i;

		if(uniqueName_.empty()) THROWERROR("No name was set for this operator.");

		o << "entity " << uniqueName_ << " is" << endl;
		if (ioList_.size() > 0)
			{
				o << tab << " port (";
				if(isSequential()) {
					// add clk, rst, etc. signals which are not member of iolist
					o << "clk";
					if(hasReset())
						o << ", rst";
					if(hasClockEnable())
						o << ", ce";
					o << " : in std_logic;" <<endl;
				}

				for (i=0; i<this->ioList_.size(); i++){
					Signal* s = this->ioList_[i];
					if (i>0 || isSequential()) // align signal names
						o << "          ";
					o << s->toVHDL();
					if(i < this->ioList_.size()-1)
						o<<";" << endl;
				}

				o << tab << ");"<<endl;
			}
		o << "end entity;" << endl << endl;
	}


	void Operator::setCopyrightString(std::string authorsYears){
		copyrightString_ = authorsYears;
	}

	void Operator::addAdditionalHeaderInformation(std::string headerString){
		additionalHeaderString_ += headerString;
	}

	std::string Operator::getAdditionalHeaderInformation()
	{
		return additionalHeaderString_;
	}

	void Operator::useStdLogicUnsigned() {
		stdLibType_ = 0;
	}

	/**
	 * Use the Synopsys de-facto standard ieee.std_logic_unsigned for this entity
	 */
	void Operator::useStdLogicSigned() {
		stdLibType_ = -1;
	}

	void Operator::useNumericStd() {
		stdLibType_ = 1;
	}

	void Operator::useNumericStd_Signed() {
		stdLibType_ = 2;
	}

	void Operator::useNumericStd_Unsigned() {
		stdLibType_ = 3;
	}

	int Operator::getStdLibType() {
		return stdLibType_;
	}

	void Operator::licence(std::ostream& o){
		licence(o, copyrightString_);
	}

	void Operator::additionalHeader(std::ostream& o){
		o << additionalHeaderString_;
	}


	void Operator::licence(std::ostream& o, std::string authorsyears){
		o<<"--------------------------------------------------------------------------------"<<endl;
		// centering the unique name
		int s, i;

		if(uniqueName_.size()<76)
			s = (76-uniqueName_.size())/2;
		else
			s=0;
		o<<"--"; for(i=0; i<s; i++) o<<" "; o  << uniqueName_ << endl;

		// if this operator was renamed from the command line, show the original name
		if(commentedName_!="") {
			if(commentedName_.size()<74) s = (74-commentedName_.size())/2; else s=0;
			o<<"--"; for(i=0; i<s; i++) o<<" "; o << "(" << commentedName_ << ")" << endl;
		}
		o<< headerComment_;
		o << "-- VHDL generated for " << getTarget()->getID() << " @ " << getTarget()->frequencyMHz() <<"MHz"  <<endl;

		o<<"-- This operator is part of the Infinite Virtual Library FloPoCoLib"<<endl;
		o<<"-- All rights reserved "<<endl;
		o<<"-- Authors: " << authorsyears <<endl;
		o<<"--------------------------------------------------------------------------------"<<endl;
	}


	void Operator::pipelineInfo(std::ostream& o){
		if(isSequential())
			o<<"-- Pipeline depth: " << getPipelineDepth() << " cycles"  <<endl;
		else
			o << "-- combinatorial"  <<endl;

		o << "-- Clock period (ns): " << (1.0e9/target_->frequency()) << endl;
		o << "-- Target frequency (MHz): " << target_->frequencyMHz() << endl;
	}

	void Operator::stdLibs(std::ostream& o){
		o << "library ieee;"<<endl
		  << "use ieee.std_logic_1164.all;"<<endl;

		if(stdLibType_==0){
			o << "use ieee.std_logic_arith.all;"<<endl
			  << "use ieee.std_logic_unsigned.all;"<<endl;
		}
		if(stdLibType_==-1){
			o << "use ieee.std_logic_arith.all;"<<endl
			  << "use ieee.std_logic_signed.all;"<<endl;
		}
		if(stdLibType_==1){
			o << "use ieee.numeric_std.all;"<<endl;
		}
		// ???
		if(stdLibType_==2){
			o << "use ieee.numeric_std.all;"<<endl
			  << "use ieee.std_logic_signed.all;"<<endl;
		}
		if(stdLibType_==3){
			o << "use ieee.numeric_std.all;"<<endl
			  << "use ieee.std_logic_unsigned.all;"<<endl;
		}

		o << "library std;" << endl
		  << "use std.textio.all;"<< endl
		  << "library work;"<<endl<< endl;
	};


	void Operator::outputVHDL(std::ostream& o) {
		this->outputVHDL(o, this->uniqueName_);
	}

	bool Operator::isSequential() {
		return isSequential_;
	}


	void Operator::setSequential() {
		isSequential_=true;
	}

	void Operator::setCombinatorial() {
		isSequential_=false;
	}


	int Operator::getPipelineDepth() {
		return pipelineDepth_;
	}




	int Operator::getCycleFromSignal(string name, bool report) {

		if(isSequential()) {
			Signal* s;

			try {
				s = getSignalByName(name);
			}catch(string &e) {
				THROWERROR("In getCycleFromSignal, " << endl << tab << e);
			}

			if(s->getCycle() < 0){
				THROWERROR("Signal " << name << " doesn't have (yet?) a valid cycle" << endl);
			}

			return s->getCycle();
		}else{
			//if combinatorial, everything happens at cycle 0
			return 0;
		}
	}

	double Operator::getCPFromSignal(string name, bool report)
	{
		Signal* s;

		try {
			s = getSignalByName(name);
		}
		catch(string &e) {
			THROWERROR("In getCPFromSignal, " << endl << tab << e);
		}

		return s->getCriticalPath();
	}

	double Operator::getCPContributionFromSignal(string name, bool report)
	{
		Signal* s;

		try {
			s = getSignalByName(name);
		}
		catch(string &e) {
			THROWERROR("In getCPContributionFromSignal, " << endl << tab << e);
		}

		return s->getCriticalPathContribution();
	}




	double Operator::getOutputDelay(string s)
	{
		Signal *signal = NULL;

		for(unsigned i=0; i<ioList_.size(); i++)
			if((ioList_[i]->getName() == s) && (ioList_[i]->type() == Signal::out))
				{
					signal = ioList_[i];
				}
		if(signal == NULL)
			THROWERROR("In getOutputDelay(): signal " << s << " not found" << endl);

		return signal->getCriticalPath();
	}

	string Operator::declare(string name, const int width, bool isbus, Signal::SignalType regType) {
		return declare(0.0, name, width, isbus, regType);
	}

	string Operator::declare(string name, Signal::SignalType regType) {
		return declare(0.0, name, 1, false, regType);
	}

	string Operator::declare(double criticalPathContribution, string name, Signal::SignalType regType) {
		return declare(criticalPathContribution, name, 1, false, regType);
	}

	string Operator::declare(double criticalPathContribution, string name, const int width, bool isbus, Signal::SignalType regType) {
		REPORT(FULL, "Declaring signal " << name << " in operator uid" << this->getuid());
		Signal* s;
		bool incompleteDeclaration;
		// check the signal doesn't already exist
		if(isSignalDeclared(name)) {
			THROWERROR("In declare(), signal " << name << " already exists" << endl);
		}

		if(name != "open") //ignore keyword 'open', this occurs for port maps with open output ports
			{
				// construct the signal (lifeSpan and cycle are reset to 0 by the constructor)
				s = new Signal(this, name, regType, width, isbus);
			}
		else
			{
				//consider 'open' as a constant (which has not to be declared)
				s = new Signal(this, name, Signal::constant, width, isbus);
			}

		if(width==-1)
			incompleteDeclaration=true;
		else
			incompleteDeclaration=false;

		// initialize the rest of its attributes
		initNewSignal(s, criticalPathContribution, regType, incompleteDeclaration);

		return name;
	}


	string Operator::declareFixPoint(string name, const bool isSigned, const int MSB, const int LSB, Signal::SignalType regType){
		return declareFixPoint(0.0, name, isSigned, MSB, LSB, regType);
	}


	string Operator::declareFixPoint(double criticalPathContribution, string name, const bool isSigned, const int MSB, const int LSB, Signal::SignalType regType){
		Signal* s;

		// check the signals doesn't already exist
		if(isSignalDeclared(name)) {
			THROWERROR("In declareFixPoint(), signal " << name << " already exists" << endl);
		}

		// construct the signal (lifeSpan and cycle are reset to 0 by the constructor)
		s = new Signal(this, name, regType, isSigned, MSB, LSB);

		initNewSignal(s, criticalPathContribution, regType, false);

		//set the flag for a fixed-point signal
		s->setIsFix(true);
		s->setIsFP(false);
		s->setIsIEEE(false);

		return name;
	}


	string Operator::declareFloatingPoint(string name, const int wE, const int wF, Signal::SignalType regType, const bool ieeeFormat){
		return declareFloatingPoint(0.0, name, wE, wF, regType, ieeeFormat);
	}


	string Operator::declareFloatingPoint(double criticalPathContribution, string name, const int wE, const int wF, Signal::SignalType regType, const bool ieeeFormat){
		Signal* s;

		// check the signals doesn't already exist
		if(isSignalDeclared(name)) {
			THROWERROR("In declareFixPoint(), signal " << name << " already exists" << endl);
		}

		// construct the signal (lifeSpan and cycle are reset to 0 by the constructor)
		s = new Signal(this, name, regType, wE, wF, ieeeFormat);

		initNewSignal(s, criticalPathContribution, regType, false);

		s->setIsFix(false);
		//set the flag for a floating-point signal
		if(ieeeFormat){
			s->setIsFP(false);
			s->setIsIEEE(true);
		}else{
			s->setIsFP(true);
			s->setIsIEEE(false);
		}
		return name;
	}



	void Operator::initNewSignal(Signal* s, double criticalPathContribution, Signal::SignalType regType, bool incompleteDeclaration)
	{
		//define its cycle, critical path and contribution to the critical path
		s->setCycle(0);
		s->setCriticalPath(0.0);
		if(UserInterface::pipelineActive_) {
			s->setCriticalPathContribution(criticalPathContribution);
		}
		else {
			s->setCriticalPathContribution(0);
		}
		//whether this is an incomplete signal declaration
		s->setIncompleteDeclaration(incompleteDeclaration);

		//initialize the signals predecessors and successors
		s->resetPredecessors();
		s->resetSuccessors();

		//add the signal to signalMap and signalList
		signalList_.push_back(s);
		signalMap_[s->getName()] = s;

		// add its lowercase version to the global list for sanity check
		allSignalsLowercased.insert(toLower(s->getName()));

	}

	void  Operator::resizeFixPoint(string lhsName, string rhsName, const int MSB, const int LSB, const int indentLevel){
		Signal* rhsSignal = getSignalByName(rhsName);
		bool isSigned = rhsSignal->isSigned();
		int oldMSB = rhsSignal->MSB();
		int oldLSB = rhsSignal->LSB();

		REPORT(DEBUG, "Resizing signal " << rhsName << " from (" << oldMSB << ", " << oldLSB << ") to (" << MSB << ", " << LSB << ")");

		for (int i=0; i<indentLevel; i++)
			vhdl << tab;

		vhdl << declareFixPoint(rhsSignal->getCriticalPathContribution(), lhsName, isSigned, MSB, LSB, rhsSignal->type()) << " <= ";

		// Cases (old is input, new is output)
		//            1            2W             3W        4         5E         6 E
		// Old:      ooooooo   oooooooo      oooooooooo    oooo     ooo               ooo
		// New:  nnnnnnnn        nnnnnnnn     nnnnnn      nnnnnnn       nnnn      nnn

		bool paddLeft, paddRight;
		int m,l, paddLeftSize, paddRightSize, oldSize; 	// eventually we take the slice m downto l of the input bit vector

		paddLeft      = MSB>oldMSB;
		paddLeftSize  = MSB-oldMSB;			// in case paddLeft is true
		paddRight     = LSB<oldLSB;
		paddRightSize = oldLSB-LSB;			// in case paddRight is true
		oldSize       = oldMSB-oldLSB+1;

		// Take input vector downto what ?
		if (LSB>=oldLSB) { // case 1 or 3
			l = LSB-oldLSB;
		}
		else {             // case 2 or 4
			l=0;
		}

		// and from what bit?
		if (MSB>oldMSB) { // cases 1 or 4
			m = oldSize-1;
		}
		else { // oldMSB>=MSB, cases 2 or 3
			if(MSB<oldMSB)
				REPORT(DETAILED, "Warning: cutting off some MSBs when resizing signal " << rhsName << " from ("
							 << oldMSB << ", " << oldLSB << ") to (" << MSB << ", " << LSB << ")");
			m = oldSize-(oldMSB-MSB)-1;
		}

		// Now do the work.
		// Possible left padding/sign extension
		if(paddLeft) {
			if(isSigned) 	{
				if(paddLeftSize == 1)
					vhdl << "(" << rhsName << of(oldSize-1) << ") & "; // sign extension
				else
					vhdl << "(" << paddLeftSize -1 << " downto 0 => " << rhsName << of(oldSize-1) << ") & "; // sign extension
			}
			else {
				vhdl << zg(paddLeftSize) << " & ";
			}
		}

		// copy the relevant bits
		vhdl << rhsName << range(m, l);

		// right padding
		if(paddRight) {
			vhdl << " & " << zg(paddRightSize);
		}

		vhdl << "; -- fix resize from (" << oldMSB << ", " << oldLSB << ") to (" << MSB << ", " << LSB << ")" << endl;
	}





	void Operator::addRegisteredSignalCopy(string copyName, string sourceName, Signal::ResetType regType, int pp_depth)
	{
		ostringstream result;
		Signal* s;
		try{
			s = getSignalByName(sourceName);
		}
		catch(string &e2) {
			THROWERROR("In addRegisteredSignalCopy(): " << e2);
		}
		s->setResetType(regType);
		vhdl << tab << declare(copyName, s->width(), s->isBus()) << " <= "<<sourceName<<"^"<<pp_depth<<";" << endl;
		// this ^ will be caught in doApplySchedule(). We could have arbitrary number of delays but I wait for a use case
		getSignalByName(copyName) -> setHasBeenScheduled(true); // so that the schedule can start from these signals -- lexicographic time is (0,0)
	}




	void Operator::disablePipelining(){
		UserInterface::pipelineActive_=false;
	}

	void Operator::enablePipelining(){
		UserInterface::pipelineActive_=true;
	}


	void Operator::outPortMap(OperatorPtr op, string componentPortName, string actualSignalName){
		REPORT(0, "Here is an obsolete version of outPortMap. Ignoring op...");
		outPortMap(componentPortName, actualSignalName);
	}

	void Operator::outPortMap(string componentPortName, string actualSignalName){

		//declare the signal only if not existing (existing signals may happen when port maps to different ranges are performed, hence we cannot treat this as an error)
		if(!isSignalDeclared(actualSignalName))
			{
				declare(actualSignalName, -1); // -1 for incomplete declaration
				REPORT(FULL,"outPortMap: Created incomplete " << actualSignalName);
			}

		/* should be removed soon:

		// check if the signal already exists, when we're supposed to create a new signal
		if(signalMap_.find(actualSignalName) !=  signalMap_.end()) {
		THROWERROR("In outPortMap(): signal " << actualSignalName << " already exists");
		}

		//create the new signal
		//	this will be an incomplete signal, as we cannot know the exact details of the signal yet
		//	the rest of the information will be completed by addOutput, which has the rest of the required information
		//	and add it to the list of signals to be scheduled
		declare(actualSignalName, -1); // -1 for incomplete declaration
		REPORT(FULL,"outPortMap: Created incomplete " << actualSignalName);
		*/
		// add the mapping to the output mapping list of Op
		tmpOutPortMap_[componentPortName] = actualSignalName;
	}



	void Operator::inPortMap(OperatorPtr op, string componentPortName, string actualSignalName){
		REPORT(0, "Here is an obsolete version of inPortMap. Ignoring op...");
		inPortMap(componentPortName, actualSignalName);
	}


	void Operator::inPortMap(string componentPortName, string actualSignalName){
		REPORT(DEBUG, "InPortMaP : " << componentPortName << " => "  << actualSignalName);

		//check if the signal already exists
		try{
			getSignalByName(actualSignalName);
		}
		catch(string &e2) {
			THROWERROR("In inPortMap(): " << e2);
		}

		// add the mapping to the input mapping list of Op
		tmpInPortMap_[componentPortName] = actualSignalName;
	}


	void Operator::setGeneric( string name, string value, int width, bool isBus ) {
		REPORT(DEBUG, "setGeneric: "<< getName() << " : " << name << " => "  << value);

		Signal *s = new Signal(this, name, Signal::constant, width, isBus);

		//add the signal to the signal dictionary
		signalMap_[name] = s;

		generics_.insert( std::make_pair( name, value ) );
	}

	void Operator::setGeneric(string name, const long value , int width, bool isBus) {
		setGeneric( name, std::to_string( value ), width, isBus );
	}

	void Operator::inPortMapCst(OperatorPtr op, string componentPortName, string constantValue){
		REPORT(0, "Here is an obsolete version of inPortMapCst. Ignoring op...");
		inPortMapCst(componentPortName, constantValue);
	}

	void Operator::inPortMapCst(string componentPortName, string constantValue){
		Signal *s;
		string name;
		double constValue;
		sollya_obj_t node;

		REPORT(DEBUG, "InPortMapCst: "<< " : " << componentPortName << " => "  << constantValue);
		// TODO: do we need to add the input port mapping to the mapping list of Op?
		// 		as this is a constant signal

		string org_constantValue = constantValue;
		//remove quotation marks from constant (as this produces sollya warnings)
		std::size_t found = constantValue.find("'");
		while(found!=std::string::npos)
		{
		constantValue.replace(found,1,"");
		found = constantValue.find("'"); //search for other "'"'s
		}

		//try to parse the constant
		node = sollya_lib_parse_string(constantValue.c_str());
		// If conversion did not succeed (i.e. parse error)
		if(node == 0)
			THROWERROR("Unable to parse string " << constantValue << " as a numeric constant" << endl);
		sollya_lib_get_constant_as_double(&constValue, node);
		sollya_lib_clear_obj(node);

		//create a new signal for constant input
		s = new Signal(this, join(org_constantValue, "_cst_", vhdlize(getNewUId())), Signal::constant, constValue);

		// TODO the following bit of code should move to Signal, for Signal::constant signals
		//set the timing for the constant signal, at cycle 0, criticalPath 0, criticalPathContribution 0
		s->setCycle(0);
		s->setCriticalPath(0.0);
		s->setCriticalPathContribution(0.0);
		s->setHasBeenScheduled(true);
		alreadyScheduled.insert(s);

		// add the newly created signal to signalMap and signalList
		signalList_.push_back(s);
		signalMap_[s->getName()] = s;

		tmpInPortMap_[componentPortName] = s->getName();
	}


	/*
		The logic of instance really depends on the sharedness.

		*** If the subcomponent is unique/not shared, the following order is expected (and ensured by newInstance)
		1/ schedule the actual inputs
		2/ build the tmpPortMapList by calling inPortMap and outPortMap
		3/ call the constructor of the subcomponents
		3.1/ its addInput() and addOutput recovers the IO maping from tmpPortMapList.
		For the outputs, the actuals are incompleteDeclarations
		3.2/ its signal graph is built
		4/ the declaration of the output signals is finalized by instance()

		*** If the subcomponent is shared, the following order is expected
		1/ call the constructor, put this operator in the globalOpList, schedule it
		2/ build  the tmpPortMapList  by calling inPortMap and outPortMap
		3/ call instance() clone the subcomponent's IO signal, and clone the relevant IO dependency.

	*/


	string Operator::instance(Operator* op, string instanceName, bool outputWarning){
		ostringstream o;

		if(outputWarning && ! op->isShared()) {
			REPORT(INFO, "instance() is deprecated except for shared operators, please use newInstance() instead");
		};

		// First, I/O sanity check: check that all the signals are connected
		for(auto i: *(op->getIOList())) 	{
			bool isSignalMapped = false;
			map<string, string>::iterator iterStart, iterStop;

			//set the start and stop values for the iterators, for either the input,
			//	or the output temporary port map, depending on the signal type
			if(i->type() == Signal::in)	{
				iterStart = tmpInPortMap_.begin();
				iterStop = tmpInPortMap_.end();
			}
			else{
				iterStart = tmpOutPortMap_.begin();
				iterStop = tmpOutPortMap_.end();
			}

			for(map<string, string>::iterator it=iterStart; it!=iterStop; it++)	{
				if(it->first == i->getName())		{
					//mark the signal as connected
					isSignalMapped = true;
					break;
				}
			}

			//if the port is not connected to a signal in the parent operator,
			//	then we cannot continue
			if(!isSignalMapped) {
				THROWERROR("In instance() while trying to create a new instance of "
									 << op->getName() << " called " << instanceName << ": input/output "
									 << i->getName() << " is not mapped to anything" << endl);
			}
		} // End of the I/O sanity check

#if 0
		if(!op->isOperatorScheduled()) {
			op->schedule();
		}
		if(op->isShared()) {
			op->applySchedule();
		}
#else // There is already a call to schedule() in newInstance, so the unique case is covered properly there
		// actually calling schedule() here for unique instances leads to bugs, as the scheduler manages to follow half-connected signals
		if(op->isShared() && !op->isOperatorScheduled() ) {
			op->schedule();
			op->applySchedule();
		}
#endif


		// Now begin VHDL output
		o << tab << instanceName << ": " << op->getName();
		o << endl;

		// INSERTED PART FOR PRIMITIVES
		if( !op->generics_.empty() ) {
			o << tab << tab << "generic map ( ";
			std::map<string, string>::iterator it = op->generics_.begin();
			o << it->first << " => " << it->second;

			for( ++it; it != op->generics_.end(); ++it  ) {
				o << "," << endl << tab << tab << it->first << " => " << it->second;
			}

			o << ")" << endl;
		}
		// INSERTED PART END

		o << tab << tab << "port map ( ";

		// build vhdl and erase portMap_
		if(op->isSequential())	{
			o << "clk  => clk";
			if (op->hasReset())
			  o << "," << endl << tab << tab << "           rst  => rst";
			if (op->hasClockEnable())
				o << "," << endl << tab << tab << "           ce => ce";
		}

		//build the code for the inputs
		map<string, string>::iterator it;
		string rhsString;
		vector<Signal*> inputActualList;

		for(it=tmpInPortMap_.begin(); it!=tmpInPortMap_.end(); it++){
			string actualName = it->second;
			Signal* actual = getSignalByName(actualName); // the connexion here is actual -> formal
			string formalName = it->first; // ... with actual in this and formal in op
			//			Signal* formal=op->getSignalByName(formalName);
			if((it != tmpInPortMap_.begin()) || op->isSequential())
				o << "," << endl <<  tab << tab << "           ";

			// The following code assumes that the IO is declared as standard_logic_vector
			// If the actual parameter is a signed or unsigned, we want to automatically convert it
			if(actual->type() == Signal::constant){
				rhsString = actualName.substr(0, actualName.find("_cst"));
			}else if(actual->isFix()){
				rhsString = std_logic_vector(actualName);
			}else{
				rhsString = actualName;
			}

			o << formalName << " => " << rhsString;
			if(op->isShared()){
				// shared instance: build a list of all the input signals, to be connected directly to the output in the dependency graph.
				inputActualList.push_back(actual);
			}
		}

		map<string, string> cloneNamesMap; // used to remember cloning information when we build instanceActualIO
		ostringstream outputSignalCopies;
		//build the code for the outputs
		for(it=tmpOutPortMap_.begin(); it!=tmpOutPortMap_.end(); it++)
			{
				string actualName = it->second;
				Signal* actual = getSignalByName(actualName); // the connexion here is actual -> formal
				string formalName = it->first; // ... with actual in this and formal in op
				Signal* formal=op->getSignalByName(formalName);
				REPORT(DEBUG, "instance: out port loop  " << actualName << " "<< actual->incompleteDeclaration()<<" " << formalName  );

				//the signal connected to the output should be an incompletely declared signal,
				//	so its information must be completed and it must be properly connected now
				if(actual->incompleteDeclaration())		{
					//copy the details from the output port
					actual->copySignalParameters(op->getSignalByName(formalName));
					actual->setIncompleteDeclaration(false); //mark the signal as completely declared
					if(op->isShared()){ // shared instance:  directly connect the inputs to the actual output in the dependency graph
						REPORT(DEBUG, "instance():  Actual parameter "<< actual->getUniqueName()  << " of shared instance " << op->getName());
						// relink the critical path info for the pipeline to work.
						double criticalPath = formal->getCriticalPath(); // this is OK because the subcomponent has been scheduled
						Signal* cloneOrActual = actual;
						if(isSequential()) {
							// We want to be able to pipeline the output signal using information from within the shared instance.
							// For this have to add an intermediate signal to receive the output of pipeline registers...
							// The actual has already been declared, so we declare a clone that will become the actual.
							if(actual->type() != Signal::constant) //ignore the case of an open output
								{
									string cloneName = actual->getName() + "_copy" +to_string(getNewUId());
									declare(cloneName, actual->width());
									Signal* clone = getSignalByName(cloneName);
									clone -> copySignalParameters(actual);
									// Now we want the clone to become the actual parameter
									outputSignalCopies << tab << actualName << " <= " << cloneName << "; -- output copy to hold a pipeline register if needed" << endl;
									actualName = cloneName; // will be consumed by the actual output of formal => actual below
									cloneOrActual = clone;
								}
						}
						actual->setCriticalPathContribution(criticalPath);
						cloneNamesMap[actual->getName()] = cloneOrActual->getName();

						for (auto i: inputActualList) {
							i->addSuccessor(cloneOrActual);
							cloneOrActual->addPredecessor(i);
							REPORT(FULL, "instance() added dependency "<< i->getUniqueName() << " -> "<<  cloneOrActual->getUniqueName() );
						}

					}
					else {	 	// unique instances: just connect both signals in the dependency graph
						actual->addPredecessor(formal);
						formal->addSuccessor(actual);
						//the new signal doesn't add anything to the critical path
						actual->setCriticalPathContribution(0.0);
					}
				}
				if(  (it != tmpOutPortMap_.begin())  ||   (tmpInPortMap_.size() != 0)   ||   op->isSequential()  )
					o << "," << endl <<  tab << tab << "           ";

				o << formalName << " => " << actualName;

			}

		o << ");" << endl;

		// add the possible copies of shared instance outputs
		o << outputSignalCopies.str();

		//add the operator to the subcomponent list/map (and possibly to globalOpList)
		addSubComponent(op);

		// populate the instance maps.
		// The following code is protected by the IO sanity checks above
		vector<string> actualIOList;
		for(auto i: *(op->getIOList())) {
			for(auto j: tmpInPortMap_)	{
				if(j.first == i->getName()) {
					actualIOList.push_back(j.second);
				}
			}
			for(auto j: tmpOutPortMap_) {
				if(j.first == i->getName()) {
					actualIOList.push_back(cloneNamesMap[j.second] );
				}
			}
		}
		instanceOp_[instanceName] = op;
		instanceActualIO_[instanceName] = actualIOList;
		REPORT(DEBUG, "Finished building instanceActualIO_["<< instanceName<< "], it is of size " << instanceActualIO_[instanceName].size());

		//clear the port mappings
		tmpInPortMap_.clear();
		tmpOutPortMap_.clear();

		return o.str();
	}


	void Operator::newSharedInstance(OperatorPtr op, string instanceName, string inPortMaps, string outPortMaps, string inPortMapsCst){
		schedule(); // Schedule the parent operator, so the subcomponent may get timing information about its inputs.
		//parse the input port mappings
		parsePortMappings(inPortMaps, 0);
		//parse the constant input port mappings, if there are any
		parsePortMappings(inPortMapsCst, 1);
		//parse the input port mappings
		parsePortMappings(outPortMaps, 2);
		//create the instance
		vhdl << this->instance(op, instanceName, false);
	}

	OperatorPtr Operator::newInstance(string opName, string instanceName, string parameters, string inPortMaps, string outPortMaps, string inPortMapsCst)
	{
		OperatorFactoryPtr instanceOpFactory = UserInterface::getFactoryByName(opName);
		OperatorPtr instance = nullptr;
		vector<string> parametersVector;
		string portName, signalName, mapping;

		REPORT(DEBUG, "entering newInstance("<< opName << ", " << instanceName <<")" );

		schedule(); // Schedule the parent operator, so the subcomponent may get timing information about its inputs.

		//parse the parameters
		parametersVector.push_back(opName);
		while(!parameters.empty())
			{
				if(parameters.find(" ") != string::npos){
					parametersVector.push_back(parameters.substr(0,parameters.find(" ")));
					parameters.erase(0,parameters.find(" ")+1);
				}else{
					parametersVector.push_back(parameters);
					parameters.erase();
				}
			}

		//parse the input port mappings
		parsePortMappings(inPortMaps, 0);
		//parse the constant input port mappings, if there are any
		parsePortMappings(inPortMapsCst, 1);
		//parse the input port mappings
		parsePortMappings(outPortMaps, 2);

		REPORT(DEBUG, "   newInstance("<< opName << ", " << instanceName <<"): after parsePortMapping" );
		for (auto i: parametersVector){
			REPORT(DEBUG, i);
		}
		//create the operator
		instance = instanceOpFactory->parseArguments(this, target_, parametersVector);

		REPORT(DEBUG, "   newInstance("<< opName << ", " << instanceName <<"): after factory call" );

		//create the instance
		vhdl << this->instance(instance, instanceName, false);
		// false means: no warning. Eventually the code of instance() should be inlined here, this is a transitionnal measure to support legacy constructor code
		REPORT(DEBUG, "   newInstance("<< opName << ", " << instanceName <<"): after instance()" );

		return instance;
	}


	void Operator::parsePortMappings(string portMappings0, int portTypes)
	{
		string portMappings="";
		// First remove any space
		for (unsigned int i=0; i<portMappings0.size(); i++) {
			if((portMappings0[i] != ' ') && (portMappings0[i] != '\t')) {
				portMappings += portMappings0[i];
			}
		}
		if(portMappings0!="") {
			// First tokenize using stack overflow code
			std::vector<std::string> tokens;
			std::size_t start = 0, end = 0;
			while ((end = portMappings.find(',', start)) != std::string::npos) {
				tokens.push_back(portMappings.substr(start, end - start));
				start = end + 1;
			}
			tokens.push_back(portMappings.substr(start));
			// now iterate over the found token, each of which should be a port map of syntax "X=>Y"
			for(auto& mapping: tokens) {
				size_t sepPos = mapping.find("=>");
				if(sepPos == string::npos)
					THROWERROR("In newInstance: these port maps are not specified correctly: <" << portMappings<<">");
				string portName = mapping.substr(0, sepPos);
				string signalName = mapping.substr(sepPos+2, mapping.size()-sepPos-2);
				REPORT(4, "port map " << portName << "=>" << signalName << " of type " << portTypes);
				if(portTypes == 0)
					inPortMap(portName, signalName);
				else if(portTypes == 1)
					inPortMapCst(portName, signalName);
				else
					outPortMap(portName, signalName);
			}
		}
	}



	string Operator::buildVHDLSignalDeclarations() {
		ostringstream o;

		for(unsigned int i=0; i<signalList_.size(); i++) {
			//constant signals don't need a declaration
			//inputs/outputs treated separately
			if((signalList_[i]->type() == Signal::constant) ||
				 (signalList_[i]->type() == Signal::in) || (signalList_[i]->type() == Signal::out))
				continue;

			o << signalList_[i]->toVHDLDeclaration() << endl;
		}
		//now the signals from the I/O List which have the cycle>0
		for(unsigned int i=0; i<ioList_.size(); i++) {
			if(ioList_[i]->getLifeSpan()>0){
				o << ioList_[i]->toVHDLDeclaration() << endl;
			}

		}

		return o.str();
	}


	// Beware, this can only be called for a subcomponent...
	void Operator::useHardRAM(Operator* t) {
		if (target_->getVendor() == "Xilinx")
			{
				addAttribute("rom_extract", "string", t->getName()+": component", "yes");
				addAttribute("rom_style", "string", t->getName()+": component", "block");
			}
		if (target_->getVendor() == "Altera")
			addAttribute("altera_attribute", "string", t->getName()+": component", "-name ALLOW_ANY_ROM_SIZE_FOR_RECOGNITION ON");
	}

	void Operator::useSoftRAM(Operator* t) {
		if (target_->getVendor() == "Xilinx")
			{
				addAttribute("rom_extract", "string", t->getName()+": component", "yes");
				addAttribute("rom_style", "string", t->getName()+": component", "distributed");
			}
		if (target_->getVendor() == "Altera")
			addAttribute("altera_attribute", "string", t->getName()+": component", "-name ALLOW_ANY_ROM_SIZE_FOR_RECOGNITION OFF");
	}


	void Operator::setArchitectureName(string architectureName) {
		architectureName_ = architectureName;
	};


	void Operator::newArchitecture(std::ostream& o, std::string name){
		o << "architecture " << architectureName_ << " of " << name  << " is" << endl;
	}


	void Operator::beginArchitecture(std::ostream& o){
		o << "begin" << endl;
	}


	void Operator::endArchitecture(std::ostream& o){
		o << "end architecture;" << endl << endl;
	}


	string Operator::buildVHDLComponentDeclarations() {
		ostringstream o;

		for(unsigned int i=0; i<subComponentList_.size(); i++)
			{
				//if this is a global operator, then it should be output only once,
				//	and with the name of the global copy, not that of the local copy
				string componentName = subComponentList_[i]->getName();

				if(componentName.find("_copy_") != string::npos)
					{
						//a global component; only the first copy should be output
						bool isComponentOutput = false;

						componentName = componentName.substr(0, componentName.find("_copy_"));

						for(unsigned int j=0; j<i; j++)
							if(subComponentList_[j]->getName().find(componentName) != string::npos)
								{
									isComponentOutput = true;
									break;
								}
						if(isComponentOutput)
							continue;
						else
							{
								subComponentList_[i]->outputVHDLComponent(o, componentName);
								o << endl;
							}
					}else
					{
						//just a regular subcomponent
						subComponentList_[i]->outputVHDLComponent(o, componentName);
						o << endl;
					}
			}

		return o.str();
	}


	void Operator::addConstant(std::string name, std::string t, mpz_class v) {
		ostringstream tmp;
		tmp << v;
		constants_[name] =  make_pair(t, tmp.str());
	}

	void Operator::addConstant(std::string name, std::string t, int v) {
		ostringstream tmp;
		tmp << v;
		constants_[name] =  make_pair(t, tmp.str());
	}

	void Operator::addConstant(std::string name, std::string t, string v) {
		constants_[name] =  make_pair(t, v);
	}

	void Operator::addType(std::string name, std::string value) {
		types_ [name] =  value;
	}


	void Operator::addAttribute(std::string attributeName,  std::string attributeType,  std::string object, std::string value, bool addSignal ) {
		// TODO add some checks ?
		attributes_[attributeName] = attributeType;
		pair<string,string> p = make_pair(attributeName,object);
		attributesValues_[p] = value;
		attributesAddSignal_[attributeName] = addSignal;
	}


	string Operator::buildVHDLTypeDeclarations() {
		ostringstream o;
		string name, value;
		for(map<string, string >::iterator it = types_.begin(); it !=types_.end(); it++) {
			name  = it->first;
			value = it->second;
			o <<  "type " << name << " is "  << value << ";" << endl;
		}
		return o.str();
	}


	string Operator::buildVHDLConstantDeclarations() {
		ostringstream o;
		string name, type, value;
		for(map<string, pair<string, string> >::iterator it = constants_.begin(); it !=constants_.end(); it++) {
			name  = it->first;
			type = it->second.first;
			value = it->second.second;
			o <<  "constant " << name << ": " << type << " := " << value << ";" << endl;
		}
		return o.str();
	}



	string Operator::buildVHDLAttributes() {
		ostringstream o;
		// First add the declarations of attribute names
		for(map<string, string>::iterator it = attributes_.begin(); it !=attributes_.end(); it++) {
			string name  = it->first;
			string type = it->second;
			o <<  "attribute " << name << ": " << type << ";" << endl;
		}
		// Then add the declarations of attribute values
		for(map<pair<string, string>, string>::iterator it = attributesValues_.begin(); it !=attributesValues_.end(); it++) {
			string name  = it->first.first;
			string object = it->first.second;
			string value = it->second;
			if(attributes_[name]=="string")
				value = '"' + value + '"';
			o <<  "attribute " << name << " of " << object << (attributesAddSignal_[name]?" : signal " : "") << " is " << value << ";" << endl;
		}
		return o.str();
	}




	string  Operator::buildVHDLRegisters() {
		ostringstream o;

		// execute only if the operator is sequential, otherwise output nothing
		string recTab = "";
		if (hasClockEnable())
			recTab = tab;
		if (isSequential()){
			// first concatenate SignalList and ioList
			vector<Signal*> siglist;
			siglist.insert( siglist.end(), signalList_.begin(), signalList_.end() );
			siglist.insert( siglist.end(), ioList_.begin(), ioList_.end() );

			// look up for delayed signals of various types, and build intermediate VHDL if needed
			ostringstream regs, aregs, aregsinit, sregs, sregsinit;
			for(auto s: siglist) {
				if(s->getLifeSpan() > 0) { // This catches all the registered signals
					for(int j=1; j <= s->getLifeSpan(); j++) {
						if (s->resetType() == Signal::noReset) {
							regs << recTab << tab << tab <<tab << tab << s->delayedName(j) << " <=  " << s->delayedName(j-1) <<";" << endl;
						}
						if (s->resetType() == Signal::asyncReset) {
							if ( (s->width()>1) || (s->isBus()))
								aregsinit << recTab << tab << tab << tab << tab  << s->delayedName(j) << " <=  (others => '0');" << endl;
							else
								aregsinit << recTab << tab <<tab << tab << tab   << s->delayedName(j) << " <=  '0';" << endl;
							aregs << recTab << tab << tab << tab << tab        << s->delayedName(j) << " <=  " << s->delayedName(j-1) <<";" << endl;
						}
						if (s->resetType() == Signal::syncReset) {
							if ( (s->width()>1) || (s->isBus()))
								sregsinit << recTab << tab << tab << tab << tab  << s->delayedName(j) << " <=  (others => '0');" << endl;
							else
								sregsinit << recTab << tab <<tab << tab << tab   << s->delayedName(j) << " <=  '0';" << endl;
							sregs << recTab << tab << tab << tab << tab        << s->delayedName(j) << " <=  " << s->delayedName(j-1) <<";" << endl;
						}
					}
				}
			}

			// Now output the actual VHDL.
			// First registers without reset
			if (regs.str() != "") {
				o << tab << "process(clk)" << endl;
				o << tab << tab << "begin" << endl;
				o << tab << tab << tab << "if clk'event and clk = '1' then" << endl;
				if (hasClockEnable())
					o << tab << tab << tab << tab << "if ce = '1' then" << endl;
				o << regs.str();
				if (hasClockEnable())
					o << tab << tab << tab << tab << "end if;" << endl;
				o << tab << tab << tab << "end if;\n";
				o << tab << tab << "end process;\n";
			}

			// then registers with asynchronous reset
			if (aregsinit.str() !="") {
				o << tab << "process(clk, rst)" << endl;
				o << tab << tab << "begin" << endl;
				o << tab << tab << tab << "if rst = '1' then" << endl;
				o << aregsinit.str();
				o << tab << tab << tab << "elsif clk'event and clk = '1' then" << endl;
			  if (hasClockEnable()) o << tab << tab << tab << tab << "if ce = '1' then" << endl;
				o << aregs.str();
				if (hasClockEnable())	o << tab << tab << tab << tab << "end if;" << endl;
				o << tab << tab << tab << "end if;" << endl;
				o << tab << tab <<"end process;" << endl;
			}

			// then registers with synchronous reset
			if (sregsinit.str() !="") {
				o << tab << "process(clk, rst)" << endl;
				o << tab << tab << "begin" << endl;
				o << tab << tab << tab << "if clk'event and clk = '1' then" << endl;
				o << tab << tab << tab << tab << "if rst = '1' then" << endl;
				o << sregsinit.str();
				o << tab << tab << tab << tab << "else" << endl;
				if (hasClockEnable()) o << tab << tab << tab << tab << "if ce = '1' then" << endl;
				o << sregs.str();
				if (hasClockEnable())	o << tab << tab << tab << tab << "end if;" << endl;
				o << tab << tab << tab << tab << "end if;" << endl;
				o << tab << tab << tab << "end if;" << endl;
				o << tab << tab << "end process;" << endl;
			}
		}
		return o.str();
	}


	void Operator::signalSignature(std::ostream &o)
	{
		stringstream inlist;
		stringstream outlist;

		for (auto i: ioList_) {
			stringstream & curlist = (i->type() == Signal::in) ? inlist : outlist;
			curlist << " " << i->getName();
		}

		o << "-- Input signals:" << inlist.str() << endl;
		o << "-- Output signals:" << outlist.str() << endl;
	}

	void Operator::buildStandardTestCases(TestCaseList* tcl) {
		// Each operator should overload this method. If not, it is mostly harmless but deserves a warning.
		cerr << "WARNING: No standard test cases implemented for this operator" << endl;
	}




	void Operator::buildRandomTestCaseList(TestCaseList* tcl, int n){
		TestCase *tc;
		// Generate test cases using random input numbers
		for (int i = 0; i < n; i++) {
			// TODO free all this memory when exiting TestBench
			tc = buildRandomTestCase(i);
			tcl->add(tc);
		}
	}

	TestCase* Operator::buildRandomTestCase(int i){
		TestCase *tc = new TestCase(this);
		// Generate test cases using random input numbers */
		// TODO free all this memory when exiting TestBench
		// Fill inputs
		for (unsigned int j = 0; j < ioList_.size(); j++) {
			Signal* s = ioList_[j];
			if(s->type() == Signal::in){
				mpz_class a = getLargeRandom(s->width());
				tc->addInput(s->getName(), a);
			}
		}
		// Get correct outputs
		emulate(tc);

		// add to the test case list
		return tc;
	}

	Target* Operator::getTarget(){
		return target_;
	}

	OperatorPtr Operator::getParentOp(){
		return parentOp_;
	}

	string Operator::getUniqueName(){
		return uniqueName_;
	}

	string Operator::getArchitectureName(){
		return architectureName_;
	}

	vector<Signal*> Operator::getTestCaseSignals(){
		return testCaseSignals_;
	}


	string Operator::getSrcFileName(){
		return srcFileName;
	}

	int Operator::getOperatorCost(){
		return cost;
	}

	map<string, Signal*> Operator::getSignalMap(){
		return signalMap_;
	}

	map<string, pair<string, string> > Operator::getConstants(){
		return constants_;
	}

	map<string, string> Operator::getAttributes(){
		return attributes_;
	}

	map<string, string> Operator::getTypes(){
		return types_;
	}

	map<pair<string,string>, string> Operator::getAttributesValues(){
		return attributesValues_;
	}

	// we used to attempt to maintain an attribute for the following but it is really simpler to test it when needed
	bool Operator::hasReset() {
		for(auto s: signalList_) {
			if ( s->resetType() != Signal::noReset )
				return true;
		}
		for(auto s: ioList_) {
			if ( s->resetType() != Signal::noReset )
				return true;
		}
		for(auto op: getSubComponentList()) {
			if ( op->hasReset() )
				return true;
		}
		return false;
	}

	bool Operator::hasClockEnable(){
		return hasClockEnable_;
	}

	void Operator::setClockEnable(bool val){
		hasClockEnable_=val;
	}

	string Operator::getCopyrightString(){
		return copyrightString_;
	}

	Operator* Operator::getIndirectOperator(){
		return indirectOperator_;
	}

	vector<Operator*> Operator::getSubComponentList(){
		return subComponentList_;
	}

	vector<Operator*>& Operator::getSubComponentListR(){
		return subComponentList_;
	}

	FlopocoStream* Operator::getFlopocoVHDLStream(){
		return &vhdl;
	}

	void Operator::outputVHDL(std::ostream& o, std::string name) {

		//safety checks
		//	if this is a copy of a global operator, then its code doesn't need to
		//	be added to the file, as the global copy is already there
		if(isShared()
			 && (getName().find("_copy_") != string::npos))
			{
				return;
			}

		if (! vhdl.isEmpty() ){
			licence(o);
			pipelineInfo(o);
			signalSignature(o);
			o << endl;
			additionalHeader(o);
			stdLibs(o);
			outputVHDLEntity(o);
			newArchitecture(o,name);
			o << buildVHDLComponentDeclarations();
			o << buildVHDLAttributes();
			o << buildVHDLTypeDeclarations();
			o << buildVHDLSignalDeclarations();			//TODO: this cannot be called before scheduling the signals (it requires the lifespan of the signals, which is not yet computed)
			o << buildVHDLConstantDeclarations();
			beginArchitecture(o);
			o << buildVHDLRegisters();					//TODO: this cannot be called before scheduling the signals (it requires the lifespan of the signals, which is not yet computed)
			if(getIndirectOperator())
				o << getIndirectOperator()->vhdl.str();
			else
				o << vhdl.str();
			endArchitecture(o);
		}
	}




	// Comment by F2D: this whas parse2().
	// This code is suspected to be OK except for the handling of functional delays
	// It could probably be simplified: it is a lexer
	void Operator::doApplySchedule()
	{
		ostringstream newStr;
		string oldStr, workStr;
		size_t currentPos, nextPos, tmpCurrentPos, tmpNextPos;
		int count, lhsNameLength, rhsNameLength;
		bool unknownLHSName = false, unknownRHSName = false;

		REPORT(DEBUG, "doApplySchedule(): entering operator " << getName());
		REPORT(FULL, "doApplySchedule: vhdl stream after first lexing " << endl << vhdl.str());
		//reset the new vhdl code buffer
		newStr.str("");

		//set the old code to the vhdl code stored in the FlopocoStream
		oldStr = vhdl.str();

		//iterate through the old code, one statement at the time
		// code that doesn't need to be modified: goes directly to the new vhdl code buffer
		// code that needs to be modified: ?? should be removed from lhs_name, $$ should be removed from rhs_name,
		//		delays of the type rhs_name_dxxx should be added for the right-hand side signals
		bool isSelectedAssignment = (oldStr.find('?') > oldStr.find('$'));
		currentPos = 0;
		nextPos = (isSelectedAssignment ? oldStr.find('$') : oldStr.find('?'));
		while(nextPos !=  string::npos)
			{
				string lhsName, rhsName;
				size_t auxPosition, auxPosition2;
				Signal *lhsSignal, *rhsSignal;

				unknownLHSName = false;
				unknownRHSName = false;

				//copy the code from the beginning to this position directly to the new vhdl buffer
				newStr << oldStr.substr(currentPos, nextPos-currentPos);

				//now get a new line to parse
				workStr = oldStr.substr(nextPos+2, oldStr.find(';', nextPos)+1-nextPos-2);

				REPORT(FULL, "doApplySchedule: processing " << workStr);
				//extract the lhs_name
				if(isSelectedAssignment == true)
					{
						int lhsNameStart, lhsNameStop;

						lhsNameStart = workStr.find('?');
						lhsNameStop  = workStr.find('?', lhsNameStart+2);
						lhsName = workStr.substr(lhsNameStart+2, lhsNameStop-lhsNameStart-2);
						auxPosition = lhsNameStop+2;
					}else
					{
						lhsName = workStr.substr(0, workStr.find('?'));

						//copy lhsName to the new vhdl buffer
						newStr << lhsName;

						auxPosition = lhsName.size()+2;
					}
				lhsNameLength = lhsName.size();

				//check for component instances
				//	the first parse marks the name of the component as a lhsName
				//	must remove the markings
				//the rest of the code contains pairs of ??lhsName?? => $$rhsName$$ pairs
				//	for which the helper signals must be removed and delays _dxxx must be added
				auxPosition2 = workStr.find("port map"); // not checking capitalization is OK because this string can only be created by flopoco

				if(auxPosition2 != string::npos)	{
					// we have the position of "port map"; we need the name of the instance name.
					// It is the word before the ":"; all this looks terribly fragile and depends on the fact that this VHDL is created here in flopoco.
					size_t i=nextPos;
					while(oldStr[i]!=':')
						i--;
					size_t endInstanceName = i;
					while(oldStr[i]!=' ')
						i--;
					size_t beginInstanceName = i+1;
					string instanceName = oldStr.substr(beginInstanceName, endInstanceName-beginInstanceName);
					REPORT(FULL,"doApplySchedule found instance name >>>>"<<instanceName<<"<<<<");

					string subOpName=lhsName;
					OperatorPtr subop = getSubComponent(subOpName);
					if(subop==nullptr)
						THROWERROR("doApplySchedule(): " << subOpName << " does not seem to be a subcomponent of " << getName());
					REPORT(DEBUG, "doApplySchedule: found instance: " << subOpName);//workStr.substr(auxPosition, auxPosition2));
					//try to parse the names of the signals in the port mapping
					if(workStr.find("?", auxPosition2) == string::npos) {
						//empty port mapping
						newStr << workStr.substr(auxPosition, workStr.size());
					} else {
						//parse a list of ??lhsName?? => $$rhsName$$
						//	or ??lhsName?? => 'x' or ??lhsName?? => "xxxx"
						size_t tmpCurrentPos, tmpNextPos;

						//copy the code up to the port mappings
						newStr << workStr.substr(auxPosition, workStr.find("?", auxPosition2)-auxPosition);

						tmpCurrentPos = workStr.find("?", auxPosition2);
						while(tmpCurrentPos != string::npos)	{
							bool singleQuoteSep = false, doubleQuoteSep = false, open = false;
							bool rhsIsSignal=false;
							//extract a lhsName
							tmpNextPos = workStr.find("?", tmpCurrentPos+2);
							lhsName = workStr.substr(tmpCurrentPos+2, tmpNextPos-tmpCurrentPos-2);
							REPORT(FULL, "doApplySchedule: instance lhsName=" << lhsName);

							//copy lhsName (the formal input/output) to the new vhdl buffer
							newStr << lhsName;

							//copy the code up to the next rhsName to the new vhdl buffer
							tmpCurrentPos = tmpNextPos+2;
							tmpNextPos = workStr.find("$", tmpCurrentPos);

							//check for constant as rhs name
							if(workStr.find("\'", tmpCurrentPos) < tmpNextPos)	{
								tmpNextPos = workStr.find("\'", tmpCurrentPos);
								singleQuoteSep = true;
							}
							else if(workStr.find("\"", tmpCurrentPos) < tmpNextPos)  {
								tmpNextPos = workStr.find("\"", tmpCurrentPos);
								doubleQuoteSep = true;
							}
							else if(workStr.find("open", tmpCurrentPos) < tmpNextPos)	{
								tmpNextPos = workStr.find("open", tmpCurrentPos);
								open = true;
							}

							REPORT(FULL, "doApplySchedule/instance skipping: " << workStr.substr(tmpCurrentPos, tmpNextPos-tmpCurrentPos));
							newStr << workStr.substr(tmpCurrentPos, tmpNextPos-tmpCurrentPos);

							//extract a rhsName
							//	this might be a constant
							if(singleQuoteSep) {
								//a 1-bit constant
								tmpCurrentPos = tmpNextPos+1;
								tmpNextPos = workStr.find("\'", tmpCurrentPos);
							}
							else if(doubleQuoteSep) {
								//a multiple bit constant
								tmpCurrentPos = tmpNextPos+1;
								tmpNextPos = workStr.find("\"", tmpCurrentPos);
							}
							else if(open) {
								//a open output
								tmpNextPos = workStr.find("open", tmpCurrentPos)-1; //has to be one less as we want the 'o' of "open"
								tmpCurrentPos = tmpNextPos+1;
							}
							else {
								//a regular signal name
								tmpCurrentPos = tmpNextPos+2;
								tmpNextPos = workStr.find("$", tmpCurrentPos);
								rhsIsSignal = true;
							}

							rhsName = workStr.substr(tmpCurrentPos, tmpNextPos-tmpCurrentPos);
							REPORT(FULL, "doApplySchedule/instance rhsname= " << rhsName);

							//						rhsSignal = NULL;
							//						lhsSignal = NULL;

							//copy rhsName to the new vhdl buffer
							if(!open)
								newStr << (singleQuoteSep ? "\'" : doubleQuoteSep ? "\"" : "") << rhsName << (singleQuoteSep ? "\'" : doubleQuoteSep ? "\"" : "");
							//                        else
							//                            newStr << "open";

							// All the inputs should be synchronized.
							// We do this by comparing their cycle to the cycle of the first output of the instance.
							try {
								//	delay it if necessary, i.e. if it is a shared instance with a dependency to a later signal
								if(isSequential()
									 && rhsIsSignal // rhs is not a constant
									 && subop->isShared() // otherwise the dependency graph takes care of all the pipelining
									 && subop->getSignalByName(lhsName)->type() == Signal::in
									 ) {// the lhs is a input of the subcomponent
									// In this case, we have in the dep graph the dependencies (actualIn->actualOut): extract the first one
									Signal* subopInput = getSignalByName(rhsName);
									//look for the first output
									int i=0;
									while((*subop->getIOList())[i]->type() != Signal::out)
										i++;
									vector<string> actualIO = instanceActualIO_[instanceName];
									Signal* subopOutput = getSignalByName(actualIO[i]);

									// was								subopOutput = (*subopInput->successors())[0].first; // but bug if the actual input has other successors
									REPORT(DEBUG, "doApplySchedule: shared instance: " << instanceName << " has input " << subopInput->getName() << " and output " << subopOutput->getName());//workStr.substr(auxPosition, auxPosition2));
									int deltaCycle =subopOutput->getCycle() - subopInput->getCycle();
									if( deltaCycle> 0) {
										newStr << "_d" << vhdlize(deltaCycle);
										subopInput -> updateLifeSpan(deltaCycle);
									}
								}
							}
							catch(string &e) {
								REPORT(FULL, "doApplySchedule caught " << e << " and is ignoring it.");
							}
							catch(char const *e) {
								REPORT(FULL, "doApplySchedule caught " << e << " and is ignoring it.");
							}

							//prepare to parse a new pair
							tmpCurrentPos = workStr.find("?", tmpNextPos+2);

							//copy the rest of the code to the new vhdl buffer
							if(tmpCurrentPos != string::npos)
								newStr << workStr.substr(tmpNextPos+(singleQuoteSep||doubleQuoteSep||open ? 1 : 2),
																				 tmpCurrentPos-tmpNextPos-(singleQuoteSep||doubleQuoteSep||open ? 1 : 2));
							else
								newStr << workStr.substr(tmpNextPos+(singleQuoteSep||doubleQuoteSep||open ? 1 : 2), workStr.size());
						}
					}

					//prepare for a new instruction to be parsed
					currentPos = nextPos + workStr.size() + 2;
					nextPos = oldStr.find('?', currentPos);
					//special case for the selected assignment statements
					isSelectedAssignment = false;
					if(oldStr.find('$', currentPos) < nextPos)
						{
							nextPos = oldStr.find('$', currentPos);
							isSelectedAssignment = true;
						}

					continue;
				}

				//the lhsName could be of the form (lhsName1, lhsName2, ...)
				//	extract the first name in the list
				if(lhsName[0] == '(')
					{
						count = 1;
						while((lhsName[count] != ' ') && (lhsName[count] != '\t')
									&& (lhsName[count] != ',') && (lhsName[count] != ')'))
							count++;
						lhsName = lhsName.substr(1, count-1);
					}

				//this could be a user-defined name
				try			{
			    lhsSignal = getSignalByName(lhsName);
				}
				catch(string &e)	{
			    lhsSignal = NULL;
			    unknownLHSName = true;
				}
				//check for "select" signal assignments
				//	with signal_name select... etc
				//	the problem is there is a signal belonging to the right hand side
				//	on the left-hand side, before the left hand side signal, which breaks the regular flow
				if(isSelectedAssignment == true)		{
					//extract the first rhs signal name
					tmpCurrentPos = 0;
					tmpNextPos = workStr.find('$');

					assert(tmpNextPos != string::npos);

					rhsName = workStr.substr(tmpCurrentPos, tmpNextPos);

					//remove the possible parentheses around the rhsName
					string newRhsName = rhsName;
					if(rhsName.find("(") != string::npos)	{
						newRhsName = newRhsName.substr(rhsName.find("(")+1, rhsName.find(")")-rhsName.find("(")-1);
					}
					// No functional register possible here
					try			{
						rhsSignal = getSignalByName(newRhsName);
					}
					catch(...){
						//this i must be user-defined name
						rhsSignal = NULL;
						unknownRHSName = true;
					}

					//output the rhs signal name
					newStr << rhsName;

					if(isSequential() && !unknownLHSName  && !unknownRHSName) {
						int deltaCycle = lhsSignal->getCycle()-rhsSignal->getCycle();
						if( deltaCycle> 0)
							newStr << "_d" << vhdlize(deltaCycle);
					}
					//copy the code up until the lhs signal name
					tmpCurrentPos = tmpNextPos+2;
					tmpNextPos = workStr.find('?', tmpCurrentPos);

					newStr << workStr.substr(tmpCurrentPos, tmpNextPos-tmpCurrentPos);

					//copy the lhs signal name
					newStr << lhsName;

					//prepare to parse a new rhs name
					tmpCurrentPos = tmpNextPos+4+lhsNameLength;
					tmpNextPos = workStr.find('$', tmpCurrentPos);

					//if there are is more code that needs preparing, then just pass
					//to the next instruction
					if(tmpNextPos == string::npos)
						{
							//copy the rest of the code
							assert(workStr.size()-tmpCurrentPos > 0);
							newStr << workStr.substr(tmpCurrentPos, workStr.size()-tmpCurrentPos);

							//prepare for a new instruction to be parsed
							currentPos = nextPos + workStr.size() + 2;
							nextPos = oldStr.find('?', currentPos);
							//special case for the selected assignment statements
							isSelectedAssignment = false;
							if(oldStr.find('$', currentPos) < nextPos)
								{
									nextPos = oldStr.find('$', currentPos);
									isSelectedAssignment = true;
								}

							continue;
						}
				}
				//extract the rhsNames and annotate them find the position of the rhsName, and copy
				// the vhdl code up to the rhsName in the new vhdl code buffer
				// There was a bug here  if one variable is had select in its name
				// Took me 2hours to figure out
				// Bug fixed by having the lexer add spaces around select (so no need to test for all the space/tab/enter possibilibits.
				// I wonder how many such bugs remain
				if(workStr.find(" select ") == string::npos	 )			{
					tmpCurrentPos = lhsNameLength+2;
					tmpNextPos = workStr.find('$', lhsNameLength+2);
				}
				while(tmpNextPos != string::npos)	{
					unknownRHSName = false;

					//copy into the new vhdl stream the code that doesn't need to be modified
					newStr << workStr.substr(tmpCurrentPos, tmpNextPos-tmpCurrentPos);

					//skip the '$$'
					tmpNextPos += 2;

					//extract a new rhsName
					rhsName = workStr.substr(tmpNextPos, workStr.find('$', tmpNextPos)-tmpNextPos);
					rhsNameLength = rhsName.size();
					//remove the possible parentheses around the rhsName
					string newRhsName = rhsName;
					if(rhsName.find("(") != string::npos)	{
						newRhsName = newRhsName.substr(rhsName.find("(")+1, rhsName.find(")")-rhsName.find("(")-1);
					}
					//this could also be a delayed signal name
					int functionalDelay=0;
					if(newRhsName.find('^') != string::npos){
						string sdelay = newRhsName.substr(newRhsName.find('^') + 1, string::npos-1);
						newRhsName = newRhsName.substr(0, newRhsName.find('^'));
						REPORT(FULL, "doApplySchedule: Found funct. delayed signal  : " << newRhsName << " delay:" << sdelay );
						functionalDelay = stoi(sdelay);
					}

					try			{
						rhsSignal = getSignalByName(newRhsName);
					}
					catch(...){
						//this i must be user-defined name
						rhsSignal = NULL;
						unknownRHSName = true;
					}


					//copy the rhsName with the delay information into the new vhdl buffer
					//	rhsName becomes rhsName_dxxx, if the rhsName signal is declared at a previous cycle
					newStr << newRhsName;
					if(isSequential() && !unknownLHSName && !unknownRHSName) {
						// Should we insert a pipeline register ?
						int deltaCycle = lhsSignal->getCycle() - rhsSignal->getCycle();
						if(deltaCycle>0)
							newStr << "_d" << vhdlize(deltaCycle);

						// Should we insert a functional register ? This case is exclusive with the previous as long as functional delays are introduced only by the functionalRegister method.
						if(functionalDelay>0) {
							getSignalByName(newRhsName) -> updateLifeSpan(functionalDelay); // wonder where it is done for pipeline registers???
							newStr << "_d" << vhdlize(functionalDelay);
						}
					}

					//find the next rhsName, if there is one
					tmpCurrentPos = tmpNextPos + rhsNameLength + 2;
					tmpNextPos = workStr.find('$', tmpCurrentPos);
				}

				//copy the code that is left up until the end of the line in
				//	the new vhdl code buffer, without changing it
				newStr << workStr.substr(tmpCurrentPos, workStr.size()-tmpCurrentPos);

				//get a new line to parse
				currentPos = nextPos + workStr.size() + 2;
				nextPos = oldStr.find('?', currentPos);
				//special case for the selected assignment statements
				isSelectedAssignment = false;
				if(oldStr.find('$', currentPos) < nextPos)
					{
						nextPos = oldStr.find('$', currentPos);
						isSelectedAssignment = true;
					}
			}

		//the remaining code might contain identifiers that were marked
		//	with ??; these must also be removed
		//if(nextPos == string::npos) && (oldStr.find )

		//copy the remaining code to the vhdl code buffer
		newStr << oldStr.substr(currentPos, oldStr.size()-currentPos);

		vhdl.setSecondLevelCode(newStr.str());

		REPORT(DEBUG, "doApplySchedule: finished " << getName());
	}










	int Operator::getPipelineDelay(Signal *rhsSignal, Signal *lhsSignal)
	{
		bool isLhsPredecessor = false;

		for(size_t i=0; i<lhsSignal->predecessors()->size(); i++)
			{
				pair<Signal*, int> newPair = *lhsSignal->predecessorPair(i);

				if(newPair.first->getName() == rhsSignal->getName())
					{
						isLhsPredecessor = true;

						if(newPair.second > 0)
							return newPair.second;
						else
							return 0;
					}
			}

		if(isLhsPredecessor == false)
			THROWERROR("In getPipelineDelay: trying to obtain the pipeline delay between signal "
								 << rhsSignal->getName() << " and signal " << lhsSignal->getName() << " which are not directly connected");
		return 0;
	}


	int Operator::getDelay(Signal *rhsSignal, Signal *lhsSignal)
	{
		bool isLhsPredecessor = false;

		for(size_t i=0; i<lhsSignal->predecessors()->size(); i++)
			{
				pair<Signal*, int> newPair = *lhsSignal->predecessorPair(i);

				if(newPair.first->getName() == rhsSignal->getName())
					{
						isLhsPredecessor = true;

						if(newPair.second < 0)
							return (-1)*newPair.second;
						else
							return newPair.second;
					}
			}

		if(isLhsPredecessor == false)
			THROWERROR("In getDelay: trying to obtain the delay between signal "
								 << rhsSignal->getName() << " and signal " << lhsSignal->getName() << " which are not directly connected");
		return 0;
	}


	// this is called by schedule() to transform the (string, string, int) dependencies produced by the lexer at each ;
	// into signal dependencies in the graph.
	void Operator::moveDependenciesToSignalGraph()
	{
		//try to parse the unknown dependences first (we have identified a dependency A->B but A or B has not yet been declared)
		// unresolvedDependenceTable is a global variable that holds this information
		vector<triplet<string, string, int>> newURDTable;
		for(auto it: unresolvedDependenceTable)
			{
				Signal *lhs, *rhs;
				int delay;
				bool unknownLHSName = false, unknownRHSName = false;

				try{
					lhs = getSignalByName(it.first); // Was this signal declared since last time?
				}catch(string &e){
					// REPORT(DEBUG, "Warning: signal name on the left-hand side of an assignment still unknown: " << it.first);
					unknownLHSName = true;
				}

				try{
					rhs = getSignalByName(it.second); // or this one
				}catch(string &e){
					// REPORT(DEBUG, "Warning: signal name on the right-hand side of an assignment still unknown: " << it.second);
					unknownRHSName = true;
				}

				delay = it.third;

				// if both sides are now known, add the dependences to the signal graph:
				//		erase the entry from unresolvedDependenceTable
				//		add the signals to the list of signals to be scheduled
				if(!unknownLHSName && !unknownRHSName)	{
						//add the dependences
						lhs->addPredecessor(rhs, delay);
						rhs->addSuccessor(lhs, delay);
					}
				else { // keep it for next time
					newURDTable.push_back(it);
				}
			}
		unresolvedDependenceTable = newURDTable;
		// Now go through the dependence table built by the vhdl lexer, transfering the corresponding information into the Signal graph.
		// dependenceTable is updated by the lexer between two VHDL statements / semicolons
		for(vector<triplet<string, string, int>>::iterator it=vhdl.dependenceTable.begin(); it!=vhdl.dependenceTable.end(); it++)
			{
				Signal *lhs, *rhs;
				int delay;
				bool unknownLHSName = false, unknownRHSName = false;

				try{
			    lhs = getSignalByName(it->first);
				}catch(string &e){
					if (allSignalsLowercased.find(toLower(it->first)) != allSignalsLowercased.end()) {
						THROWERROR("Signal " << it->first << " undeclared, but a signal that differs only by capitalization has been declared" << endl
											 << "Please fix it, as it will crash the scheduler: FloPoCo, contrary to VHDL, is case-sensitive");
					}
					else{
						REPORT(DEBUG, "Warning: LHS signal name: " << it->first << " unknown so far" );
						unknownLHSName = true;
					}
				}

				try{
			    rhs = getSignalByName(it->second);
				}catch(string &e){
					if (allSignalsLowercased.find(toLower(it->second)) != allSignalsLowercased.end() ) {
						THROWERROR("Signal" << it->second << " undeclared, but a signal that differs only by capitalization has been declared");
					}
					else{
						REPORT(DEBUG, endl << "Warning: RHS signal name: " << it->second << " unknown so far"  << endl);
						unknownRHSName = true;
					}
				}

				delay = it->third;

				// If both signals are known, we may move this dependency to the Signal graph.
				//	if not, add a new entry to unknownDependenceTable, the list of unknown dependences
				if(!unknownLHSName && !unknownRHSName)
					{
						lhs->addPredecessor(rhs, delay);
						rhs->addSuccessor(lhs, delay);
					}else{
					triplet<string, string, int> newDep = make_triplet(it->first, it->second, it->third);
					unresolvedDependenceTable.push_back(newDep);
				}
			}

		//clear the current partial dependence table
		vhdl.dependenceTable.clear();

		//start the parsing of the dependence table for the subcomponents
		for(unsigned int i=0; i<subComponentList_.size(); i++)
			{
				subComponentList_[i]->moveDependenciesToSignalGraph();
			}

	}

	void  Operator::buildAlreadyScheduledList(set<Signal*> & alreadyScheduled) {
		for(auto i: signalList_)	{
			if (i->predecessors()->size()==0) {
				i->setHasBeenScheduled(true);
			}
			if (i->hasBeenScheduled()) {  // this captures the previous constant signals but also the functional register outputs
				alreadyScheduled.insert(i);
			}

		}
		// and do the same recursively for all subcomponents
		for(auto op: subComponentList_)	{
			op->buildAlreadyScheduledList(alreadyScheduled);
		}
	}


	void Operator::schedule()
	{
		REPORT(DEBUG, "Entering schedule() of operator " << getName() << " with isOperatorScheduled_="<< isOperatorScheduled_);
		if(noParseNoSchedule_ || isOperatorScheduled_) // for TestBench and Wrapper
			return;

		// move the dependences extracted by the lexer to the operator's internal signals
		moveDependenciesToSignalGraph();


		// schedule from the root parent op
		if(parentOp_ != nullptr && !isShared()) {
			REPORT(DEBUG, "schedule(): Not the root Operator, moving up to " << parentOp_->getName());
			parentOp_ ->schedule();
		}
		else { // We are the root parent op
			REPORT(DEBUG, "schedule(): It seems I am a root Operator, starting scheduling");


			// Algorithm initialization
			// We need a global list of all the signals of the graph
			size_t numberScheduled;
			set<Signal*> successorsFront;
			// restate that inputs  are already scheduled for good measure (recall that we are in the top level)
			for(auto i: ioList_)	{
				if (i->type()==Signal::in) {
					alreadyScheduled.insert(i); // TODO refactor into one call setScheduled(Signal*) ?
					i->setHasBeenScheduled(true);
				}
			}
			// recursively run through subcomponents looking for already scheduled signals such as constants signals, functional register outputs, etc
			buildAlreadyScheduledList(alreadyScheduled);

			// then build the current wavefront (new successors may have been added to already scheduled signals)
			for(auto i: alreadyScheduled)	{
				for(auto successor : *i->successors()) {
					if(successor.first->hasBeenScheduled()==false)
						successorsFront.insert(successor.first);
				}
			}

			numberScheduled=0;
			// The main loop only tries to schedule signals in the wavefront
			while(alreadyScheduled.size() > numberScheduled) {
				// Things are still moving
				numberScheduled = alreadyScheduled.size();
				set<Signal*> nextIterationFront;

				// REPORTING
				{
					ostringstream s1,s2;
					for(auto i: successorsFront)
						s1 << i->getUniqueName() << " ";
					REPORT(DEBUG, "schedule(): current wavefront is " << s1.str());
					for(auto i: alreadyScheduled)
						s2 << i->getUniqueName() << " ";
					REPORT(DEBUG, "schedule(): currently scheduled: " << s2.str());
				}

				for(auto candidate: successorsFront) {
					// Can we schedule this candidate?
					//check if all the signal's predecessors have been scheduled
					bool allPredecessorsScheduled = true;
					for(auto i : *candidate->predecessors()) {
						Signal* pred=i.first;
						if(pred->hasBeenScheduled() == false) {
							REPORT(DEBUG, "schedule():   " << candidate->getUniqueName() << " cannot be scheduled because of predecessor " << pred->getUniqueName());
							allPredecessorsScheduled = false;
						}
					}
					if(allPredecessorsScheduled) {
						setSignalTiming(candidate); // also marks it as scheduled
						alreadyScheduled.insert(candidate);
						REPORT(DEBUG, "schedule(): :) " << candidate->getUniqueName()
									 << " has been scheduled at lexicographic time (" << candidate->getCycle() << ", " << candidate->getCriticalPath() <<")"  );

						nextIterationFront.erase(candidate);// it may have been already added to nextIterationFront in this iteration

						for(auto i : *candidate->successors()) {
							if(i.first->hasBeenScheduled() == false) {
								REPORT(DEBUG, "schedule():     " << i.first->getUniqueName() << " added to the wavefront");
								nextIterationFront.insert(i.first);
							}
						}
					}
					else { // not all the predecessors scheduled, keep this signal for next iteration
						nextIterationFront.insert(candidate);
					}
				}
				// copy the new wavefront
				successorsFront = nextIterationFront;
			} // end main while loop

			set<string> unscheduledOutputs;
			for(auto i: ioList_)	{
				if (i->type()==Signal::out) {
					if(!i->hasBeenScheduled())
						unscheduledOutputs.insert(i->getName());
				}
			}
#if 0 // The following assumes that outputs are declared at the beginning of operator constructor
			if(unscheduledOutputs.size()==0) {
				// Success! All outputs are scheduled
				isOperatorScheduled_=true;
				computePipelineDepths();	 // also for the subcomponents
			}
			else{ // Some outputs unscheduled, report them
				ostringstream unscheduled;
				for (auto i: unscheduledOutputs)
					unscheduled << "  " << i;
				REPORT(DEBUG, "schedule(): Warning: the following outputs were NOT scheduled: " << unscheduled.str());
			}
#endif
			ostringstream unscheduled;
			for (auto i: unscheduledOutputs)
				unscheduled << "  " << i;
			REPORT(DEBUG, "exiting schedule(), currently unscheduled outputs: " << unscheduled.str());
		}
	}









	// This is NOT a recursive function
	void Operator::setSignalTiming(Signal* targetSignal)
	{
		int maxCycle = 0;
		double maxCriticalPath = 0.0, maxTargetCriticalPath;

		//check if the signal has already been scheduled
		if(targetSignal->hasBeenScheduled())
			//there is nothing else to be done
			return;

		//initialize the maximum cycle and critical path of the predecessors
		if(targetSignal->predecessors()->size() != 0)
			{
				//if the delay is negative, it means this is a functional delay,
				//	so we can ignore it for the pipeline computations
				maxCycle = targetSignal->predecessor(0)->getCycle() + max(0, targetSignal->predecessorPair(0)->second);
				maxCriticalPath = targetSignal->predecessor(0)->getCriticalPath();
			}

		//determine the lexicographic maximum cycle and critical path of the signal's parents
		for(auto i : *targetSignal->predecessors())
			{
				Signal* currentPred = i.first;
				//if the delay is negative, it means this is a functional delay,
				//	so we can ignore it for the pipeline computations
				int currentPredCycleDelay = max(0, i.second);

				//constant signals are not taken into account
				if((currentPred->type() == Signal::constant) || (currentPred->type() == Signal::constantWithDeclaration))
					continue;

				//check if the predecessor is at a later cycle
				if(currentPred->getCycle()+currentPredCycleDelay >= maxCycle)
					{
						//differentiate between delayed and non-delayed signals
						if((currentPred->getCycle()+currentPredCycleDelay > maxCycle) && (currentPredCycleDelay > 0))
							{
								//if the maximum cycle is at a delayed signal, then
								//	the critical path must also be reset
								maxCycle = currentPred->getCycle()+currentPredCycleDelay;
								maxCriticalPath = 0;
							}else
							{
								if(currentPred->getCycle() > maxCycle)
									{
										maxCycle = currentPred->getCycle();
										maxCriticalPath = currentPred->getCriticalPath();
									}else if((currentPred->getCycle() == maxCycle) && (currentPred->getCriticalPath() > maxCriticalPath))
									{
										//the maximum cycle and critical path come from a
										//	predecessor, on a link without delay
										maxCycle = currentPred->getCycle();
										maxCriticalPath = currentPred->getCriticalPath();
									}
							}
					}
			}

		//compute the cycle and the critical path for the node itself from
		//	the maximum cycle and critical path of the predecessors
		maxTargetCriticalPath = 1.0 / getTarget()->frequency() - getTarget()->ffDelay();
		//check if the signal needs to pass to the next cycle, due to its critical path contribution
		// except for shared operators which shouldn't advance the cycle
		if(!isShared() &&  maxCriticalPath + targetSignal->getCriticalPathContribution() > maxTargetCriticalPath)
			{
				double totalDelay = maxCriticalPath + targetSignal->getCriticalPathContribution();

				while(totalDelay > maxTargetCriticalPath)
					{
						// if maxCriticalPath+criticalPathContribution > 1/frequency, it may insert several pipeline levels.
						// This is what we want to pipeline block-RAMs and DSPs up to the nominal frequency by just passing their overall delay.
						maxCycle++;
						totalDelay -= maxTargetCriticalPath;
					}

				if(totalDelay < 0)
					totalDelay = 0.0;
				targetSignal->setCycle(maxCycle);

#define ASSUME_RETIMING 1 // we leave a bit of the critical path to this signal
#if ASSUME_RETIMING
				targetSignal->setCriticalPath(totalDelay);
#else //ASSUME_NO_RETIMING
				targetSignal->setCriticalPath(targetSignal->getCriticalPathContribution());
#endif

			}else
			{
				targetSignal->setCycle(maxCycle);
				targetSignal->setCriticalPath( maxCriticalPath + targetSignal->getCriticalPathContribution());
			}

		//update the lifespan of inputSignal's predecessors
		for(auto i : *targetSignal->predecessors())
			{
				//predecessor signals that belong to a subcomponent do not need to have their lifespan affected
				if((targetSignal->parentOp()->getName() != i.first->parentOp()->getName()) &&
					 (i.first->type() == Signal::out))
					continue;
				i.first->updateLifeSpan(targetSignal->getCycle() - i.first->getCycle());
			}

		targetSignal->setHasBeenScheduled(true);
	}




	void Operator::computePipelineDepths()
	{
		// first compute it for the subcomponents
		for(auto i: subComponentList_) {
			i-> computePipelineDepths();
		}

		ostringstream in, out;
		for(auto i: ioList_) {
			if((i->type() == Signal::in))	{
				in << i->getName() << "@" << i->getCycle() << " ";
			}
			if((i->type() == Signal::out)) {
				out << i->getName() << "@" << i->getCycle() << " ";
			}
		}
		REPORT(DETAILED, "Input timing:  " << in.str());
		REPORT(DETAILED, "Output timing: " << out.str());

		int maxInputCycle  = -1;
		int maxOutputCycle = -1;

		for(auto i: ioList_) {
			if((i->type() == Signal::in) && (i->getCycle() > maxInputCycle))	{
				maxInputCycle = i->getCycle();
				continue;
			}
			if((i->type() == Signal::out) && (i->getCycle() > maxOutputCycle)) {
				maxOutputCycle = i->getCycle();
				continue;
			}
		}

		for(auto i:ioList_)	{
			if((i->type() == Signal::out) && (i->getCycle() != maxOutputCycle))
				REPORT(DEBUG, "A warining from computePipelineDepths(): this operator's outputs are not synchronized!");
		}

		pipelineDepth_ = maxOutputCycle-maxInputCycle;
	}

	void Operator::outputFinalReport(ostream& s, int level) {

		if (getIndirectOperator()!=NULL){
			// interface operator
			if(getSubComponentList().size()!=1){
				ostringstream o;
				o << "!?! Operator " << getUniqueName() << " is an interface operator with " << getSubComponentList().size() << "children";
				throw o.str();
			}
			getSubComponentList()[0]->outputFinalReport(s, level);
		}else
			{
				// Hard operator
				if (! getSubComponentList().empty())
					for (auto i: getSubComponentList())
						i->outputFinalReport(s, level+1);

				ostringstream tabs, ctabs;
				for (int i=0;i<level-1;i++){
					tabs << "|" << tab;
					ctabs << "|" << tab;
				}

				if (level>0){
					tabs << "|" << "---";
					ctabs << "|" << tab;
				}

				s << tabs.str() << "Entity " << uniqueName_ << endl;
				if(this->getPipelineDepth()!=0)
					s << ctabs.str() << tab << "Pipeline depth = " << getPipelineDepth() << endl;
				else
					s << ctabs.str() << tab << "Not pipelined"<< endl;
			}
	}






	void Operator::drawDotDiagram(ofstream& file, int mode, std::string dotDrawingMode, std::string tabs)
	{
		bool mustDrawCompact = true;

		//check if this operator has already been drawn
		if(mode == 1)
			{
				//for global operators, which are not subcomponents of other operators
				if(isShared_)
					//nothing else to do
					return;
			}else
			file << "\n";

		//draw a component (a graph) or a subcomponent (a subgraph)
		if(mode == 1)
			{
				//main component in the globalOpList
				file << tabs << "digraph " << getName() << "\n";
			} else if(mode == 2)
			{
				//a subcomponent
				file << tabs << "subgraph cluster_" << getName() << "\n";
			}else
			{
				THROWERROR("In drawDotDiagram: unhandled mode=" << mode);
			}

		file << tabs << "{\n";

		//increase the tabulation
		tabs = join(tabs, "\t");

		//if this is a

		file << tabs << "//graph drawing options\n";
		file << tabs << "label=" << getName() << ";\n"
				 << tabs << "labelloc=bottom;\n" << tabs << "labeljust=right;\n";

		if(mode == 2)
			file << tabs << "style=\"bold, dotted\";\n";

		if(dotDrawingMode == "compact")
			file << tabs << "ratio=compress;\n" << tabs << "fontsize=8;\n";
		else
			file << tabs << "ratio=auto;\n";

		//check if it's worth drawing the subcomponent in the compact style
		int maxInputCycle = -1, maxOutputCycle = -1;
		for(int i=0; (unsigned int)i<ioList_.size(); i++)
			if((ioList_[i]->type() == Signal::in) && (ioList_[i]->getCycle() > maxInputCycle))
				maxInputCycle = ioList_[i]->getCycle();
			else if((ioList_[i]->type() == Signal::out) && (ioList_[i]->getCycle() > maxOutputCycle))
				maxOutputCycle = ioList_[i]->getCycle();
		//if the inputs and outputs of a subcomponent are at the same cycle,
		//	then it's probably not worth drawing the operator in the compact style
		if((maxOutputCycle-maxInputCycle < 1) && (dotDrawingMode == "compact") && (mode == 2))
			mustDrawCompact = false;

		//if the dot drawing option is compact
		//	then, if this is a subcomponent, only draw the input-output connections
		if((mode == 2) && (dotDrawingMode == "compact") && mustDrawCompact)
			{
				file << tabs << "nodesep=0.15;\n" << tabs << "ranksep=0.15;\n" << tabs << "concentrate=yes;\n\n";

				//draw the input/output signals
				file << tabs << "//input/output signals of operator " << this->getName() << "\n";
				for(int i=0; (unsigned int)i<ioList_.size(); i++)
					file << drawDotNode(ioList_[i], tabs);

				//draw the subcomponents of this operator
				file << "\n" << tabs << "//subcomponents of operator " << this->getName() << "\n";
				for(int i=0; (unsigned int)i<subComponentList_.size(); i++)
					subComponentList_[i]->drawDotDiagram(file, 2, dotDrawingMode, tabs);

				file << "\n";

				//draw the invisible node, which replaces the content of the subcomponent
				file << tabs << "//signal connections of operator " << this->getName() << "\n";
				file << drawDotNode(NULL, tabs);
				//draw edges between the inputs and the intermediary node
				for(int i=0; (unsigned int)i<ioList_.size(); i++)
					if(ioList_[i]->type() == Signal::in)
						file << drawDotEdge(ioList_[i], NULL, tabs);
				//draw edges between the intermediary node and the outputs
				for(int i=0; (unsigned int)i<ioList_.size(); i++)
					if(ioList_[i]->type() == Signal::out)
						file << drawDotEdge(NULL, ioList_[i], tabs);
			}else
			{
				file << tabs << "nodesep=0.25;\n" << tabs << "ranksep=0.5;\n\n";

				//draw the input/output signals
				int inputCount = 0, outputCount = 0;
				file << tabs << "//input/output signals of operator " << this->getName() << "\n";
				for(int i=0; (unsigned int)i<ioList_.size(); i++)
					{
						file << drawDotNode(ioList_[i], tabs);
						inputCount += ((ioList_[i]->type() == Signal::in) ? 1 : 0);
						outputCount += ((ioList_[i]->type() == Signal::out) ? 1 : 0);
					}
				//force all the inputs of the operator to have the same rank (same for the outputs)
				string inputRankString = "", outputRankString = "";
				for(int i=0; (unsigned int)i<ioList_.size(); i++)
					{
						if(ioList_[i]->type() == Signal::in)
							inputRankString += " " + ioList_[i]->getName() + "__" + this->getName() + ",";
						if(ioList_[i]->type() == Signal::out)
							outputRankString += " " + ioList_[i]->getName() + "__" + this->getName() + ",";
					}
				if(inputRankString != "")
					{
						//remove the last comma
						inputRankString = inputRankString.substr(0, inputRankString.size()-1);
						file << tabs << "{rank=same" << inputRankString << "};" << "\n";
					}
				if(outputRankString != "")
					{
						//remove the last comma
						outputRankString = outputRankString.substr(0, outputRankString.size()-1);
						file << tabs << "{rank=same" << outputRankString << "};" << "\n";
					}
				//draw the signals of this operator as nodes
				file << tabs << "//internal signals of operator " << this->getName() << "\n";
				for(int i=0; (unsigned int)i<signalList_.size(); i++)
					file << drawDotNode(signalList_[i], tabs);

				//draw the subcomponents of this operator
				file << "\n" << tabs << "//subcomponents of operator " << this->getName() << "\n";
				for(auto i: subComponentList_) {
					if(!i->isShared()) {
						i->drawDotDiagram(file, 2, dotDrawingMode, tabs);
					}
				}
				file << "\n";

				//draw the out connections of each input of this operator
				file << tabs << "//input and internal signal connections of operator " << this->getName() << "\n";
				for(int i=0; (unsigned int)i<ioList_.size(); i++)
					if(ioList_[i]->type() == Signal::in)
						file << drawDotNodeEdges(ioList_[i], tabs);
				//draw the out connections of each signal of this operator
				for(int i=0; (unsigned int)i<signalList_.size(); i++)
					file << drawDotNodeEdges(signalList_[i], tabs);
			}

		//decrease tabulation
		tabs = tabs.substr(0, tabs.length()-1);
		//end of component/subcomponent
		file << tabs << "}\n\n";
		//increase tabulation
		tabs = join(tabs, "\t");

		//for subcomponents, draw the connections of the output ports
		//draw the out connections of each output of this operator
		if(mode == 2)
			{
				file << tabs << "//output signal connections of operator " << this->getName() << "\n";
				for(int i=0; (unsigned int)i<ioList_.size(); i++)
					if(ioList_[i]->type() == Signal::out)
						file << drawDotNodeEdges(ioList_[i], tabs);
			}
	}

	std::string Operator::drawDotNode(Signal *node, std::string tabs)
	{
	  ostringstream stream;
	  std::string nodeName = (node!=NULL ? node->getName() : "invisibleNode");

	  //different flow for the invisible node, that replaces the content of a subcomponent
	  if(node == NULL)
			{
				//output the node name
				//	for uniqueness purposes the name is signal_name::parent_operator_name
				stream << tabs << nodeName << "__" << this->getName() << " ";

				//output the node's properties
				stream << "[ label=\"...\", shape=plaintext, color=black, style=\"bold\", fontsize=32, fillcolor=white];\n";

				return stream.str();
			}

	  //process the node's name for correct dot format
	  if(node->type() == Signal::constant)
		  nodeName = node->getName().substr(node->getName().find("_cst")+1);

	  //output the node name
	  //	for uniqueness purposes the name is signal_name::parent_operator_name
	  stream << tabs << nodeName << "__" << node->parentOp()->getName() << " ";

	  //output the node's properties
	  stream << "[ label=\"" << nodeName << "\\n" << "dT = " << node->getCriticalPathContribution() << "\\n(" << node->getCycle() << ", "
					 << node->getCriticalPath() << ")\"";
	  stream << ", shape=box, color=black";
	  stream << ", style" << ((node->type() == Signal::in || node->type() == Signal::out) ? "=\"bold, filled\"" : "=filled");
	  stream << ", fillcolor=" << Signal::getDotNodeColor(node->getCycle());
	  stream << ", peripheries=" << (node->type() == Signal::in ? "2" : node->type() == Signal::out ? "3" : "1");
	  stream << " ];\n";

	  return stream.str();
	}

	std::string Operator::drawDotNodeEdges(Signal *node, std::string tabs)
	{
	  ostringstream stream;
	  std::string nodeName = node->getName();
	  std::string nodeParentName;

	  //process the node's name for correct dot format
	  if(node->type() == Signal::constant)
		  nodeName = node->getName().substr(node->getName().find("_cst")+1);

	  for(int i=0; (unsigned int)i<node->successors()->size(); i++)
			{
				if(node->successor(i)->type() == Signal::constant)
					nodeParentName = node->successor(i)->getName().substr(node->successor(i)->getName().find("_cst")+1);
				else
					nodeParentName = node->successor(i)->getName();

	      stream << tabs << nodeName << "__" << node->parentOp()->getName() << " -> "
							 << nodeParentName << "__" << node->successor(i)->parentOp()->getName() << " [";
	      stream << " arrowhead=normal, arrowsize=1.0, arrowtail=normal, color=black, dir=forward ";
				// Removed by F2D: never-used functional delays
				//	      stream << " label=" << max(0, node->successorPair(i)->second);
				stream << " ];\n";
			}

	  return stream.str();
	}

	std::string Operator::drawDotEdge(Signal *source, Signal *sink, std::string tabs)
	{
		ostringstream stream;
		std::string sourceNodeName = (source == NULL ? "invisibleNode" : source->getName());
		std::string sinkNodeName = (sink == NULL ? "invisibleNode" : sink->getName());

		//process the source node's name for correct dot format
		if((source != NULL) && (source->type() == Signal::constant))
			sourceNodeName = source->getName().substr(source->getName().find("_cst")+1);
		//process the sink node's name for correct dot format
		if((sink != NULL) && (sink->type() == Signal::constant))
			sinkNodeName = sink->getName().substr(sink->getName().find("_cst")+1);

		stream << tabs << sourceNodeName << "__" << (source!=NULL ? source->parentOp()->getName() : this->getName()) << " -> "
					 << sinkNodeName << "__" << (sink!=NULL ? sink->parentOp()->getName() : this->getName()) << " [";
		stream << " arrowhead=normal, arrowsize=1.0, arrowtail=normal, color=black, dir=forward";
		if((source != NULL) && (sink != NULL))
			stream << " label=" << max(0, sink->getCycle()-source->getCycle());
		stream << " ];\n";

		return stream.str();
	}


	void Operator::setuid(int mm){
		myuid = mm;
	}

	int Operator::getuid(){
		return myuid;
	}



	void  Operator::setIndirectOperator(Operator* op){
		indirectOperator_ = op;

		if(op != NULL){
			op->setuid(getuid()); //the selected implementation becomes this operator

			op->setNameWithFreqAndUID(getName());//accordingly set the name of the implementation

			signalList_ = op->signalList_;
			subComponentList_ = op->subComponentList_;
			ioList_ = op->ioList_;
		}
	}

	void Operator::cleanup(vector<Operator*> *ol, Operator* op){
		//iterate through all the components of op
		for(unsigned int it=0; it<op->subComponentList_.size(); it++)
			cleanup(ol, subComponentList_[it]);

		for(unsigned j=0; j<(*ol).size(); j++){
			if((*ol)[j]->myuid == op->myuid){
				(*ol).erase((*ol).begin()+j);
			}
		}
	}

	string Operator::signExtend(string name, int w){
		Signal* s;

		try{
			s = getSignalByName(name);
		}
		catch(string &e){
			THROWERROR("In signExtend, " << e);
		}

		//get the signals's width
		if(w == s->width()){
			//nothing to do
			return name;
		}
		else if(w < s->width()){
			REPORT(INFO, "WARNING: signExtend() called for a sign extension to " << w
						 << " bits of signal " << name << " whose width is " << s->width());
			return name;
		}
		else{
			ostringstream n;
			n << "(";
			for(int i=0; i<(w - s->width()); i++){
				n << name << of(s->width() -1) << " & ";
			}
			n << name << ")";
			return n.str();
		}
	}

	string Operator::zeroExtend(string name, int w){
		Signal* s;

		try{
			s = getSignalByName(name);
		}
		catch(string &e){
			THROWERROR("In zeroExtend, " << e);
		}

		//get the signals's width
		if (w == s->width()){
			//nothing to do
			return name;
		}
		else if(w < s->width()){
			REPORT(INFO,  "WARNING: zeroExtend() to " << w << " bits of signal " << name << " whose width is " << s->width());
			return name;
		}
		else{
			ostringstream n;
			n << "(" << zg(w-s->width()) << " &" << name << ")";
			return n.str();
		}
	}

	void Operator::emulate(TestCase * tc) {
		throw std::string("emulate() not implemented for ") + uniqueName_;
	}

	bool Operator::hasComponent(string s){
		return (getSubComponent(s) != NULL);
	}

	void Operator::addComment(string comment, string align){
		vhdl << align << "-- " << comment << endl;
	}

	void Operator::addFullComment(string comment, int lineLength) {
		string align = "--";
		// - 2 for the two spaces
		for (unsigned i = 2; i < (lineLength-2-comment.size()) / 2; i++)
			align += "-";
		vhdl << align << " " << comment << " " << align << endl;
	}


	//Completely replace "this" with a copy of another operator.
	void  Operator::cloneOperator(Operator *op){
		subComponentList_           = op->getSubComponentList();
		signalList_                 = op->getSignalList();
		ioList_                     = op->getIOListV();

		parentOp_                   = op->parentOp_;

		target_                     = op->getTarget();
		uniqueName_                 = op->getUniqueName();
		architectureName_           = op->getArchitectureName();
		testCaseSignals_            = op->getTestCaseSignals();
		vhdl.vhdlCode.str(op->vhdl.vhdlCode.str());
		vhdl.vhdlCodeBuffer.str(op->vhdl.vhdlCodeBuffer.str());

		vhdl.dependenceTable        = op->vhdl.dependenceTable;

		srcFileName                 = op->getSrcFileName();
		cost                        = op->getOperatorCost();
		subComponentList_           = op->getSubComponentList();
		stdLibType_                 = op->getStdLibType();
		isSequential_               = op->isSequential();
		pipelineDepth_              = op->getPipelineDepth();
		signalMap_                  = op->getSignalMap();
		constants_                  = op->getConstants();
		attributes_                 = op->getAttributes();
		types_                      = op->getTypes();
		attributesValues_           = op->getAttributesValues();

		commentedName_              = op->commentedName_;
		headerComment_              = op->headerComment_;
		copyrightString_            = op->getCopyrightString();
		hasClockEnable_             = op->hasClockEnable();
		indirectOperator_           = op->getIndirectOperator();
		hasDelay1Feedbacks_         = op->hasDelay1Feedbacks();

		isOperatorImplemented_      = op->isOperatorImplemented();
		isTopLevelDotDrawn_ = op->isOperatorDrawn();

		isShared_                   = op->isShared();
		isLibraryComponent_         = op->isLibraryComponent();

		resourceEstimate.str(op->resourceEstimate.str());
		resourceEstimateReport.str(op->resourceEstimateReport.str());
		reHelper                    = op->reHelper;
		reActive                    = op->reActive;

		floorplan.str(op->floorplan.str());
		flpHelper                   = op->flpHelper;

		generics_					= op->getGenerics();
	}


	void  Operator::deepCloneOperator(Operator *op){
		//start by cloning the operator
		cloneOperator(op);

		//create deep copies of the signals, sub-components, instances etc.

		//create deep copies of the signals
		vector<Signal*> newSignalList;
		for(unsigned int i=0; i<signalList_.size(); i++)
			{
				if((signalList_[i]->type() == Signal::in) || (signalList_[i]->type() == Signal::out))
					continue;

				Signal* tmpSignal = new Signal(this, signalList_[i]);

				//if this a constant signal, then it doesn't need to be scheduled
				if((tmpSignal->type() == Signal::constant) || (tmpSignal->type() == Signal::constantWithDeclaration))
					{
						tmpSignal->setCycle(0);
						tmpSignal->setCriticalPath(0.0);
						tmpSignal->setCriticalPathContribution(0.0);
						tmpSignal->setHasBeenScheduled(true);
					}

				newSignalList.push_back(tmpSignal);
			}
		signalList_.clear();
		signalList_.insert(signalList_.begin(), newSignalList.begin(), newSignalList.end());

		//create deep copies of the inputs/outputs
		vector<Signal*> newIOList;
		for(unsigned int i=0; i<ioList_.size(); i++)
			{
				Signal* tmpSignal = new Signal(this, ioList_[i]);

				//if this a constant signal, then it doesn't need to be scheduled
				if((tmpSignal->type() == Signal::constant) || (tmpSignal->type() == Signal::constantWithDeclaration))
					{
						tmpSignal->setCycle(0);
						tmpSignal->setCriticalPath(0.0);
						tmpSignal->setCriticalPathContribution(0.0);
						tmpSignal->setHasBeenScheduled(true);
					}

				newIOList.push_back(tmpSignal);
			}
		ioList_.clear();
		ioList_.insert(ioList_.begin(), newIOList.begin(), newIOList.end());
		//signalList_.insert(signalList_.end(), newIOList.begin(), newIOList.end());

		//update the signal map
		signalMap_.clear();
		//insert the inputs/outputs
		for(unsigned int i=0; i<ioList_.size(); i++)
		  signalMap_[ioList_[i]->getName()] = ioList_[i];
		//insert the internal signals
		for(unsigned int i=0; i<signalList_.size(); i++)
		  signalMap_[signalList_[i]->getName()] = signalList_[i];

		//create deep copies of the subcomponents
		vector<Operator*> newOpList;
		for(unsigned int i=0; i<op->getSubComponentList().size(); i++)
			{
				Operator* tmpOp = new Operator(op, op->getSubComponentList()[i]->getTarget());

				tmpOp->deepCloneOperator(op->getSubComponentList()[i]);
				//add the new subcomponent to the subcomponent list
				newOpList.push_back(tmpOp);
			}
		subComponentList_.clear();
		subComponentList_ = newOpList;

		//recreate the signal dependences, for each of the signals
		for(unsigned int i=0; i<signalList_.size(); i++)
			{
				vector<pair<Signal*, int>> newPredecessors, newSuccessors;
				Signal *originalSignal = op->getSignalByName(signalList_[i]->getName());

				//create the new list of predecessors for the signal currently treated
				for(unsigned int j=0; j<originalSignal->predecessors()->size(); j++)
					{
						pair<Signal*, int> tmpPair = *(originalSignal->predecessorPair(j));

						//for the input signals, there is no need to do the predecessor list here
						if(signalList_[i]->type() == Signal::in)
							{
								break;
							}

						//if the signal is connected to the output of a subcomponent,
						//	then just skip this predecessor, as it will be added later on
						if((tmpPair.first->type() == Signal::out)
							 && (tmpPair.first->parentOp()->getName() != originalSignal->parentOp()->getName()))
							continue;

						//signals connected only to constants are already scheduled
						if(((tmpPair.first->type() == Signal::constant) || (tmpPair.first->type() == Signal::constantWithDeclaration))
							 && (originalSignal->predecessors()->size() == 1))
							{
								signalList_[i]->setCycle(0);
								signalList_[i]->setCriticalPath(0.0);
								signalList_[i]->setCriticalPathContribution(0.0);
								signalList_[i]->setHasBeenScheduled(true);
							}

						newPredecessors.push_back(make_pair(getSignalByName(tmpPair.first->getName()), tmpPair.second));
					}
				//replace the old list of predecessors with the new one
				signalList_[i]->resetPredecessors();
				signalList_[i]->addPredecessors(newPredecessors);

				//create the new list of successors for the signal currently treated
				for(unsigned int j=0; j<originalSignal->successors()->size(); j++)
					{
						pair<Signal*, int> tmpPair = *(originalSignal->successorPair(j));

						//for the output signals, there is no need to do the successor list here
						if(signalList_[i]->type() == Signal::out)
							{
								break;
							}

						//if the signal is connected to the input of a subcomponent,
						//	then just skip this successor, as it will be added later on
						if((tmpPair.first->type() == Signal::in)
							 && (tmpPair.first->parentOp()->getName() != originalSignal->parentOp()->getName()))
							continue;

						newSuccessors.push_back(make_pair(getSignalByName(tmpPair.first->getName()), tmpPair.second));
					}
				//replace the old list of predecessors with the new one
				signalList_[i]->resetSuccessors();
				signalList_[i]->addSuccessors(newSuccessors);
			}

		//update the signal map
		signalMap_.clear();
		for(unsigned int i=0; i<signalList_.size(); i++)
		  signalMap_[signalList_[i]->getName()] = signalList_[i];
		for(unsigned int i=0; i<ioList_.size(); i++)
		  signalMap_[ioList_[i]->getName()] = ioList_[i];

		//no need to recreate the signal dependences for each of the input/output signals,
		//	as this is either done by instance, or it is done by the parent operator of this operator
		//	or, they may not need to be set at all (e.g. when you just copy an operator)

		//connect the inputs and outputs of the subcomponents to the corresponding signals
		for(unsigned int i=0; i<subComponentList_.size(); i++)
			{
				Operator *currentOp = subComponentList_[i], *originalOp;

				//look for the original operator
				for(unsigned int j=0; j<op->getSubComponentList().size(); j++)
					if(currentOp->getName() == op->getSubComponentList()[j]->getName())
						{
							originalOp = op->getSubComponentList()[j];
							break;
						}

				//connect the inputs/outputs of the subcomponent
				for(unsigned int j=0; j<currentOp->getIOList()->size(); j++)
					{
						Signal *currentIO = currentOp->getIOListSignal(j), *originalIO;

						//recreate the predecessor and successor list, as it is in
						//	the original operator, which we are cloning
						originalIO = originalOp->getSignalByName(currentIO->getName());
						//recreate the predecessor/successor (for the input/output) list
						for(unsigned int k=0; k<originalIO->predecessors()->size(); k++)
							{
								pair<Signal*, int>* tmpPair = originalIO->predecessorPair(k);

								//if the signal is only connected to a constant,
								//	then the signal doesn't need to be scheduled
								if(((tmpPair->first->type() == Signal::constant) || (tmpPair->first->type() == Signal::constantWithDeclaration))
									 && (originalIO->predecessors()->size() == 1))
									{
										currentIO->setCycle(0);
										currentIO->setCriticalPath(0.0);
										currentIO->setCriticalPathContribution(0.0);
										currentIO->setHasBeenScheduled(true);
									}

								if(currentIO->type() == Signal::in)
									{
										currentIO->addPredecessor(getSignalByName(tmpPair->first->getName()), tmpPair->second);
										getSignalByName(tmpPair->first->getName())->addSuccessor(currentIO, tmpPair->second);
									}else if(currentIO->type() == Signal::out)
									{
										currentIO->addPredecessor(currentOp->getSignalByName(tmpPair->first->getName()), tmpPair->second);
										currentOp->getSignalByName(tmpPair->first->getName())->addSuccessor(currentIO, tmpPair->second);
									}
							}
						//recreate the successor/predecessor (for the input/output) list
						for(unsigned int k=0; k<originalIO->successors()->size(); k++)
							{
								pair<Signal*, int>* tmpPair = originalIO->successorPair(k);

								if(currentIO->type() == Signal::in)
									{
										currentIO->addSuccessor(currentOp->getSignalByName(tmpPair->first->getName()), tmpPair->second);
										currentOp->getSignalByName(tmpPair->first->getName())->addPredecessor(currentIO, tmpPair->second);
									}else if(currentIO->type() == Signal::out)
									{
										currentIO->addSuccessor(getSignalByName(tmpPair->first->getName()), tmpPair->second);
										getSignalByName(tmpPair->first->getName())->addPredecessor(currentIO, tmpPair->second);
									}
							}
					}
			}

		//create deep copies of the instances
		//	replace the references in the instances to references to
		//	the newly created deep copies
		//the port maps for the instances do not need to be modified
		subComponentList_.clear();
		for(unsigned int i=0; i<subComponentList_.size(); i++)
			subComponentList_.push_back(subComponentList_[i]);

	}



	bool Operator::isOperatorImplemented(){
		return isOperatorImplemented_;
	}

	void Operator::setIsOperatorImplemented(bool newValue){
		isOperatorImplemented_ = newValue;
	}

	bool Operator::isOperatorScheduled(){
	  return isOperatorScheduled_;
	}

	void Operator::setIsOperatorScheduled(bool newValue){
	  isOperatorScheduled_ = newValue;
	}

	void Operator::setShared(){
		isShared_ = true;
		isSequential_ = false; // shared operators must be combinatorial
	}

	bool Operator::isShared(){
		return isShared_;
	}

	void Operator::setLibraryComponent(){
		isLibraryComponent_ = true;
	}

	bool Operator::isLibraryComponent(){
		return isLibraryComponent_;
	}

	map<std::string, std::string> Operator::getGenerics(){
		return generics_;
	}

	void Operator::applySchedule()
	{
		// launch the second VHDL parsing step. Works for sequential and combinatorial operators as well
		if(!isOperatorApplyScheduleDone_) {
			isOperatorApplyScheduleDone_=true;
			doApplySchedule();
			// recursive call for the operator's subcomponents
			for(auto it: subComponentList_) {
				it->applySchedule();
			}
			computePipelineDepths();	 // also for the subcomponents
		}
	}


	void Operator::outputVHDLToFile(vector<Operator*> &oplist, ofstream& file)
	{
		string srcFileName = "Operator.cpp"; // for REPORT

		for(auto it: oplist)
			{
				try {
					// check for subcomponents
					if(! it->getSubComponentListR().empty() ){
						//recursively call to print subcomponents
						outputVHDLToFile(it->getSubComponentListR(), file);
					}

					//output the vhdl code to file
					//	for global operators, this is done only once
					if(!it->isOperatorImplemented())
						{
							it->outputVHDL(file);
							it->setIsOperatorImplemented(true);
						}
				} catch (std::string &s) {
					cerr << "Exception while generating '" << it->getName() << "': " << s << endl;
				}
			}
	}




#if 1
	void Operator::outputVHDLToFile(ofstream& file){
		vector<Operator*> oplist;

		REPORT(DEBUG, "Entering outputVHDLToFile");

		//build a copy of the global oplist hidden in UserInterface (if it exists):
		for (unsigned i=0; i<UserInterface::globalOpList.size(); i++)
			oplist.push_back(UserInterface::globalOpList[i]);
		// add self (and all its subcomponents) to this list
		oplist.push_back(this);
		// generate the code
		Operator::outputVHDLToFile(oplist, file);
	}
#endif


	void Operator::setHasDelay1Feedbacks(){
		hasDelay1Feedbacks_=true;
	}


	bool Operator::hasDelay1Feedbacks(){
		return hasDelay1Feedbacks_;
	}


	/////////////////////////////////////////////////////////////////////////////////////////////////
	////////////Functions used for resource estimations


	//--Logging functions

	std::string Operator::addFF(int count){
		reActive = true;
		return reHelper->addFF(count);
	}

	std::string Operator::addLUT(int nrInputs, int count){
		reActive = true;
		return reHelper->addLUT(nrInputs, count);
	}

	std::string Operator::addReg(int width, int count){
		reActive = true;
		return reHelper->addReg(width, count);
	}

	std::string Operator::addMultiplier(int count){
		reActive = true;
		return reHelper->addMultiplier(count);
	}

	std::string Operator::addMultiplier(int widthX, int widthY, double ratio, int count){
		reActive = true;
		return reHelper->addMultiplier(widthX, widthY, ratio, count);
	}

	std::string Operator::addAdderSubtracter(int widthX, int widthY, double ratio, int count){
		reActive = true;
		return reHelper->addAdderSubtracter(widthX, widthY, ratio, count);
	}

	std::string Operator::addMemory(int size, int width, int type, int count){
		reActive = true;
		return reHelper->addMemory(size, width, type, count);
	}

	//---More particular resource logging
	std::string Operator::addDSP(int count){
		reActive = true;
		return reHelper->addDSP(count);
	}

	std::string Operator::addRAM(int count){
		reActive = true;
		return reHelper->addRAM(count);
	}

	std::string Operator::addROM(int count){
		reActive = true;
		return reHelper->addROM(count);
	}

	std::string Operator::addSRL(int width, int depth, int count){
		reActive = true;
		return reHelper->addSRL(width, depth, count);
	}

	std::string Operator::addWire(int count, std::string signalName){
		reActive = true;
		return reHelper->addWire(count, signalName);
	}

	std::string Operator::addIOB(int count, std::string portName){
		reActive = true;
		return reHelper->addIOB(count, portName);
	}

	//---Even more particular resource logging-------------------------

	std::string Operator::addMux(int width, int nrInputs, int count){
		reActive = true;
		return reHelper->addMux(width, nrInputs, count);
	}

	std::string Operator::addCounter(int width, int count){
		reActive = true;
		return reHelper->addCounter(width, count);
	}

	std::string Operator::addAccumulator(int width, bool useDSP, int count){
		reActive = true;
		return reHelper->addAccumulator(width, useDSP, count);
	}

	std::string Operator::addDecoder(int wIn, int wOut, int count){
		reActive = true;
		return reHelper->addDecoder(wIn, wOut, count);
	}

	std::string Operator::addArithOp(int width, int nrInputs, int count){
		reActive = true;
		return reHelper->addArithOp(width, nrInputs, count);
	}

	std::string Operator::addFSM(int nrStates, int nrTransitions, int count){
		reActive = true;
		return reHelper->addFSM(nrStates, nrTransitions, count);
	}

	//--Resource usage statistics---------------------------------------
	std::string Operator::generateStatistics(int detailLevel){
		reActive = true;
		return reHelper->generateStatistics(detailLevel);
	}

	//--Utility functions related to the generation of resource usage statistics

	std::string Operator::addPipelineFF(){
		reActive = true;
		return reHelper->addPipelineFF();
	}

	std::string Operator::addWireCount(){
		reActive = true;
		return reHelper->addWireCount();
	}

	std::string Operator::addPortCount(){
		reActive = true;
		return reHelper->addPortCount();
	}

	std::string Operator::addComponentResourceCount(){
		reActive = true;
		return reHelper->addComponentResourceCount();
	}

	void Operator::addAutomaticResourceEstimations(){
		reActive = true;
		resourceEstimate << reHelper->addAutomaticResourceEstimations();
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////////////////////////////////
	////////////Functions used for floorplanning

	std::string Operator::manageFloorplan(){
		return flpHelper->manageFloorplan();
	}

	std::string Operator::addPlacementConstraint(std::string source, std::string sink, int type){
		return flpHelper->addPlacementConstraint(source, sink, type);
	}

	std::string Operator::addConnectivityConstraint(std::string source, std::string sink, int nrWires){
		return flpHelper->addConnectivityConstraint(source, sink, nrWires);
	}

	std::string Operator::addAspectConstraint(std::string source, double ratio){
		return flpHelper->addAspectConstraint(source, ratio);
	}

	std::string Operator::addContentConstraint(std::string source, int value, int length){
		return flpHelper->addContentConstraint(source, value, length);
	}

	std::string Operator::processConstraints(){
		return flpHelper->processConstraints();
	}

	std::string Operator::createVirtualGrid(){
		return flpHelper->createVirtualGrid();
	}

	std::string Operator::createPlacementGrid(){
		return flpHelper->createPlacementGrid();
	}

	std::string Operator::createConstraintsFile(){
		return flpHelper->createConstraintsFile();
	}

	std::string Operator::createPlacementForComponent(std::string moduleName){
		return flpHelper->createPlacementForComponent(moduleName);
	}

	std::string Operator::createFloorplan(){
		return flpHelper->createFloorplan();
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////

}
