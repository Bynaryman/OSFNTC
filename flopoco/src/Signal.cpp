#include <iostream>
#include <sstream>

#include "Signal.hpp"
#include "Operator.hpp"

using namespace std;

namespace flopoco{


	const vector<string> Signal::dotNodeColors = []()->vector<string>{
  	    vector<string> v;

  	    v.push_back("coral");
  	    v.push_back("chartreuse");
  	    v.push_back("cyan");
  	    v.push_back("orchid");
  	    v.push_back("cadetblue");
  	    v.push_back("orangered");
  	    v.push_back("chocolate");
  	    v.push_back("mintcream");
  	    v.push_back("tomato");
  	    v.push_back("slategray");
  	    v.push_back("sienna");
  	    v.push_back("indigo");
  	    v.push_back("greenyellow");
  	    v.push_back("plum");
  	    v.push_back("maroon");
  	    v.push_back("palegoldenrod");
  	    v.push_back("cornflowerblue");
  	    v.push_back("khaki");
  	    v.push_back("aquamarine");
  	    v.push_back("peru");
  	    v.push_back("olivedrab");
  	    v.push_back("powderblue");
  	    v.push_back("darksalmon");
  	    v.push_back("aliceblue");
  	    v.push_back("burlywood");
  	    v.push_back("peachpuff");
  	    v.push_back("magenta");
  	    v.push_back("rosybrown");
  	    v.push_back("dodgerblue");
  	    v.push_back("sandybrown");
  	    v.push_back("papayawhip");

  	    return v;
  	}();


	// plain logic vector, or wire
	Signal::Signal(Operator* parentOp, const string name, const Signal::SignalType type, const int width, const bool isBus) :
		parentOp_(parentOp), name_(name), type_(type), resetType_(noReset), width_(width), constValue_(0.0), tableAttributes_(""), numberOfPossibleValues_(1),
		lifeSpan_(0), cycle_(0), criticalPath_(0.0), criticalPathContribution_(0.0),
		incompleteDeclaration_(false), hasBeenScheduled_(false), hasBeenDrawn_(false),
		isFP_(false), isFix_(false), isIEEE_(false),
		wE_(0), wF_(0), MSB_(width-1), LSB_(0),
		isSigned_(false), isBus_(isBus)
	{
		predecessors_.clear();
		successors_.clear();
	}

	// constant signal
	Signal::Signal(Operator* parentOp, const std::string name, const Signal::SignalType type, const double constValue) :
		parentOp_(parentOp), name_(name), type_(type), resetType_(noReset), constValue_(constValue), tableAttributes_(""), numberOfPossibleValues_(1),
		lifeSpan_(0), cycle_(0), criticalPath_(0.0), criticalPathContribution_(0.0),
		incompleteDeclaration_(false), hasBeenScheduled_(false), hasBeenDrawn_(false),
		isFP_(false), isFix_(false), isIEEE_(false),
		wE_(0), wF_(0), MSB_(0), LSB_(0),
		isBus_(false)
	{
		width_ = intlog2(fabs(constValue_));
		MSB_=width_-1;
		isSigned_ = constValue_ < 0;
		predecessors_.clear();
		successors_.clear();
	}


	// table
	Signal::Signal(Operator* parentOp, const std::string name, const Signal::SignalType type, const int width, const std::string tableValue) :
		parentOp_(parentOp), name_(name), type_(type), resetType_(noReset), width_(width), constValue_(0.0), tableAttributes_(tableValue), numberOfPossibleValues_(1),
		lifeSpan_(0), cycle_(0), criticalPath_(0.0), criticalPathContribution_(0.0),
		incompleteDeclaration_(false), hasBeenScheduled_(false), hasBeenDrawn_(false),
		isFP_(false), isFix_(false), isIEEE_(false),
		wE_(0), wF_(0), MSB_(width-1), LSB_(0),
		isSigned_(false), isBus_(false)
	{
		predecessors_.clear();
		successors_.clear();
	}


	// fixed point constructor
	Signal::Signal(Operator* parentOp, const string name, const Signal::SignalType type, const bool isSigned, const int MSB, const int LSB) :
		parentOp_(parentOp), name_(name), type_(type), resetType_(noReset), width_(MSB-LSB+1), constValue_(0.0), tableAttributes_(""), numberOfPossibleValues_(1),
		lifeSpan_(0), cycle_(0), criticalPath_(0.0), criticalPathContribution_(0.0),
		incompleteDeclaration_(false), hasBeenScheduled_(false), hasBeenDrawn_(false),
		isFP_(false), isFix_(true),  isIEEE_(false),
		wE_(0), wF_(0), MSB_(MSB), LSB_(LSB),
		isSigned_(isSigned), isBus_(true)
	{
		predecessors_.clear();
		successors_.clear();
	}

	// floating point constructor
	Signal::Signal(Operator* parentOp, const string name, const Signal::SignalType type, const int wE, const int wF, const bool ieeeFormat) :
		parentOp_(parentOp), name_(name), type_(type), resetType_(noReset), width_(wE+wF+3), constValue_(0.0), tableAttributes_(""), numberOfPossibleValues_(1),
		lifeSpan_(0), cycle_(0), criticalPath_(0.0), criticalPathContribution_(0.0),
		incompleteDeclaration_(false), hasBeenScheduled_(false), hasBeenDrawn_(false),
		isFP_(true), isFix_(false), isIEEE_(false),
		wE_(wE), wF_(wF), MSB_(0), LSB_(0),
		isSigned_(false), isBus_(false)
	{
		if(ieeeFormat) { // correct some of the initializations above
			width_  = wE+wF+1;
			isFP_   = false;
			isIEEE_ = true;
		}
		predecessors_.clear();
		successors_.clear();
	}

	Signal::Signal(Signal* originalSignal)
	{
		parentOp_ = originalSignal->parentOp_;
		name_ = originalSignal->getName();
		type_ = originalSignal->type();
		resetType_= originalSignal->resetType();
		copySignalParameters(originalSignal);
	}

	Signal::Signal(Operator* newParentOp, Signal* originalSignal)
	{
		parentOp_ = newParentOp;
		name_ = originalSignal->getName();
		type_ = originalSignal->type();
		resetType_= originalSignal->resetType();

		copySignalParameters(originalSignal);
	}

	Signal::~Signal(){}

	void	Signal::promoteToFix(const bool isSigned, const int MSB, const int LSB){
		if(isFP_){
			std::ostringstream o;
			o << "Error in Signal::promoteToFix(" <<  getName() << "): can't promote a floating-point signal to fix point";
			throw o.str();
		}
		if(MSB - LSB +1 != width_){
			std::ostringstream o;
			o << "Error in Signal::promoteToFix(" <<  getName() << "): width doesn't match";
			throw o.str();
		}
		isFix_ = true;
		MSB_   = MSB;
		LSB_   = LSB;
		isSigned_ = isSigned;
	}


	void Signal::copySignalParameters(Signal *originalSignal)
	{
		lifeSpan_ = 0;
		constValue_ = originalSignal->constValue_;
		tableAttributes_ = originalSignal->tableAttributes();
		isFix_ = originalSignal->isFix();
		isFP_ = originalSignal->isFP();
		cycle_ = originalSignal->getCycle();
		criticalPath_ = originalSignal->getCriticalPath();
		criticalPathContribution_ = originalSignal->getCriticalPathContribution();
		numberOfPossibleValues_ = originalSignal->getNumberOfPossibleValues();
		incompleteDeclaration_ = originalSignal->incompleteDeclaration_;
		hasBeenScheduled_ = false;
		hasBeenDrawn_ = false;

		// Some of the following parameters are only used in some contexts but it doesn't hurt to copy them all
		width_  = originalSignal->width_;
		wE_     = originalSignal->wE();
		wF_     = originalSignal->wF();
		isIEEE_ = originalSignal->isIEEE_;
		MSB_    = originalSignal->MSB();
		LSB_    = originalSignal->LSB();
		isBus_  = originalSignal->isBus();
		isSigned_ = originalSignal->isSigned();
	}


	const string& Signal::getName() const {
		return name_;
	}

	string Signal::getUniqueName() {
		ostringstream s;
		s << "uid" <<  parentOp_->getuid() << ":" << name_;
		return s.str();
	}

	Operator* Signal::parentOp() const {return parentOp_;}

	void Signal::setParentOp(Operator* newParentOp)
	{
		parentOp_ = newParentOp;
	}

	int Signal::width() const {return width_;}

	const std::string& Signal::tableAttributes() const {
		return tableAttributes_;
	}

	void Signal::setTableAttributes(std::string newTableAttributes){
		tableAttributes_ = newTableAttributes;
	}

	int Signal::wE() const {return(wE_);}

	int Signal::wF() const {return(wF_);}

	int Signal::MSB() const {return(MSB_);}

	int Signal::LSB() const {return(LSB_);}

	bool Signal::isSigned() const {return isSigned_;};

	void Signal::setIsSigned(bool newIsSigned) {isSigned_ = newIsSigned;}

	bool Signal::isFix() const {return isFix_;}

	void Signal::setIsFix(bool newIsFix) {isFix_ = newIsFix;}

	bool Signal::isIEEE() const {return isIEEE_;}

	bool Signal::isFP() const {return isFP_;}

	void Signal::setIsFP(bool newIsFP) {isFP_ = newIsFP;}

	void Signal::setIsIEEE(bool newIsIEEE) {isIEEE_ = newIsIEEE;}

	bool Signal::isBus() const {return isBus_;}

	Signal::SignalType Signal::type() const {return type_;}

	void Signal::setName(std::string name) {
		name_=name;
	}

	void Signal::setType(SignalType t) {
		type_ = t;
	}

	void Signal::setResetType(Signal::ResetType t) {
		resetType_ = t;
	}

	Signal::ResetType Signal::resetType() {
		return resetType_;
	}


	vector<pair<Signal*, int>>* Signal::predecessors() {return &predecessors_;}

	Signal* Signal::predecessor(int count) {
		if(((unsigned)count >= predecessors_.size()) || (count < 0))
			throw("Error in Signal::predecessor: trying to access an element at an index outside of bounds");

		return predecessors_[count].first;
	}

	pair<Signal*, int>* Signal::predecessorPair(int count) {
		if(((unsigned)count >= predecessors_.size()) || (count < 0))
			throw("Error in Signal::predecessorPair: trying to access an element at an index outside of bounds");

		return &(predecessors_[count]);
	}

	vector<pair<Signal*, int>>* Signal::successors() {return &successors_;}

	Signal* Signal::successor(int count) {
		if(((unsigned)count >= successors_.size()) || (count < 0))
			throw("Error in Signal::successor: trying to access an element at an index outside of bounds");

		return successors_[count].first;
	}

	pair<Signal*, int>* Signal::successorPair(int count) {
		if(((unsigned)count >= successors_.size()) || (count < 0))
			throw("Error in Signal::successorPair: trying to access an element at an index outside of bounds");

		return &(successors_[count]);
	}

	void Signal::resetPredecessors()
	{
		predecessors_.clear();
	}

	void Signal::addPredecessor(Signal* predecessor, int delayCycles)
	{
		//check if the signal already exists, within the same instance
		for(int i=0; (unsigned)i<predecessors_.size(); i++)
		{
			pair<Signal*, int> predecessorPair = *(this->predecessorPair(i));
			if((predecessorPair.first->parentOp()->getName() == predecessor->parentOp()->getName())
					&& (predecessorPair.first->getName() == predecessor->getName())
				 && (predecessorPair.first->type() == predecessor->type())
					&& (predecessorPair.second == delayCycles))
			{
#if 0
				cerr << "in addPredecessor(): trying to add an already existing signal "
							 << predecessor->getName() << " to the predecessor list of " << name_ << endl;
#endif
				//nothing else to do
				return;
			}
		}

		//safe to insert a new signal in the predecessor list
		pair<Signal*, int> newPredecessorPair = make_pair(predecessor, delayCycles);
		predecessors_.push_back(newPredecessorPair);
	}

	void Signal::addPredecessors(vector<pair<Signal*, int>> predecessorList)
	{
		for(unsigned int i=0; i<predecessorList.size(); i++)
			addPredecessor(predecessorList[i].first, predecessorList[i].second);
	}

	void Signal::removePredecessor(Signal* predecessor, int delayCycles)
	{
		//only try to remove the predecessor if the signal
		//	already exists, within the same instance and with the same delay
		for(int i=0; (unsigned)i<predecessors_.size(); i++)
		{
			pair<Signal*, int> predecessorPair = *(this->predecessorPair(i));
			if((predecessorPair.first->parentOp()->getName() == predecessor->parentOp()->getName())
					&& (predecessorPair.first->getName() == predecessor->getName())
					&& (predecessorPair.first->type() == predecessor->type())
					&& (predecessorPair.second == delayCycles))
			{
				//delete the element from the list
				predecessors_.erase(predecessors_.begin()+i);
				return;
			}
		}

		throw("ERROR in removePredecessor(): trying to remove a non-existing signal "
				+ predecessor->getName() + " from the predecessor list");
	}

	void Signal::resetSuccessors()
	{
		successors_.clear();
	}

	void Signal::addSuccessor(Signal* successor, int delayCycles)
	{
		//check if the signal already exists, within the same instance
		for(int i=0; (unsigned)i<successors_.size(); i++)
		{
			pair<Signal*, int> successorPair = *(this->successorPair(i));
			if((successorPair.first->parentOp()->getName() == successor->parentOp()->getName())
					&& (successorPair.first->getName() == successor->getName())
					&& (successorPair.first->type() == successor->type())
					&& (successorPair.second == delayCycles))
			{
#if 0
				cerr << "in addSuccessor(): trying to add an already existing signal "
							 << successor->getName() << " to the successor list of " << name_ << endl;
#endif
				//nothing else to do
				return;
			}
		}

		//safe to insert a new signal in the predecessor list
		pair<Signal*, int> newSuccessorPair = make_pair(successor, delayCycles);
		successors_.push_back(newSuccessorPair);
	}

	void Signal::addSuccessors(vector<pair<Signal*, int>> successorList)
	{
		for(unsigned int i=0; i<successorList.size(); i++)
			addSuccessor(successorList[i].first, successorList[i].second);
	}

	void Signal::removeSuccessor(Signal* successor, int delayCycles)
	{
		//only try to remove the successor if the signal
		//	already exists, within the same instance and with the same delay
		for(int i=0; (unsigned)i<successors_.size(); i++)
		{
			pair<Signal*, int> successorPair = *(this->successorPair(i));
			if((successorPair.first->parentOp()->getName() == successor->parentOp()->getName())
					&& (successorPair.first->getName() == successor->getName())
					&& (successorPair.first->type() == successor->type())
					&& (successorPair.second == delayCycles))
			{
				//delete the element from the list
				successors_.erase(successors_.begin()+i);
				return;
			}
		}

		throw("ERROR in removeSuccessor(): trying to remove a non-existing signal "
				+ successor->getName() + " to the predecessor list");
	}

	void Signal::setSignalParentOp(Operator* newParentOp)
	{
		//erase the signal from the operator's signal list and map
		for(unsigned int i=0; i<(parentOp_->getSignalList()).size(); i++)
			if(parentOp_->getSignalList()[i]->getName() == name_)
				parentOp_->getSignalList().erase(parentOp_->getSignalList().begin()+i);
		parentOp_->getSignalMap().erase(name_);

		//change the signal's parent operator
		setParentOp(newParentOp);

		//add the signal to the new parent's signal list
		parentOp_->signalList_.push_back(this);
		parentOp_->getSignalMap()[name_] = this;
	}

	string Signal::toVHDLType() {
		ostringstream o;

		if ((1==width())&&(!isBus_))
			o << " std_logic" ;
		else
			if(isFP_)
				o << " std_logic_vector(" << wE() <<"+"<<wF() << "+2 downto 0)";
			else if(isFix_){
				o << (isSigned_?" signed":" unsigned") << "(" << MSB_;
				if(LSB_<0)
					o  << "+" << -LSB_;
				else
					o << "-" << LSB_;
				o << " downto 0)";
			}
			else
				o << " std_logic_vector(" << width()-1 << " downto 0)";
		return o.str();
	}



	string Signal::toVHDL() {
		ostringstream o;

		if(type()==Signal::wire || type()==Signal::table
				|| type()==Signal::constantWithDeclaration)
			o << "signal ";
		o << getName();
		o << " : ";

		if (type()==Signal::in)
			o << "in ";
		if(type()==Signal::out)
			o << "out ";

		o << toVHDLType();
		return o.str();
	}



	string Signal::delayedName(int delay){
		ostringstream o;

		o << getName();
		if(delay>0)
			o << "_d" << delay;

		return o.str();
	}


	string Signal::toVHDLDeclaration() {
		ostringstream o;
		o << "signal ";
		if (type_!=Signal::in)
			o << getName() << (lifeSpan_ > 0 ? ", ": "");
		if (lifeSpan_ > 0)
			o << getName() << "_d" << 1;
		for (int i=2; i<=lifeSpan_; i++) {
			o << ", " << getName() << "_d" << i;
		}
		o << " : ";

		o << toVHDLType();
		o << ";";

		if(tableAttributes_ != "")
			o << endl << tableAttributes_;

		return o.str();
	}

	void Signal::setSchedule(int cycle, double criticalPathWithinCycle){
		cycle_ = cycle;
		criticalPath_ = criticalPathWithinCycle;
		hasBeenScheduled_ = true;
	}


	void Signal::setCycle(int cycle) {
		cycle_ = cycle;
	}

	int Signal::getCycle() {
		return cycle_;
	}

	void Signal::updateLifeSpan(int delay) {
		if(delay>lifeSpan_)
			lifeSpan_=delay;
	}

	int Signal::getLifeSpan() {
		return lifeSpan_;
	}

	double Signal::getCriticalPath(){
		return criticalPath_;
	}

	void Signal::setCriticalPath(double cp){
		criticalPath_=cp;
	}

	double Signal::getCriticalPathContribution(){
		return criticalPathContribution_;
	}

	void Signal::setCriticalPathContribution(double contribution){
		criticalPathContribution_ = contribution;
	}


	bool Signal::incompleteDeclaration(){
		return incompleteDeclaration_;
	}

	void Signal::setIncompleteDeclaration(bool newVal){
		incompleteDeclaration_ = newVal;
	}


	bool Signal::hasBeenScheduled(){
		return hasBeenScheduled_;
	}

	void Signal::setHasBeenScheduled(bool newVal){
		hasBeenScheduled_ = newVal;
	}


	bool Signal::unscheduleSignal(){
		bool allPredecessorsConstant = true;

		for(auto j: predecessors_){
			if(j.first->type() != Signal::constant)
			{
				allPredecessorsConstant = false;
				break;
			}
		}

		if((predecessors_.size() == 0) && (type_ != Signal::constant))
			allPredecessorsConstant = false;

		if(allPredecessorsConstant){
			setHasBeenScheduled(true);
		}else{
			setHasBeenScheduled(false);
		}

		return allPredecessorsConstant;
	}


	bool Signal::getHasBeenDrawn(){
		return hasBeenDrawn_;
	}

	void Signal::setHasBeenDrawn(bool newVal){
		hasBeenDrawn_ = newVal;
	}


	void  Signal::setNumberOfPossibleValues(int n){
		numberOfPossibleValues_ = n;
	}


	int  Signal::getNumberOfPossibleValues(){
		return numberOfPossibleValues_;
	}


	std::string Signal::valueToVHDL(mpz_class v, bool quot){
		std::string r;

		/* Get base 2 representation */
		r = v.get_str(2);

		/* Some checks */
		if ((int) r.size() > width())	{
			std::ostringstream o;
			o << "Error in " <<  __FILE__ << "@" << __LINE__ << ": value (" << r << ") is larger than signal " << getName();
			throw o.str();
		}

		/* Do padding */
		while ((int)r.size() < width())
			r = "0" + r;

		/* Put apostrophe / quot */
		if (!quot) return r;
		if ((width() > 1) || ( isBus() ))
			return "\"" + r + "\"";
		else
			return "'" + r + "'";
	}


	std::string Signal::valueToVHDLHex(mpz_class v, bool quot){
		std::string o;

		/* Get base 16 representation */
		o = v.get_str(16);

		/* Some check */
		/* XXX: Too permissive */
		if ((int)o.size() * 4 > width() + 4)	{
			std::ostringstream o;
			o << "Error in " <<  __FILE__ << "@" << __LINE__ << ": value is larger than signal " << getName();
			throw o.str();
		}

		/* Do padding */
		while ((int)o.size() * 4 < width())
			o = "0" + o;

		/* Put apostrophe / quot */
		if (!quot) return o;
		if (width() > 1)
			return "x\"" + o + "\"";
		else
			return "'" + o + "'";
	}


	std::string Signal::getDotNodeColor(int index) {
	  return dotNodeColors[index%dotNodeColors.size()];
	}

}
