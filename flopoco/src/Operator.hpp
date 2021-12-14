#ifndef OPERATOR_HPP
#define OPERATOR_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vector>
#include <set>
#include <map>
#include <memory>
#include <gmpxx.h>
#include <float.h>
#include <utility>

#include "Target.hpp"
#include "Signal.hpp"

#include "TestBenches/TestCase.hpp"

#include "sollya.h"

#include "FlopocoStream.hpp"
#include "utils.hpp"
#include "Tools/ResourceEstimationHelper.hpp"
#include "Tools/FloorplanningHelper.hpp"

using namespace std;

#include "UserInterface.hpp"

namespace flopoco {
	// global const variables
	static const map<string, double> emptyDelayMap;
	const std::string tab = "   ";

	// Reporting levels
#define LIST 0       // information necessary to the user of FloPoCo
#define INFO 1       // information useful to the user of FloPoCo
#define DETAILED 2   // information that shows how the algorithm works, useful to inquisitive users
#define DEBUG 3      // debug info, useful mostly to developers
#define FULL 4       // pure noise

#define INNER_SEPARATOR "................................................................................"
#define DEBUG_SEPARATOR "________________________________________________________________________________"
#define OUTER_SEPARATOR "################################################################################"
#define REPORT(level, stream) {if ((level)<=(UserInterface::verbose)){ cerr << "> " << srcFileName << " " << uniqueName_ <<": " << stream << endl;}else{}}
#define THROWERROR(stream) {{ostringstream o; o << " ERROR in " << uniqueName_ << " (" << srcFileName << "): " << stream << endl; throw o.str();}}


	//Floorplanning - direction of placement constraints
#define ABOVE						0
#define UNDER						1
#define TO_LEFT_OF					2
#define TO_RIGHT_OF					3

#define ABOVE_WITH_EXTRA				4
#define UNDER_WITH_EXTRA				5
#define TO_LEFT_OF_WITH_EXTRA			6
#define TO_RIGHT_OF_WITH_EXTRA			7

	//Floorplanning - constraint type
#define PLACEMENT 					0
#define CONNECTIVITY				1

	/** @brief the format of a fixed-point number
	 *
	 * as described in the developer manual
	 */
	typedef pair<int, int> fdim;

	/**
	 * This is a top-level class representing an Operator.
	 * This class is inherited by all classes which will output a VHDL entity.
	 */
	class Operator
	{



		static int uid;                  /**< The counter holding a unique id */

	public:


		/**
		 * Operator Constructor.
		 * Creates an operator instance with an instantiated target for deployment.
		 * @param target_ The deployment target of the operator.
		 */
		Operator(Operator* parentOp, Target* target);

		/**
		 * Operator Constructor. DEPRECATED, REMOVE ME
		 * Creates an operator instance with an instantiated target for deployment.
		 * @param target_ The deployment target of the operator.
		 */
		Operator(Target* target);


		/**
		 * Operator Destructor.
		 */
		virtual ~Operator() {}




		/**
		 * Add a sub-operator to this operator
		 All the operators in the subCompontentList will be declared in the VHDL.
		 The shared operators are also added to UserInterface::globalOpList
		*/
		void addSubComponent(Operator* op);

		/** Retrieve a sub-operator by its name, NULL if not found */
		OperatorPtr getSubComponent(string name);


		/**
		 * Add this operator to the global (first-level) list, which is stored in its Target (not really its place, sorry).
		 * This method should be called by
		 * 	1/ the main / top-level, or
		 * 	2/ for sub-components that are really basic operators,
		 * 	   expected to be used several times, *in a way that is independent of the context/timing*.
		 * Typical example is a table designed to fit in a LUT or parallel row of LUTs
		 * We assume all the operators added to GlobalOpList are un-pipelined.
		 */
		//	void addToGlobalOpList();

		/**
		 * Add operator @param op to the global (first-level) list, which is stored in its Target.
		 * C.f. version of the method with no parameters for usage and more explanations
		 * @param op the operator to add to the global operator list
		 */
		//	void addToGlobalOpList(Operator *op);

		/**
		 * Apply the schedule computed by schedule() to the VHDL stream.
		 * This is a recursive function that calls itself on all the subcomponents.
		 * The actual work is performed by doApplySchedule()
		 * @param parseType decide whether to perform the first, or the second parse
		 * 		  1=first parse (default), 2=second parse
		 */
		void applySchedule();

		/**
		 * Second level parsing of the VHDL code
		 * This function should not be called before the signals are scheduled
		 *
		 */
		void doApplySchedule();



		/**
		 * Generates the code for a list of operators and all their subcomponents
		 */
		static void outputVHDLToFile(vector<Operator*> &oplist, ofstream& file);


#if 1
		/**
		 * Generates the code for this operator and all its subcomponents
		 */
		void outputVHDLToFile(ofstream& file);
#endif



		/*****************************************************************************/
		/*         Paperwork-related methods (for defining an operator entity)       */
		/*****************************************************************************/

		/**
		 * Adds an input signal to the operator.
		 * 	Adds a signal of type Signal::in to the the I/O signal list.
		 * @param name  the name of the signal
		 * @param width the number of bits of the signal.
		 * @param isBus describes if this signal is a bus, that is, an instance of std_logic_vector
		 */
		void addInput  (const std::string name, const int width, const bool isBus=true);

		/**
		 * Adds an input wire (of type std_logic) to the operator.
		 * 	Adds a signal of type Signal::in to the the I/O signal list.
		 * @param name  the name of the signal
		 */
		void addInput  (const std::string name);

		//	void addInput (const char* name); //ambiguous to addInput(const std::string name) !!!

		/**
		 * Adds  signal to the operator.
		 * 	Adds a signal of type Signal::out to the the I/O signal list.
		 * @param name  the name of the signal
		 * @param width the number of bits of the signal.
		 * @param numberOfPossibleOutputValues (optional, defaults to 1) set to 2 for a faithfully rounded operator for instance
		 * @param isBus describes if this signal is a bus, that is, an instance of std_logic_vector
		 */
		void addOutput(const std::string name, const int width, const int numberOfPossibleOutputValues=1, const bool isBus=true);

		/**
		 * Adds an output wire (of type std_logic) with one possible value to the operator.
		 * 	Adds a signal of type Signal::out to the the I/O signal list.
		 * @param name  the name of the signal
		 */
		void addOutput(const std::string name);

		//	void addOutput(const char* name); //ambiguous to addOutput(const std::string name) !!!

#if 1
		// Test:
		// One option is that fixed-point I/Os should always be plain std_logic_vectors.
		// It just makes the framework simpler, and anyway typing is managed internally
		// FP I/O need to be typed to manage the testbenches, e.g. FP equality does
		// not resume to equality on the bit vectors.
		// This is not the case for fixed-point
		// (comment by F de Dinechin)

		/**
		 * Adds a fixed-point input signal to the operator.
		 * @param name  the name of the signal
		 * @param isSigned  is the signal signed/unsigned
		 * @param msb the most significant bit of the signal's format
		 * @param lsb the least significant bit of the signal's format
		 */
		void addFixInput(const std::string name, const bool isSigned, const int msb, const int lsb);


		/**
		 * Adds a fixed-point output signal to the operator.
		 * @param name  the name of the signal
		 * @param isSigned  is the signal signed/unsigned
		 * @param msb the most significant bit of the signal's format
		 * @param lsb the least significant bit of the signal's format
		 * @param numberOfPossibleOutputValues the number of possible values that the signal can take;
		 * 	useful for testing; related to rounding
		 */
		void addFixOutput(const std::string name, const bool isSigned, const int msb, const int lsb, const int numberOfPossibleOutputValues=1);
#endif

		/**
		 * Adds a floating point (FloPoCo format) input signal to the operator.
		 * 	Adds a signal of type Signal::in to the the I/O signal list,
		 * having the FP flag set on true. The total width of this signal will
		 * be wE + wF + 3. (2 bits for exception, 1 for sign)
		 * @param name the name of the signal
		 * @param wE   the width of the exponent
		 * @param wF   the withh of the fraction
		 */
		void addFPInput(const std::string name, const int wE, const int wF);


		/**
		 * Adds a floating point (FloPoCo format) output signal to the operator.
		 * 	Adds a signal of type Signal::out to the the I/O signal list,
		 * having the FP flag set on true. The total width of this signal will
		 * be wE + wF + 3. (2 bits for exception, 1 for sign)
		 * @param name the name of the signal
		 * @param wE   the width of the exponent
		 * @param wF   the withh of the fraction
		 * @param numberOfPossibleOutputValues (optional, defaults to 1) set to 2 for a faithfully rounded operator for instance
		 */
		void addFPOutput(const std::string name, const int wE, const int wF, const int numberOfPossibleOutputValues=1);

		/**
		 * Adds a IEEE floating point input signal to the operator.
		 * 	Adds a signal of type Signal::in to the the I/O signal list,
		 * having the FP flag set on true. The total width of this signal will
		 * be wE + wF + 1.  (1 bit for sign)
		 * @param name the name of the signal
		 * @param wE   the width of the exponent
		 * @param wF   the withh of the fraction
		 */
		void addIEEEInput(const std::string name, const int wE, const int wF);


		/**
		 * Adds a floating point output signal to the operator.
		 * 	Adds a signal of type Signal::out to the the I/O signal list,
		 * having the FP flag set on true. The total width of this signal will
		 * be wE + wF + 1. (1 bit for sign)
		 * @param name the name of the signal
		 * @param wE   the width of the exponent
		 * @param wF   the withh of the fraction
		 * @param numberOfPossibleOutputValues (optional, defaults to 1) set to 2 for a faithfully rounded operator for instance
		 */
		void addIEEEOutput(const std::string name, const int wE, const int wF, const int numberOfPossibleOutputValues=1);

		/**
		 * Called by addInput(), addOutput(), etc.
		 * Connect a I/O signal that has just been created to the corresponding
		 * signal in the parent operator, extracted from the current tmpIOPortMap.
		 * @param ioSignal the newly created I/O
		 * @param connectionName the name of the signal part of the parent operator that is connected to ioSignal
		 */
		void connectIOFromPortMap(Signal *ioSignal);


		/**
		 * Sets the copyright string: should be authors + year
		 * @param authorsYears the names of the authors and the years of their contributions
		 */
		void setCopyrightString(std::string authorsYears);

		/**
		 * Adds user-defined header information (used, e.g., for primitives that require extra libraries)
		 * May be called multiple times
		 * @param headerString the header information
		 */
		void addAdditionalHeaderInformation(std::string headerString);

		/**
		 * Returns the user-defined header information defined so far
		 * @return the user-defined header information
		 */
		std::string getAdditionalHeaderInformation();

		/**
		 * Use the Synopsys de-facto standard ieee.std_logic_unsigned for this entity
		 */
		void useStdLogicUnsigned();

		/**
		 * Use the Synopsys de-facto standard ieee.std_logic_signed for this entity
		 */
		void useStdLogicSigned();

		/**
		 * Use the real IEEE standard ieee.numeric_std for this entity
		 */
		void useNumericStd();

		/**
		 * Use the real IEEE standard ieee.numeric_std for this entity, also
		 * with support for signed operations on bit vectors
		 */
		void useNumericStd_Signed();

		/**
		 * Use the real IEEE standard ieee.numeric_std for this entity, also
		 * with support for unsigned operations on bit vectors
		 */
		void useNumericStd_Unsigned();

		/**
		 * Return the type of library used for this operator (ieee.xxx)
		 */
		int getStdLibType();

		/**
		 * Sets Operator name to given name, with either the frequency appended, or "comb" for combinatorial.
		 * @param operatorName new name of the operator
		 */
		void setNameWithFreq(std::string operatorName = "UnknownOperator");

		/** Sets Operator name to given name, with either the frequency appended, or "comb" for combinatorial, and a unique identifier for good measure.
		 * @param operatorName new name of the operator
		 */
		void setNameWithFreqAndUID(std::string operatorName = "UnknownOperator");

		/** Sets Operator name to givenName.
		 * Sets the name of the operator to operatorName.
		 * @param operatorName new name of the operator
		 */
		void setName(std::string operatorName = "UnknownOperator");

		/**
		 * This method should be used by an operator to change the default name of a sub-component.
		 * The default name becomes the commented name.
		 * @param operatorName new name of the operator
		 */
		void changeName(std::string operatorName);


		/**
		 * Adds a comment before the entity declaration, along with the copyright string etc.
		 * The "comment" string should include -- at the beginning of each line.
		 */
		void addHeaderComment(std::string comment);

		/**
		 * Returns a string value representing the name of the operator.
		 * @return operator name
		 */
		string getName() const;

		/**
		 * Produces a new unique identifier
		 */
		static int getNewUId();



		/**
		 * Set the parent operator of this operator
		 */
		OperatorPtr setParentOperator(OperatorPtr parentOp);





		/*****************************************************************************/
		/*        VHDL-related methods (for defining an operator architecture)       */
		/*****************************************************************************/

		/**
		 * Functions related to pipeline management
		 */

		/**
		 * Return the critical path delay associated to a given output of the operator
		 * @param the name of the output
		 */
		double getOutputDelay(string s);

		/**
		 *@param[in] inputList the list of input signals
		 *@return the maximum delay of the inputs list
		 */
		//double getMaxInputDelays(vector<Signal*> inputList);


		/**
		 * Return the cycle of the signal specified by @param name
		 * @param name the name of the signal
		 */
		int getCycleFromSignal(string name, bool report = false);

		/**
		 * Return the critical path of the signal specified by @param name
		 * @param name the name of the signal
		 */
		double getCPFromSignal(string name, bool report = false);

		/**
		 * Return the contribution to the critical path of the signal specified by @param name
		 * @param name the name of the signal
		 */
		double getCPContributionFromSignal(string name, bool report = false);


		/**
		 * Functions modifying Signals
		 * These methods belong to the Signal class. Unfortunately, having them in the Signal class
		 * creates a circular dependency, so they are now in Operators, seeing as this is the only place
		 * where they are used.
		 */



		/**
		 * Declares a signal appearing on the Left Hand Side of a VHDL assignment
		 * @param name is the name of the signal
		 * @param width is the width of the signal (optional, default 1; value -1 means: incomplete declaration, see Signal.hpp)
		 * @param isbus: a signal of width 1 is declared as std_logic when false,
		 * 				 as std_logic_vector when true (optional, default false)
		 * @param regType: the registring type of this signal. See also the Signal
		 * 				   Class for more info
		 * @param incomplete declaration: whether only part of the signal parameters are specified,
		 * 									and the rest are specified later
		 * @return name
		 */
		string declare(string name, const int width, bool isbus=true, Signal::SignalType regType = Signal::wire);

		/**
		 * Declares a signal appearing on the Left Hand Side of a VHDL assignment
		 * @param criticalPathContribution is the delay that the signal being declared adds to the critical path
		 * @param name is the name of the signal
		 * @param width is the width of the signal (optional, default 1)
		 * @param isbus: a signal of width 1 is declared as std_logic when false, as std_logic_vector when true (optional, default false)
		 * @param regType: the registring type of this signal. See also the Signal Class for more info
		 * @param criticalPathContribution: the delay that the signal adds to the critical path of the circuit
		 * @param incomplete declaration: whether only part of the signal parameters are specified,
		 * 									and the rest are specified later
		 * @return name
		 */
		string declare(double criticalPathContribution, string name, const int width, bool isbus=true, Signal::SignalType regType = Signal::wire);

		/**
		 * Declares a signal of length 1 as in the previous declare() function, but as std_logic by default
		 * @param name is the name of the signal
		 * @param isbus: if true, declares the signal as std_logic_vector; else declares the signal as std_logic
		 * @param regType: the registring type of this signal. See also the Signal Class for more info
		 * @param incomplete declaration: whether only part of the signal parameters are specified,
		 * 									and the rest are specified later
		 * @return name
		 */
		string declare(string name, Signal::SignalType regType = Signal::wire);

		/**
		 * Declares a signal of length 1 as in the previous declare() function, but as std_logic by default
		 * @param criticalPathContribution is the delay that the signal being declared adds to the critical path
		 * @param name is the name of the signal
		 * @param isbus: if true, declares the signal as std_logic_vector; else declares the signal as std_logic
		 * @param regType: the registring type of this signal. See also the Signal Class for mor info
		 * @param criticalPathContribution: the delay that the signal adds to the critical path of the circuit
		 * @param incomplete declaration: whether only part of the signal parameters are specified,
		 * 									and the rest are specified later
		 * @return name
		 */
		string declare(double criticalPathContribution, string name, Signal::SignalType regType = Signal::wire);


		/**
		 * Declares a fixed-point signal on the Left Hand Side of a VHDL assignment
		 * @param name is the name of the signal
		 * @param isSigned whether the signal is signed, or not
		 * @param MSB the weight of the MSB of the signal
		 * @param LSB the weight of the LSB of the signal
		 * @param regType: the registring type of this signal. See also the Signal Class for more info
		 * @param incomplete declaration: whether only part of the signal parameters are specified,
		 * 									and the rest are specified later
		 * @return name
		 */
		string declareFixPoint(string name, const bool isSigned, const int MSB, const int LSB, Signal::SignalType regType = Signal::wire);

		/**
		 * Declares a fixed-point signal on the Left Hand Side of a VHDL assignment
		 * @param name is the name of the signal
		 * @param isSigned whether the signal is signed, or not
		 * @param MSB the weight of the MSB of the signal
		 * @param LSB the weight of the LSB of the signal
		 * @param regType: the registring type of this signal. See also the Signal Class for more info
		 * @param criticalPathContribution: the delay that the signal adds to the critical path of the circuit
		 * @param incomplete declaration: whether only part of the signal parameters are specified,
		 * 									and the rest are specified later
		 * @return name
		 */
		string declareFixPoint(double criticalPathContribution, string name, const bool isSigned, const int MSB, const int LSB, Signal::SignalType regType = Signal::wire);


		/**
		 * Declares a floating-point signal on the Left Hand Side of a VHDL assignment
		 * @param name is the name of the signal
		 * @param wE the weight of the exponent of the signal
		 * @param wF the weight of the mantisa of the signal
		 * @param regType: the registring type of this signal. See also the Signal Class for more info
		 * @param incomplete declaration: whether only part of the signal parameters are specified,
		 * 									and the rest are specified later
		 * @return name
		 */
		string declareFloatingPoint(string name, const int wE, const int wF, Signal::SignalType regType = Signal::wire, const bool ieeeFormat=false);

		/**
		 * Declares a floating-point signal on the Left Hand Side of a VHDL assignment
		 * @param name is the name of the signal
		 * @param wE the weight of the exponent of the signal
		 * @param wF the weight of the mantisa of the signal
		 * @param regType: the registring type of this signal. See also the Signal Class for more info
		 * @param criticalPathContribution: the delay that the signal adds to the critical path of the circuit
		 * @param incomplete declaration: whether only part of the signal parameters are specified,
		 * 									and the rest are specified later
		 * @return name
		 */
		string declareFloatingPoint(double criticalPathContribution, string name, const int wE, const int wF, Signal::SignalType regType = Signal::wire, const bool ieeeFormat=false);

		/**
		 * Initialize a newly declared signal.
		 * Method used to share code between the declare functions
		 * TODO should probably be refactored in Signakl
		 * @param s the newly declared signal
		 * @param criticalPathContribution: the delay that the signal adds to the critical path of the circuit
		 * @param regType: the registring type of this signal. See also the Signal Class for more info
		 * @param incomplete declaration: whether only part of the signal parameters are specified,
		 * 									and the rest are specified later
		 */
		void initNewSignal(Signal *s, double criticalPathContribution, Signal::SignalType regType, bool incompleteDeclaration = false);

		/**
		 * Resizes a fixed-point signal and assigns it to a new declared signal.
		 * 	May zero-extend, sign-extend, or truncate.
		 * 	Warns at debug level when truncating LSBs, and warns a bit louder when truncating MSBs.
		 * @param lhsName is the name of the new (resized) signal
		 * @param rhsName is the name of the old (to be resized) signal
		 * @return name
		 */
		void resizeFixPoint(string lhsName, string rhsName, const int MSB, const int LSB, const int indentLevel=1);



		/**
		 * addRegisteredSignalCopy is _the_ interface to have functional delays,
		 (e.g. the z^(-1) function used in signal processing)
		 irrespective of what happens during pipelining.
		 It registers sourceName and creates a registerd copy (which is declare()d.
		 * The inserted register is a functional delay ,
		 * @param registeredCopyName as the name suggests
		 * @param sourceName  the signal to register
		 * @param sigType the type of delay inserted (with or without reset, etc...), defaults to usual pipeline register without reset
		 */
		void  addRegisteredSignalCopy(string registeredCopyName, string sourceName, Signal::ResetType regType=Signal::noReset, int pp_depth=1);

		/**
		 * Disables pipeline locally. All the delays passed to declare() will be ignored until the next invokation of  enablePipelining();
		 */
		void disablePipelining();

		/**
		 * Enables pipeline locally. All the delays passed to declare() will be ignored until the next invokation of  enablePipelining();
		 */
		void enablePipelining();

		// TODO: add methods that allow for signals with reset (when rewriting FPLargeAcc for the new framework)

		/**
		 * setGeneric Sets the specific generic
		 * @param name Name of the generic to set
		 * @param value Value of the generic
		 */
		void setGeneric(std::string name, string value, int width, bool isBus=false);

		/**
		 * setGeneric Sets the specific generic
		 * @param name Name of the generic to set
		 * @param value Value of the generic
		 */
		void setGeneric(string name, const long value, int width, bool isBus=false);


		/**
		 * Declare an output mapping for an instance of a sub-component
		 * @param componentPortName is the name of the port on the component
		 * @param actualSignalName is the name of the signal in This mapped to this port
		 * @return name
		 */
		void outPortMap(string componentPortName, string actualSignalName);

		/** Obsolete method, because refactoring takes ages */
		void outPortMap(OperatorPtr op, string componentPortName, string actualSignalName);


		/**
		 * Use a signal as input of a subcomponent
		 * @param componentPortName is the name of the port on the component
		 * @param actualSignalName is the name of the signal (of this) mapped to this port
		 */
		void inPortMap(string componentPortName, string actualSignalName);

		/** Obsolete method, because refactoring takes ages */
		void inPortMap(OperatorPtr op, string componentPortName, string actualSignalName);

		/**
		 * Use a constant signal as input of a subcomponent.
		 * @param componentPortName is the name of the port on the component
		 * @param constantValue is the constant value to be mapped to this port
		 */
		void inPortMapCst(string componentPortName, string constantValue);

		/** Obsolete method, because refactoring takes ages */
		void inPortMapCst(OperatorPtr op, string componentPortName, string constantValue);


		/**
		 * Returns the VHDL for an instance of a sub-component.
		 * @param op represents the operator to be port mapped
		 * @param instanceName is the name of the instance as a label
		 * @param outputWarning is a flag, to be removed eventually, that warns that this method shouldn't be called directly from constructor code
		 * @return name
		 */
		string instance(Operator* op, string instanceName, bool outputWarning=true);

		/**
		 * Create a new instance of an operator inside the current operator
		 * @param opName the type of operator being instantiated
		 * @param instanceName the name of the instance being created (label compulsory in VHDL)
		 * @param parameters the parameters given to the constructor of the instance's operator
		 * 				specified as a string containing 'paramName=paramValue' separated by spaces (as on the FloPoCo command line)
		 * @param inPortMaps the port mappings for the inputs
		 * 				specified as a string containing 'portName=>signalName' separated by ',' (as on VHDL port maps)
		 * @param outPortMaps the port mappings for the outputs
		 * 				specified as a string containing 'portName=>signalName' separated by ','(as on VHDL port maps)
		 * @param inPortMapsCst the constant port mappings for the inputs, if there are any
		 * 				specified as a string containing 'portName=>signalName' separated by ','(as on VHDL port maps)
		 */
		OperatorPtr newInstance(string opName, string instanceName, string parameters, string inPortMaps, string outPortMaps, string inPortMapsCst = "");

		/**
		 * Create a new instance of a shared operator that has been constructed beforehand.inside
		 * @param op a pointer to the shared operator
		 * @param instanceName the name of the instance being created (label compulsory in VHDL)
		 * @param inPortMaps the port mappings for the inputs
		 * 				specified as a string containing 'portName=>signalName' separated by ',' (as on VHDL port maps)
		 * @param outPortMaps the port mappings for the outputs
		 * 				specified as a string containing 'portName=>signalName' separated by ','(as on VHDL port maps)
		 * @param inPortMapsCst the constant port mappings for the inputs, if there are any
		 * 				specified as a string containing 'portName=>signalName' separated by ','(as on VHDL port maps)
		 */
		void newSharedInstance(OperatorPtr op, string instanceName, string inPortMaps, string outPortMaps, string inPortMapsCst = "");

	private:
		/**
		 * Parse a string containing port mappings for a new instance of an operator
		 * and add the corresponding port mappings to the parent operator.
		 * The port mappings are of the form "poarName:connectedSignalName:..."
		 * @param instance the operator to which the port mappings are performed
		 * @portMappings a list of port-connected signal
		 * @param portTypes the type of port being added (0=input, 1=constant inputs, 2=output)
		 */
		void parsePortMappings(string portMappings, int portTypes);

	public:
		/**
		 * Adds attributes to the generated VHDL so that the tools use embedded RAM blocks for an instance
		 * @param t a pointer to this instance
		 */
		void useHardRAM(Operator* t);

		/**
		 * Adds attributes to the generated VHDL so that the tools use LUT-based RAM for an instance
		 * @param t a pointer to this instance
		 */
		void useSoftRAM(Operator* t);



		/**
		 * Define the architecture name for this operator (by default : arch)
		 *	@param[in] 	architectureName		- new name for the operator architecture
		 */
		void setArchitectureName(string architectureName);

		/**
		 * A new architecture inline function
		 * @param[in,out] o 	- the stream to which the new architecture line will be added
		 * @param[in]     name	- the name of the entity corresponding to this architecture
		 **/
		void newArchitecture(std::ostream& o, std::string name);

		/**
		 * A begin architecture inline function
		 * @param[in,out] o 	- the stream to which the begin line will be added
		 **/
		void beginArchitecture(std::ostream& o);

		/**
		 * A end architecture inline function
		 * @param[in,out] o 	- the stream to which the begin line will be added
		 **/
		void endArchitecture(std::ostream& o);





		/*****************************************************************************/
		/*        Testing-related methods (for defining an operator testbench)       */
		/*****************************************************************************/

		/**
		 * Gets the correct value associated to one or more inputs.
		 * @param tc the test case, filled with the input values, to be filled with the output values.
		 * @see FPAdd for an example implementation
		 */
		virtual void emulate(TestCase * tc);

		/**
		 * Append standard test cases to a test case list. Standard test
		 * cases are operator-dependent and should include any specific
		 * corner cases you may think of. Never mind removing a standard test case because you think it is no longer useful!
		 * @param tcl a TestCaseList
		 */
		virtual void buildStandardTestCases(TestCaseList* tcl);


		/**
		 * Generate Random Test case identified by an integer . There is a default
		 * implementation using a uniform random generator, but most
		 * operators are not exercised efficiently using such a
		 * generator. For instance, in FPAdd, the random number generator
		 * should be biased to favor exponents which are relatively close
		 * so that an effective addition takes place.
		 * This function create a new TestCase (to be free after use)
		 * See FPExp.cpp for an example of overloading this method.
		 * @param i the identifier of the test case to be generated
		 * @return TestCase*
		 */
		virtual TestCase* buildRandomTestCase(int i);





		/*****************************************************************************/
		/*     From this point, we have methods that are not needed in normal use    */
		/*****************************************************************************/

		/**
		   Only for very specific operators such as TestBench or Wrapper for which we just want the VHDL to get through untouched
		*/
		void setNoParseNoSchedule();
		bool noParseNoSchedule();


		/**
		 * Append random test cases to a test case list. There is a default
		 * implementation using a uniform random generator, but most
		 * operators are not exercised efficiently using such a
		 * generator. For instance, in FPAdd, the random number generator
		 * should be biased to favor exponents which are relatively close
		 * so that an effective addition takes place.
		 * In most cases you do need to overload this method,
		 * but simply overload  buildRandomTestCase(int i)
		 * which is called by the default implementation of buildRandomTestCaseList
		 * @param tcl a TestCaseList
		 * @param n the number of random test cases to add
		 */
		virtual void buildRandomTestCaseList(TestCaseList* tcl, int n);





		/**
		 * Build all the signal declarations from signals implicitly declared by declare().
		 */
		virtual string buildVHDLSignalDeclarations();

		/**
		 * Build all the component declarations from the list of components built by instance().
		 */
		string buildVHDLComponentDeclarations();

		/**
		 * Build all the registers from signals implicitly delayed by declare()
		 *	This is the 2.0 equivalent of outputVHDLSignalRegisters
		 */
		string buildVHDLRegisters();

		/**
		 * Build all the type declarations.
		 */
		string buildVHDLTypeDeclarations();

		/**
		 * Output the VHDL constants.
		 */
		string buildVHDLConstantDeclarations();

		/**
		 * Output the VHDL constants.
		 */
		string buildVHDLAttributes();





		/**
		 * The main function that outputs the VHDL for the operator.
		 * If you use the modern (post-0.10) framework you no longer need to overload this method,
		 * the default will do.
		 * @param o the stream where the entity will be output
		 * @param name the name of the architecture
		 */
		virtual void outputVHDL(std::ostream& o, std::string name);

		/**
		 * The main function outputs the VHDL for the operator.
		 * Calls the two parameter version, with name = uniqueName
		 * @param o the stream where the entity will be output
		 */
		void outputVHDL(std::ostream& o);



		/**
		 * Returns true if the operator needs a clock signal;
		 */
		bool isSequential();

		/**
		 * Set the operator to sequential.
		 * You shouldn't need to use this method for standard operators
		 */
		void setSequential();

		/**
		 * Set the operator to combinatorial
		 * You shouldn't need to use this method for standard operators
		 */
		void setCombinatorial();


		/**
		 * Indicates that it is not a warning if there is feedback of one cycle, but it
		 * is an error if a feedback of more than one cycle happens.
		 */
		void setHasDelay1Feedbacks();


		/**
		 * Indicates that it is not a warning if there is feedback of one cycle, but it
		 * is an error if a feedback of more than one cycle happens.
		 */
		bool hasDelay1Feedbacks();



		/**
		 * Returns a pointer to the signal having the name as @param s.
		 * Throws an exception if the signal is not yet declared.
		 * @param name the name of the signal we want to return
		 * @return the pointer to the signal having name s
		 */
		Signal* getSignalByName(string name);

		/**
		 * Return the list of signals declared in this operator
		 */
		vector<Signal*> getSignalList();

		/**
		 * Checks if a signal is already declared
		 * Possible range specifier are ignored
		 * @param name the name of the signal we want to check
		 * @return true, if signal exists, false otherwise
		 */
		bool isSignalDeclared(string name);

		/**
		 * Checks if the operator has already been converted to dot
		 */


		/**
		 * Return the list of component instances declared in this operator
		 */
		//vector<Instance*> getInstances();


		/** DEPRECATED
		 * Outputs component declaration
		 * @param o the stream where the component is outputed
		 * @param name the name of the VHDL component we want to output to o
		 */
		virtual void outputVHDLComponent(std::ostream& o, std::string name);

		/** DEPRECATED
		 * Outputs the VHDL component code of the current operator
		 * @param o the stream where the component is outputed
		 */
		void outputVHDLComponent(std::ostream& o);



		/**
		 * Return the number of input+output signals
		 * @return the size of the IO list. The total number of input and output signals
		 *         of the architecture.
		 */
		int getIOListSize() const;

		/**
		 * Returns a pointer to the list containing the IO signals.
		 * @return pointer to ioList
		 */
		vector<Signal*> * getIOList();

		/**
		 * Passes the IOList by value.
		 * @return the ioList
		 */
		vector<Signal*> getIOListV(){
			return ioList_;
		}


		/**
		 * Returns a pointer a signal from the ioList.
		 * @param the index of the signal in the list
		 * @return pointer to the i'th signal of ioList
		 */
		Signal * getIOListSignal(int i);





		/** DEPRECATED, better use setCopyrightString
		 * Output the licence
		 * @param o the stream where the licence is going to be outputted
		 * @param authorsYears the names of the authors and the years of their contributions
		 */
		void licence(std::ostream& o, std::string authorsYears);


		/**
		 * Output the licence, using copyrightString_
		 * @param o the stream where the licence is going to be outputted
		 */
		void licence(std::ostream& o);


		void pipelineInfo(std::ostream& o, std::string authorsYears);

		void signalSignature(std::ostream& o);

		void pipelineInfo(std::ostream& o);

		/**
		 * Output the standard library paperwork
		 * @param o the stream where the libraries will be written to
		 */
		void stdLibs(std::ostream& o);


		/**
		 * Output user defined header information, using additionalHeaderString_
		 * @param o the stream where the header is going to be outputted
		 */
		void additionalHeader(std::ostream& o);

		/** DEPRECATED
		 * Output the VHDL entity of the current operator.
		 * @param o the stream where the entity will be outputted
		 */
		void outputVHDLEntity(std::ostream& o);

		/** DEPRECATED
		 * Output all the signal declarations
		 * @param o the stream where the signal deca
		 */
		void outputVHDLSignalDeclarations(std::ostream& o);


		/**
		 * Add a VHDL type declaration.
		 */
		void addType(std::string name, std::string def);

		/**
		 * Add a VHDL constant. This may make the code easier to read, but more difficult to debug.
		 */
		void addConstant(std::string name, std::string ctype, int cvalue);

		void addConstant(std::string name, std::string ctype, mpz_class cvalue);

		void addConstant(std::string name, std::string ctype, string cvalue);


		/**
		 * Add an attribute, declaring the attribute's name if it is not done already.
		 */
		void addAttribute(std::string attributeName,  std::string attributeType,  std::string object, std::string value, bool addSignal=false );

		/**
		 * A new line inline function
		 * @param[in,out] o the stream to which the new line will be added
		 **/
		inline void newLine(std::ostream& o) {	o<<endl; }



		/**
		 * Final report function, prints to the terminal.
		 * By default, reports the pipeline depth, but feel free to overload
		 * it if you have anything useful to tell to the end user
		 */
		virtual void outputFinalReport(ostream& s, int level);


		/**
		 * Returns the pipeline depth of this operator
		 * @return the pipeline depth of the operator
		 */
		virtual int getPipelineDepth();

		/**
		 * Computes pipeline depth after scheduling, for this operator and all its subcomponents
		 */
		void computePipelineDepths();
		/**
		 * Return the target member
		 */
		Target* getTarget();

		/**
		 * Return the target member
		 */
		OperatorPtr getParentOp();

		/**
		 * Return the operator's unique name
		 */
		string getUniqueName();

		/**
		 * Return the architecture name
		 */
		string getArchitectureName();

		vector<Signal*> getTestCaseSignals();

		string getSrcFileName();

		int getOperatorCost();

		map<string, Signal*> getSignalMap();

		map<string, pair<string, string> > getConstants();

		map<string, string> getAttributes();

		map<string, string> getTypes();

		map<pair<string,string>, string> getAttributesValues();

		bool getHasRegistersWithoutReset();

		bool getHasRegistersWithAsyncReset();

		bool getHasRegistersWithSyncReset();

		bool hasReset();

		bool hasClockEnable();

		void setClockEnable(bool val);

		string getCopyrightString();

		Operator* getIndirectOperator();

		void setIndirectOperator(Operator* op);

		vector<Operator*> getSubComponentList();

		vector<Operator*>& getSubComponentListR();


		bool hasComponent(string s);

		void cleanup(vector<Operator*> *ol, Operator* op);

		FlopocoStream* getFlopocoVHDLStream();


		/**
		 * Return the pipeline delay, in cycles, between two signals.
		 * @param rhsSignal the signal on the right-hand side of the assignment
		 * @param lhsSignal the signal on the left-hand side of the assignment
		 */
		int getPipelineDelay(Signal *rhsSignal, Signal *lhsSignal);

		/**
		 * Return the delay (pipeline or functional), in cycles, between two signals.
		 * @param rhsSignal the signal on the right-hand side of the assignment
		 * @param lhsSignal the signal on the left-hand side of the assignment
		 */
		int getDelay(Signal *rhsSignal, Signal *lhsSignal);


		/**
		 * Extract the timing dependences between signals.
		 * The raw data is stored in the vhdl FlopocoStream object, in the form of
		 * triplets, storing ("lhs_signal_name", "rhs_signal_name", delay).
		 *
		 * WARNING: This function should only be called after the vhdl code
		 * 			has been parsed (the first parse)
		 */
		void moveDependenciesToSignalGraph();

		/* auxillary recursive function */
		void buildAlreadyScheduledList(set<Signal*> & alreadyScheduled);

		/**
		 * Performs as much as possible of an ASAP scheduling for the root operator of this operator.
		 */
		void schedule();

		/**
		 * Set the timing of a signal.
		 * Used also to share code between the different timing methods.
		 * @param targetSignal the signal to be scheduled
		 */
		void setSignalTiming(Signal* targetSignal);


		/**
		 * Start drawing the dot diagram for this Operator
		 * @param file the file to which to output the dot drawing
		 * @param mode the drawing mode, dependent on whether this is a global operator, or not
		 * 		mode=1 global operator, mode=2 sub-component
		 * @param dotDrawingMode the dot drawing options
		 * 		dotDrawingMode=full the fully flattened tree, dotDrawingMode=compact flattened, but with reduced viwe of the subcomponents
		 */
		void drawDotDiagram(ofstream& file, int mode, std::string dotDrawingMode, std::string tabs = "");

		/**
		 * Outputs the dot code used for drawing this signal as a graph node
		 * @param node the node to draw
		 */
		std::string drawDotNode(Signal *node, std::string tabs = "");

		/**
		 * Outputs the dot code used for drawing this signal's connections as graph edges
		 * @param node the node who's connections are to be drawn
		 */
		std::string drawDotNodeEdges(Signal *node, std::string tabs = "");

		/**
		 * Outputs the dot code used for drawing the edge between @param source and @param sink
		 * @param source the source node for the edge
		 * @param sink the sink node for the edge
		 */
		std::string drawDotEdge(Signal *source, Signal *sink, std::string tabs = "");


		void setuid(int mm);

		int getuid();



		string signExtend(string name, int w);

		string zeroExtend(string name, int w);

		int level; //printing issues




		/**
		 * Add a comment line in to vhdl stream
		 */
		void addComment(string comment, string align = tab);

		/**
		 * Add a full line of '-' with comment centered within
		 */
		void addFullComment(string comment, int lineLength = 80);


		/**
		 * Completely replace "this" with a copy of another operator.
		 */
		void cloneOperator(Operator *op);

		/**
		 * Create a deep copy of the operator op, changing also the corresponding
		 * internal references.
		 */
		void deepCloneOperator(Operator *op);


		/**
		 * Get the value of isOperatorImplemented
		 */
		bool isOperatorImplemented();

		/**
		 * Set the value of isOperatorImplemented
		 */
		void setIsOperatorImplemented(bool newValue);

		/**
		 * Get the value of isOperatorScheduled
		 */
		bool isOperatorScheduled();

		/**
		 * Set the value of isOperatorScheduled
		 */
		void setIsOperatorScheduled(bool newValue);

		/**
		 * Set the operator to be reused in the design
		 */
		void setShared();

		/**
		 * Is this operator a unique component?
		 */
		bool isUnique();

		/**
		 * Is this operator a shared component?
		 */
		bool isShared();

		/**
		 * Sets this operator to a library component. Typically a wrapper to components like primitives
		 */
		void setLibraryComponent();

		/**
		 * Is this operator just a wrapper to a library component (like primitives)?
		 */
		bool isLibraryComponent();

		/**
		 * Has the dot file having this operator as top level one been produced
		 */
		bool isOperatorDrawn();

		void markOperatorDrawn();

		/**
		 * Getter for generics (needed for deep copies)
		 */
		map<std::string, std::string> getGenerics();

		/////////////////////////////////////////////////////////////////////////////////////////////////
		////////////Functions used for resource estimations

		//--Logging functions

		/**
		 * Add @count flip-flops to the total estimate
		 * @param count (by default 1) the number of elements to add
		 * @return the string describing the performed operation
		 */
		std::string addFF(int count = 1);

		/**
		 * Add @count registers to increase the total flip-flop estimate
		 * from the register characteristics
		 * @param count (by default 1) the number of registers to add
		 * @param width the width of each register
		 * @return the string describing the performed operation
		 */
		std::string addReg(int width, int count = 1);

		/**
		 * Add @count function generators to the total estimate
		 * Suggest Look-Up Table type (based on number of inputs), in order
		 * to obtain more accurate predictions
		 * @param count (by default 1) the number of elements to add
		 * @param nrInputs number of inputs of the LUT (0 for default option
		 * of target technology)
		 * @return the string describing the performed operation
		 */
		std::string addLUT(int nrInputs = 0, int count = 1);

		/**
		 * Add @count multipliers to the total estimate
		 * NOTE: also increases the DSP count
		 * @param count (by default 1) the number of elements to add
		 * @return the string describing the performed operation
		 */
		std::string addMultiplier(int count = 1);

		/**
		 * Add @count multipliers each having inputs of bitwidths @widthX and
		 * @widthY, respectively
		 * The user can also chose to what degree the multipliers are
		 * implemented in logic (a number between 0 and 1)
		 * NOTE: also increases the DSP count
		 * @param count (by default 1) the number of elements to add
		 * @param width the bitwidth of the multipliers
		 * @param ratio (by default 1) the ratio to which the multipliers
		 * are implemented in logic (0 for 0%, 1 for 100%)
		 * @return the string describing the performed operation
		 */
		std::string addMultiplier(int widthX, int widthY, double ratio = 1, int count = 1);

		/**
		 * Add @count adders/subtracters each having inputs of bitwidths @widthX and
		 * @widthY, respectively
		 * The user can also chose to what degree the adders/subtracters are
		 * implemented in logic (a number between 0 and 1)
		 * NOTE: can also increase the DSP count
		 * @param count (by default 1) the number of elements to add
		 * @param width the bitwidth of the multipliers
		 * @param ratio (by default 0) the ratio to which the multipliers
		 * are implemented in logic (0 for 0%, 1 for 100%)
		 * @return the string describing the performed operation
		 */
		std::string addAdderSubtracter(int widthX, int widthY, double ratio = 0, int count = 1);

		/**
		 * Add @count memories to the total estimate, each having @size
		 * words of @width bits
		 * The memories can be either RAM or ROM, depending on the value of
		 * the @type parameter
		 * NOTE: Defaults to adding RAM memories
		 * @param count (by default 1) the number of elements to add
		 * @param size the number of words of the memory
		 * @param width the bitwidth of each of the memory's word
		 * @param type (by default 0) the type of the memory
		 * (0 for RAM, 1 for ROM)
		 * @return the string describing the performed operation
		 */
		std::string addMemory(int size, int width, int type = 0, int count = 1);

		//---More particular resource logging
		/**
		 * Add @count DSP(s) to the total estimate
		 * @param count (by default 1) the number of elements to add
	 * @return the string describing the performed operation
	 */
	std::string addDSP(int count = 1);

	/**
	 * Add @count RAM(s) to the total estimate
	 * NOTE: For a more precise description of the memory being added, use the
	 * @addMemory() function with the corresponding parameters
	 * NOTE: adds memories with the default widths and sizes
	 * @param count (by default 1) the number of elements to add
	 * @return the string describing the performed operation
	 */
	std::string addRAM(int count = 1);

	/**
	 * Add @count ROM(s) to the total estimate
	 * NOTE: For a more precise description of the memory being added, use the
	 * @addMemory() function with the corresponding parameters
	 * NOTE: adds memories with the default widths and sizes
	 * @param count (by default 1) the number of elements to add
	 * @return the string describing the performed operation
	 */
	std::string addROM(int count = 1);

	/**
	 * Add @count Shift Registers to the total estimate, each having a
	 * bitwidth of @width bits
	 * NOTE: this function also modifies the total number of LUTs and FFs
	 * in the design; this aspect should be considered so as not to result
	 * in counting the resources multiple times and overestimate
	 * @param count (by default 1) the number of elements to add
	 * @param width the bitwidth of the registers
	 * @param depth the depth of the shift register
	 * @return the string describing the performed operation
	 */
	std::string addSRL(int width, int depth, int count = 1);

	/**
	 * Add @count wire elements to the total estimate
	 * The estimation can be done in conjunction with the declaration of a
	 * certain signal, in which specify the signal's name is specified
	 * through the @signalName parameter
	 * NOTE: it is not advised to use the function without specifying
	 * the signal's name, as it results in duplication of resource count
	 * NOTE: if @signalName is provided, @count can be omitted, as it
	 * serves no purpose
	 * @param count (by default 1) the number of elements to add
	 * @param signalName (by default the empty string) the name of the
	 * corresponding signal
	 * @return the string describing the performed operation
	 */
	std::string addWire(int count = 1, std::string signalName = "");

	/**
	 * Add @count I/O ports to the total estimate
	 * The estimation can be done in conjunction with the declaration
	 * of a certain port, in which specify the port's name is specified
	 * through the @portName parameter
	 * NOTE: it is not advised to use the function without specifying
	 * the port's name, as it results in duplication of resource count
	 * NOTE: if @portName is provided, @count can be omitted, as it
	 * serves no purpose
	 * @param count (by default 1) the number of elements to add
	 * @param portName (by default the empty string) the name of the
	 * corresponding port
	 * @return the string describing the performed operation
	 */
	std::string addIOB(int count = 1, std::string portName = "");

	//---Even more particular resource logging
	/**
	 * Add @count multiplexers to the total estimate, each having
	 * @nrInputs inputs of @width bitwidths
	 * NOTE: this function also modifies the total number of LUTs in
	 * the design; this aspect should be considered so as not to result
	 * in counting the resources multiple times and overestimate
	 * @param count (by default 1) the number of elements to add
	 * @param nrInputs (by default 2) the number of inputs to the MUX
	 * @param width the bitwidth of the inputs and the output
	 * @return the string describing the performed operation
	 */
	std::string addMux(int width, int nrInputs = 2, int count = 1);

	/**
	 * Add @count counters to the total estimate, each having
	 * @width bitwidth
	 * NOTE: this function also modifies the total number of LUTs and
	 * FFs in the design; this aspect should be considered so as not to
	 * result in counting the resources multiple times and overestimate
	 * @param count (by default 1) the number of elements to add
	 * @param width the bitwidth of the counter
	 * @return the string describing the performed operation
	 */
	std::string addCounter(int width, int count = 1);

	/**
	 * Add @count accumulators to the total estimate, each having
	 * @width bitwidth
	 * NOTE: this function also modifies the total number of LUTs and
	 * FFs and DSPs in the design; this aspect should be considered so
	 * as not to result in counting the resources multiple times and
	 * overestimate
	 * @param count (by default 1) the number of elements to add
	 * @param width the bitwidth of the accumulator
	 * @param useDSP (by default false) whether the use of DSPs is allowed
	 * @return the string describing the performed operation
	 */
	std::string addAccumulator(int width, bool useDSP = false, int count = 1);

	/**
	 * Add @count decoder to the total estimate, each decoding an input
	 * signal of wIn bits to an output signal of wOut bits
	 * NOTE: this function also modifies the total number of LUTs and
	 * FFs and RAMs in the design; this aspect should be considered so
	 * as not to result in counting the resources multiple times and
	 * overestimate
	 * @param count (by default 1) the number of elements to add
	 * @return the string describing the performed operation
	 */
	std::string addDecoder(int wIn, int wOut, int count = 1);

	/**
	 * Add @count arithmetic operator to the total estimate, each having
	 * @nrInputs of @width bitwidths
	 * NOTE: this function also modifies the total number of LUTs in
	 * the design; this aspect should be considered so as not to result
	 * in counting the resources multiple times and overestimate
	 * @param count (by default 1) the number of elements to add
	 * @param nrInputs (by default 2) the number of inputs of the gate
	 * @param width the bitwidth of the inputs
	 * @return the string describing the performed operation
	 */
	std::string addArithOp(int width, int nrInputs = 2, int count = 1);

	/**
	 * Add @count Finite State Machine to the total estimate, each
	 * having @nrStates states, @nrTransitions transitions
	 * NOTE: this function also modifies the total number of LUTs and
	 * FFs and ROMs in the design; this aspect should be considered so
	 * as not to result in counting the resources multiple times and
	 * overestimate
	 * @param count (by default 1) the number of elements to add
	 * @param nrStates the number of states of the FSM
	 * @param nrTransitions (by default 0) the number of transitions of
	 * the FSM
	 * @return the string describing the performed operation
	 */
	std::string addFSM(int nrStates, int nrTransitions = 0, int count = 1);

	//--Resource usage statistics
	/**
	 * Generate statistics regarding resource utilization in the design,
	 * based on the user's approximations
	 * @param detailLevel (by default 0, basic resource estimations)
	 * the level of detail to which the resource utilizations are
	 * reported (0 - basic report; 1 - include the more specific
	 * resources; 2 - include all statistics)
	 * @return a formatted string containing the statistics
	 */
	std::string generateStatistics(int detailLevel = 0);

	//--Utility functions related to the generation of resource usage statistics
	/**
	 * Count registers that are due to design pipelining
	 * @return the string describing the performed operation
	 */
	std::string addPipelineFF();

	/**
	 * Count wires from declared signals
	 * @return the string describing the performed operation
	 */
	std::string addWireCount();

	/**
	 * Count I/O ports from declared inputs and outputs
	 * @return the string describing the performed operation
	 */
	std::string addPortCount();

	/**
	 * Count resources added from components
	 * @return the string describing the performed operation
	 */
	std::string addComponentResourceCount();

	/**
	 * Perform automatic operations related to resource estimation; this includes:
	 * 		- count registers added due to pipelining framework
	 * 		- count input/output ports
	 * 		- count resources in subcomponents
	 * Should not be used together with the manual estimation functions
	 * addWireCount, addPortCount, addComponentResourceCount!
	 * @return the string describing the performed operation
	 */
	void addAutomaticResourceEstimations();
	/////////////////////////////////////////////////////////////////////////////////////////////////


	/////////////////////////////////////////////////////////////////////////////////////////////////
	////////////Functions used for floorplanning
	/**
	 * NOTE: Floorplanning should be used only is resource estimation is
	 * also used. The floorplanning tools rely on the data provided by
	 * the resource estimation.
	 */


	/**
	 * Count the resources that have been added (as glue logic), since
	 * the last module has been instantiated. It will create a virtual
	 * module that is placed between the real modules, and that accounts
	 * for the space needed for the glue logic.
	 * Possibly to be integrated in the instance() method, as the
	 * process can be done without the intervention of the user.
	 * Uses and updates the pastEstimation... set of variables.
	 * @return the string summarizing the operation
	 */
	std::string manageFloorplan();

	/**
	 * Add a new placement constraint between the @source and @sink
	 * modules. The constraint should be read as: "@sink is @type of @source".
	 * The type of the constraint should be one of the following
	 * predefined constants: TO_LEFT_OF, TO_RIGHT_OF, ABOVE, UNDER.
	 * NOTE: @source and @sink are the operators' names, NOT
	 * the instances' names
	 * @param source the source sub-component
	 * @param sink the sink sub-component
	 * @param type the constraint type (has as value predefined constant)
	 * @return the string summarizing the operation
	 */
	std::string addPlacementConstraint(std::string source, std::string sink, int type);

	/**
	 * Add a new connectivity constraint between the @source and @sink
	 * modules. The constraint should be read as: "@sink is connected
	 * to @source by @nrWires wires".
	 * NOTE: @source and @sink are the operators' names, NOT
	 * the instances' names
	 * @param source the source sub-component
	 * @param sink the sink sub-component
	 * @param nrWires the number of wires that connect the two modules
	 * @return the string summarizing the operation
	 */
	std::string addConnectivityConstraint(std::string source, std::string sink, int nrWires);

	/**
	 * Add a new aspect constraint for @source module. The constraint
	 * should be read as: "@source's width is @ratio times larger than
	 * its width".
	 * @param source the source sub-component
	 * @param ratio the aspect ratio
	 * @return the string summarizing the operation
	 */
	std::string addAspectConstraint(std::string source, double ratio);

	/**
	 * Add a new constraint for @source module, regarding the contents
	 * of the module. The constraint gives an indication on the possible
	 * size/shape constraints, depending what the module contains.
	 * @param source the source sub-component
	 * @param value the type of content constraint
	 * @param length the length, if needed, of the component (for
	 * example for adders or multipliers)
	 * @return the string summarizing the operation
	 */
	std::string addContentConstraint(std::string source, int value, int length);

	/**
	 * Process the placement and connectivity constraints that the
	 * user has input using the corresponding functions.
	 * Start by processing the placement constraints and then, when
	 * needed, process the connectivity constraints
	 * @return the string summarizing the operation
	 */
	std::string processConstraints();

	/**
	 * Create the virtual grid for the sub-components.
	 * @return the string summarizing the operation
	 */
	std::string createVirtualGrid();

	/**
	 * Transform the virtual placement grid into the actual placement on
	 * the device, ready to generate the actual constraints file.
	 * @return the string summarizing the operation
	 */
	std::string createPlacementGrid();

	/**
	 * Create the file that will contain the floorplanning constraints.
	 * @return the string summarizing the operation
	 */
	std::string createConstraintsFile();

	/**
	 * Generate the placement for a given module.
	 * @param moduleName the name of the module
	 * @return the string summarizing the operation
	 */
	std::string createPlacementForComponent(std::string moduleName);

	/**
	 * Create the floorplan, according the flow described in each
	 * function and according to the user placed constraints.
	 */
	std::string createFloorplan();




	/////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////Attributes
	////////////BEWARE: don't add anything below without adding it to cloneOperator, too
	// TODO Most of these should be protected
		vector<Operator*>      subComponentList_;				/**< The list of instantiated sub-components */
		vector<Signal*>        signalList_;      				/**< The list of internal signals of the operator */
		vector<Signal*>        ioList_;                 /**< The list of I/O signals of the operator */
		set<string> allSignalsLowercased;        /**< a list of all lowercased signals, used for sanity checks */
	FlopocoStream          vhdl;                    /**< The internal stream to which the constructor will build the VHDL code */

	std::ostringstream 	resourceEstimate;                   /**< The log of resource estimations made by the user */
	std::ostringstream 	resourceEstimateReport;             /**< The final report of resource estimations made by the user */
	ResourceEstimationHelper* reHelper;                     /**< Performs all the necessary operations for resource estimation */
	bool reActive;                                          /**< Shows if any resource estimation operations have been performed */
	std::ostringstream 			floorplan;                  /**< Stream containing the floorplanning operations */
	FloorplanningHelper*		flpHelper;                  /**< Tools for floorplanning */
	int                    numberOfTests;                   /**< The number of tests, set by TestBench before this operator is tested. Useful for operators with state */

protected:
	string              srcFileName;                       /**< Used to debug and report.  */
	string              uniqueName_;                        /**< By default, a name derived from the operator class and the parameters */
	string 				      architectureName_;                  /**< Name of the operator architecture */
	vector<Signal*>     testCaseSignals_;                   /**< The list of pointers to the signals in a test case entry. Its size also gives the dimension of a test case */

	int                  myuid;                             /**< Unique id */
	int                  cost;                              /**< The cost of the operator depending on different metrics */


private:
	Target*                target_;                         /**< The target on which the operator will be deployed */
	Operator*              parentOp_;                       /**< The parent operator (i.e. the operator inside which this operator is a subcomponent)  */
	int                    stdLibType_;                     /**< 0 will use the Synopsys ieee.std_logic_unsigned, -1 uses std_logic_signed, 1 uses ieee numeric_std  (preferred) */
	bool                   isSequential_;                   /**< True if the operator needs a clock signal */
	int                    pipelineDepth_;                  /**< The pipeline depth of the operator. 0 for combinatorial circuits. A non-pipelined signal can still be sequential, e.g. a FIR. */
	map<string, Signal*>   signalMap_;                      /**< A dictionary of signals, for recovering a signal based on it's name */
	map<string, OperatorPtr> instanceOp_ ;                  /**< A map to get instance info   */
	map<string, vector<string>> instanceActualIO_ ;         /**< A map to get instance info. This list is in the same order as the ioList of the subcomponent   */
	map<string, pair<string, string>> constants_;           /**< The list of constants of the operator: name, <type, value> */
	map<string, string>    attributes_;                     /**< The list of attribute declarations (name, type) */
	map<pair<string,string>, string >  attributesValues_;   /**< attribute values <attribute name, object (component, signal, etc)> ,  value> */
	map<string, bool>      attributesAddSignal_;            /**< Vivado requires to add :signal, I have to read a VHDL book to understand how to do this cleany */
	map<string, string>    types_;                          /**< The list of type declarations (name, type) */
	string                 commentedName_;                  /**< Usually is the default name of the architecture.  */
	string                 headerComment_;                  /**< Optional comment that gets added to the header. Possibly multiline.  */
	string                 copyrightString_;                /**< Authors and years.  */
	string                 additionalHeaderString_;         /**< User-defined header information (used, e.g., for primitives that require extra libraries).  */
	bool                   hasClockEnable_;    	            /**< True if the operator has a clock enable signal  */
	int		                 hasDelay1Feedbacks_;             /**< True if this operator has feedbacks of one cycle, and no more than one cycle (i.e. an error if the distance is more). False gives warnings */
	Operator*              indirectOperator_;               /**< NULL if this operator is just an interface operator to several possible implementations, otherwise points to the instance*/
	// small TODO: rename
	bool                   isOperatorScheduled_;            /**< Flag to show whether this operator has already been scheduled, mostly to avoid redundant work */
	bool                   isOperatorApplyScheduleDone_;    /**< Flag to show whether this operator has already passed applySchedule, mostly to avoid redundant work */
	bool                   isOperatorImplemented_;          /**< Flag to show whether this operator has already been implemented (down to VHDL output) */
	bool 					isTopLevelDotDrawn_;
	bool                   noParseNoSchedule_;              /**< Flag instructing the VHDL to go through unchanged */
	bool                   isShared_;                       /**< Flag to show whether the instances of this operator are flattened in the design or not */
	bool                   isLibraryComponent_;             /**< Flag that indicates the the component is a library component (e.g., like primitives) and no code for the component or entity is generated. */

	vector<triplet<string, string, int>> unresolvedDependenceTable;   /**< The list of dependence relations which contain on either the lhs or rhs an (still) unknown name */
	std::ostringstream     dotDiagram;                          /**< The internal stream to which the drawing methods will output */
	set<Signal*> alreadyScheduled;                          /**< only used by a top-level operator: the set of signals that are already scheduled. */

	map<string, string>  tmpInPortMap_;                    /**< Input port map for the instance of this operator currently being built. Temporary variable, that will be pushed into portMaps_. Strings are used to allow to connect with ranges of a signal like, e.g., A => B(7) */
	map<string, string>  tmpOutPortMap_;                   /**< Output port map for the instance of this operator currently being built. Temporary variable, that will be pushed into portMaps_ Strings are used to allow to connect with ranges of a signal like, e.g., A => B(7) */
	map<std::string, std::string> generics_;               /**< A map for generics, this is required to include library elements like primitives */


};


} //namespace


#endif
