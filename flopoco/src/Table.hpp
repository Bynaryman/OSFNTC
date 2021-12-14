/*

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author:    Florent de Dinechin

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,

  All rights reserved.

 */

#ifndef TABLE_HPP
#define TABLE_HPP
#include <gmpxx.h>

#include "Operator.hpp"

/**
 A basic hardware look-up table for FloPoCo.

	 If the input to your table are negative, etc, or if you want to
	 define errors, or... then derive a class from this one.

	 To derive a class with a user interface from Table, see FixFunctions/FixFunctionByTable
	 To use a Table as a sub-component of a larger operator, see FixFunctions/FixFunctionByPiecewisePoly

	 A Table is, so far, always combinatorial. It does increase the critical path

	 On logic tables versus blockRam tables:
	 This has unfortunately to be managed twice,
	   firstly by passing the proper bool value to the logicTable argument of the constructor
	   and secondly by calling useSoftRAM() or useHardRAM() on each instance to set the synthesis attributes.

*/

namespace flopoco{


	class Table : public Operator
	{
		public:
		/**
		 * The Table constructor
		 * @param[in] parentOp 	the parent operator in the component hierarchy
		 * @param[in] target 		the target device
		 * @param[in] values 		the values used to fill the table. Each value is a bit vector given as positive mpz_class.
		 * @param[in] name      a new name for the VHDL entity
		 * @param[in] wIn    		the with of the input in bits (optional, may be deduced from values)
		 * @param[in] wOut   		the with of the output in bits  (optional, may be deduced from values)
		 * @param[in] logicTable 1 if the table is intended to be implemented as logic; It is then shared  
                            -1 if it is intended to be implemented as embedded RAM; 
	                        	 0 (default): let the constructor decide, depending on the size and target
		 * @param[in] minIn			minimal input value, to which value[0] will be mapped (default 0)
		 * @param[in] maxIn			maximal input value (default: values.size()-1)
		 */ 
		Table(OperatorPtr parentOp, Target* target, vector<mpz_class> _values, string name="",
					int _wIn = -1, int _wOut = -1, int _logicTable = 0, int _minIn = -1, int _maxIn = -1);

		Table(OperatorPtr parentOp, Target* target);

		virtual ~Table() {};

		/** A function that does the actual constructor work, so that it can be called from operators that overload Table.  See FixFunctionByTable for an example */

		void init(vector<mpz_class> _values, string name="", int _wIn = -1, int _wOut = -1, int _logicTable = 0, int _minIn = -1, int _maxIn = -1);
 
	
		/** get one element of the table */
	  mpz_class val(int x);

		/** Table has no factory because passing the values vector would be a pain. This replaces it.  
		 * @param[in] op            The Operator that will be the parent of this Table (usually "this")
		 * @param[in] actualInput   The actual input name
		 * @param[in] actualOutput  The actual input name
		 * @param[in] values        The vector of mpz_class values to be passed to the Table constructor
 */
		static OperatorPtr newUniqueInstance(OperatorPtr op,
																				 string actualInput, string actualOutput,
																				 vector<mpz_class> values, string name,
																				 int wIn = -1, int wOut = -1);

		/** A function that returns an estimation of the size of the table in LUTs. Your mileage may vary thanks to boolean optimization */
		int size_in_LUTs();
	private:
		bool full; 					/**< true if there is no "don't care" inputs, i.e. minIn=0 and maxIn=2^wIn-1 */
		bool logicTable; 			/**< true: LUT-based table; false: BRAM-based */
		double cpDelay;  				/**< For a LUT-based table, its delay; */

	public:

		vector<mpz_class> values;	/**< the values used to fill the table */

		/** Input width (in bits)*/
		int wIn;

		/** Output width (in bits)*/
		int wOut;
		
		/** minimal input value (default 0) */
		mpz_class minIn;

		/** maximal input value (default 2^wIn-1) */
		mpz_class maxIn;

		





};

}
#endif
