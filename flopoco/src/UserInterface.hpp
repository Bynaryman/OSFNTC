/**
A generic user interface class that manages the command line and the documentation for various operators
Includes an operator factory inspired by David Thomas

For typical use, see src/ExpLog/FPExp.*

Author : David Thomas, Florent de Dinechin

Initial software.
Copyright © INSA-Lyon, ENS-Lyon, INRIA, CNRS, UCBL,
2015-.
  All rights reserved.

*/
#ifndef UserInterface_hpp
#define UserInterface_hpp

#include "Operator.hpp"
#include <memory>
#include <cstdint> //for int64_t

// Operator Factory, based on the one by David Thomas, with a bit of clean up.
// For typical use, see src/ShiftersEtc/Shifter  or   src/FPAddSub/FPAdd*

namespace flopoco
{
	class Operator;
	typedef Operator* OperatorPtr;
	typedef vector<vector<pair<string,string>>> TestList;
	typedef TestList (*unitTest_func_t)(int);

	typedef OperatorPtr (*parser_func_t)(OperatorPtr, Target *, vector<string> &);	//this defines parser_func_t as a pointer to a function taking as parameters Target* etc., and returning an OperatorPtr
	class OperatorFactory;
	typedef shared_ptr<OperatorFactory> OperatorFactoryPtr;

	typedef pair<string, vector<string>> option_t;


	/** This is the class that manages a list of OperatorFactories, and the overall command line and documentation.
			Each OperatorFactory is responsible for the command line and parsing for one Operator sub-class. */
	class UserInterface
	{
	public:

		/** The function that does it all */
		static void main(int argc, char* argv[]);


		/**  main initialization function */
		static	void initialize();

		static void registerFactories();
		
		/** parse all the operators passed on the command-line */
		static void buildAll(int argc, char* argv[]);

		/** starts the dot diagram plotter on the operators */
		static void drawDotDiagram(vector<OperatorPtr> &oplist);

		/** generates the code to the default file */
		static void outputVHDL();

		/** generates a report for operators in globalOpList, and all their subcomponents */
		static void finalReport(ostream & s);


		/**a helper factory function. For the parameter documentation, see the OperatorFactory constructor */
		static void add(
										string name,
										string description,
										string category,
										string seeAlso,
										string parameterList,
										string extraHTMLDoc,
										parser_func_t parser,
										unitTest_func_t unitTest = nullptr
										);
		
		static unsigned getFactoryCount();
		static OperatorFactoryPtr getFactoryByIndex(unsigned i);
		static OperatorFactoryPtr getFactoryByName(string operatorName);



		////////////////// Helper parsing functions to be used in each Operator parser ///////////////////////////////
		static void parseBoolean(vector<string> &args, string key, bool* variable, bool genericOption=false);
		static void parseInt(vector<string> &args, string key, int* variable, bool genericOption=false);
		static void parsePositiveInt(vector<string> &args, string key, int* variable, bool genericOption=false);
		static void parseStrictlyPositiveInt(vector<string> &args, string key, int* variable, bool genericOption=false);
		static void parseFloat(vector<string> &args, string key, double* variable, bool genericOption=false);
		static void parseString(vector<string> &args, string key, string* variable, bool genericOption=false);
		static void parseColonSeparatedStringList(vector<string> &args, string key, vector<string>* variable, bool genericOption=false);
		static void parseColonSeparatedIntList(vector<string> &args, string key, vector<int>* variable, bool genericOption=false);

		/** Provide a string with the full documentation.*/
		static string getFullDoc();

		/** add an operator to the global (first-level) list.
				This method should be called by
				1/ the main / top-level, or
				2/ for "shared" sub-components that are really basic operators,
				expected to be used several times, *in a way that is independent of the context/timing*.
				Typical example is a table designed to fit in a LUT, such as compressors etc.
				Such shared components are identified by their name. 
				If several components want to add the same shared component, only the first is added.
				Subsequent attempts to add a shared component with the same name instead return a pointer to the one in globalOpList.
		*/
		static OperatorPtr addToGlobalOpList(OperatorPtr op);

		/** Saves the current globalOpList on globalOpListStack, then clears globalOpList. 
				Typical use is when you want to build various variants of a subcomponent before chosing the best one.
				TODO: a variant that leaves in globalOpList a deep copy of the saved one, so that such comparisons can be done in context 
		*/
		static void pushAndClearGlobalOpList();

		/** Restores the globalOpList from top of globalOpListStack. 
				Typical use is when you want to build various variants of a subcomponent before chosing the best one.
		*/
		static void popGlobalOpList();

		/** generates the code for operators in globalOpList, and all their subcomponents */
		static void outputVHDLToFile(ofstream& file);

		/** generates the code for operators in oplist, and all their subcomponents */
		static void outputVHDLToFile(vector<OperatorPtr> &oplist, ofstream& file, set<string> &alreadyOutput);

	private:
		/** register a factory */
		static void registerFactory(OperatorFactoryPtr factory);

		/** error reporting */
		static void throwMissingArgError(string opname, string key);
		/** parse all the generic options such as name, target, verbose, etc. */
		static void parseGenericOptions(vector<string>& args);

		/** Build operators.html directly into the doc directory. */
		static void buildHTMLDoc();

		/** Build flopoco bash autocompletion file **/
		static void buildAutocomplete();

	public:
		static vector<OperatorPtr>  globalOpList;  /**< Level-0 operators. Each of these can have sub-operators */
		static vector<vector<OperatorPtr>>  globalOpListStack;  /**< a stack on which to save globalOpList when you don't want to mess with it */
		static int    verbose;
		static int pipelineActive_;
	private:
		static string outputFileName;
		static string entityName;
		static string targetFPGA;
		static double targetFrequencyMHz;
		static bool   pipeline;
		static bool   clockEnable;
		static bool   useHardMult;
		static bool   plainVHDL;
		static bool   generateFigures;
		static double unusedHardMultThreshold;
        static bool   useTargetOptimizations;
		static string compression;
		static string tiling;
		static string ilpSolver;
		static int    ilpTimeout;
		static int    resourceEstimation;
		static bool   floorplanning;
		static bool   reDebug;
		static bool   flpDebug;
		static vector<pair<string,OperatorFactoryPtr>> factoryList; // used to be a map, but I don't want them listed in alphabetical order
		static const vector<pair<string,string>> categories;

		static const vector<string> known_fpgas;
		static const vector<string> special_targets;
		static const vector<option_t> options;

		static string depGraphDrawing;
	};

	/** This is the abstract class that each operator factory will inherit.
			Each OperatorFactory is responsible for the command line and parsing for one Operator sub-class.  */
	class OperatorFactory
	{
		friend UserInterface;
	private:

		string m_name; /**< see constructor doc */
		string m_description;  /**< see constructor doc */
		string m_category;  /**< see constructor doc */
		string m_seeAlso; /**< see constructor doc */
		vector<string> m_paramNames;  /**< list of paramater names */
		map<string,string> m_paramType;  /**< type of parameters listed in m_paramNames */
		map<string,string> m_paramDoc;  /**< description of parameters listed in m_paramNames */
		map<string,string> m_paramDefault; /* If equal to "", this parameter is mandatory (no default). Otherwise, default value (as a string, to be parsed) */
		string m_extraHTMLDoc;
		parser_func_t m_parser;
		unitTest_func_t m_unitTest;

	public:

		/** Implements a no-frills factory
				\param name         Name for the operator.
				\param description  The short documentation
				\param category     A category used to organize the doc.
				\param parameters   A semicolon-separated list of parameter description, each being name(type)[=default]:short_description
				\param parser       A function that can parse a vector of string arguments into an Operator instance
				\param extraHTMLDoc Extra information to go to the HTML doc, for instance links to articles or details on the algorithms
		**/
		OperatorFactory(
						 string name,
						 string description,
						 string category,
						 string seeAlso,
						 string parameters,
						 string extraHTMLDoc,
						 parser_func_t parser,
						 unitTest_func_t unitTest	);

		virtual const string &name() const // You can see in this prototype that it was not written by Florent
		{ return m_name; }

		/** Provide a string with the full documentation. */
		string getFullDoc();
		/** Provide a string with the full documentation in HTML. */
		string getHTMLDoc();

		const vector<string> &param_names(void) const;

		string getOperatorFunctions(void);

		/** get the default value associated to a parameter (empty string if there is no default)*/
		string getDefaultParamVal(const string& key);

		/*! Consumes zero or more string arguments, and creates an operator
			The parentOp may be nullptr for top-level entities
			\param args The offered arguments start at index 0 of the vector, and it is up to the
			factory to check the types and whether there are enough.
			\param consumed On exit, the factory indicates how many of the arguments are used up.
		*/
		virtual OperatorPtr parseArguments(OperatorPtr parentOp, Target* target, vector<string> &args	);
		
		/*! Generate a list of arg-value pairs out of the index value
		*/
		virtual TestList unitTestGenerator(int index);

	};
}; // namespace flopoco

#endif

