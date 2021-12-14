#ifndef PositFunctionByTable_HPP
#define PositFunctionByTable_HPP
#include <vector>

#include "../../Table.hpp"
#include "PositFunction.hpp"

namespace flopoco{

	/** The PositFunctionByTable class */
	class PositFunctionByTable : public Table
	{
	public:
		/**
			 The PositFunctionByTable constructor. For the meaning of the parameters, see FixFunction.hpp
		 */

		PositFunctionByTable(OperatorPtr parentOp, Target* target, string func, int width_, int wES);

		/**
		 * PositFunctionByTable destructor
		 */
		~PositFunctionByTable();

		void emulate(TestCase * tc);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */
		static void registerFactory();

	protected:

		PositFunction *f;
		unsigned wR;
	};

}

#endif
