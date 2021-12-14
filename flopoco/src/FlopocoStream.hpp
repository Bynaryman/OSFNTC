#ifndef FlopocoStream_HPP
#define FlopocoStream_HPP
#include <vector>
#include <sstream>
#include <utility>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <cstdlib>
#include <string.h>

#include <string>

//#include "VHDLLexer.hpp"
#include "LexerContext.hpp"

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

namespace flopoco{

	//forward reference to Operator, in order to avoid cyclic references
	class Operator;

	//forward reference to FlopocoStream, in order to overload the << stream operator
	class FlopocoStream;

	/**
	 * The FlopocoStream class.
	 * Segments of code having the same pipeline informations are scanned
	 * on-the-fly using flex++ to find the VHDL signal IDs. The found IDs
	 * are marked (ID_Name becomes $$ID_Name$$, if it is a signal on the
	 * right-hand side of an assignment, ??ID_Name??, if it is on the left-hand side)
	 * for the second pass.
	 * The signals with delays are marked as well (ID_Name becomes ID_Name^nb_cycles (pipeline delay)
	 * or ID_Name^nb_cycles (functional delay)).
	 * The assignment statements are appended with the name of the left-hand signal (??ID_Name??).
	 */
	class FlopocoStream{
		public:

		template <class paramType>
		friend FlopocoStream& operator <<(FlopocoStream& output, paramType c) {
			output.vhdlCodeBuffer << c;
			output.codeParsed = false;
			return output;
		}

		friend FlopocoStream& operator <<(FlopocoStream& output, char c) {
			output.vhdlCodeBuffer << c;
			output.codeParsed = false;
			if(c == ';')
				output.flushAndParseAndBuildDependencyTable();
			return output;
		}

		friend FlopocoStream& operator<<(FlopocoStream& output, std::string s) {
			output.vhdlCodeBuffer << s;
			output.codeParsed = false;
			if(s.find(';') != s.npos)
				output.flushAndParseAndBuildDependencyTable();
			return output;
		}

		friend FlopocoStream& operator<<(FlopocoStream& output, char *s) {
			output.vhdlCodeBuffer << s;
			output.codeParsed = false;
			string str(s);
			if(str.find(';') != str.npos)
				output.flushAndParseAndBuildDependencyTable();
			return output;
		}

		friend FlopocoStream& operator<<(FlopocoStream& output, char const *s) {
			output.vhdlCodeBuffer << s;
			output.codeParsed = false;
			string str(s);
			if(str.find(';') != str.npos)
				output.flushAndParseAndBuildDependencyTable();
			return output;
		}

		friend FlopocoStream& operator <<(FlopocoStream& output, FlopocoStream fs) {
			output.vhdlCodeBuffer << fs.vhdlCode.str();
			output.codeParsed = false;
			if(fs.vhdlCode.str().find(';') != fs.vhdlCode.str().npos)
				output.flushAndParseAndBuildDependencyTable();
			return output;
		}

		friend FlopocoStream& operator <<(FlopocoStream& output, UNUSED(ostream& (*f)(ostream& fs)) ){
			output.vhdlCodeBuffer << std::endl;
			output.codeParsed = false;
			return output;
		}

		public:
			/**
			 * FlopocoStream constructor.
			 * Initializes the two streams: vhdlCode and vhdlCodeBuffer
			 */
			FlopocoStream();

			/**
			 * FlopocoStream destructor
			 */
			~FlopocoStream();

			/**
			 * NOTE: this is the safe way of triggering the vhdl code parsing and processing
			 *
			 * Method that does a similar thing as str() does on ostringstream objects.
			 * Processing is done using the vhdl code buffer (vhdlCodeBuffer).
			 * The output code is = existing code + newly transformed code (from the code buffer);
			 * The transformation on vhdlCode is done when the buffer is flushed
			 * @return the augmented string encapsulated by FlopocoStream
			 */
			string str();

			/**
			 * Resets both the code stream and the code buffer.
			 * @return returns empty string for compatibility issues.
			 */
			string str(string UNUSED(s));

			/**
			 * Function used to flush the buffer
			 * 	- save the code in the temporary buffer
			 * 	- parse the code from the temporary buffer and add it to the stream
			 * Extract the dependencies between the signals.
			 * Annotate the signal names, for the second phase. All signals on the right-hand side of signal assignments
			 * will be transformed from signal_name to $$signal_name$$.
			 * The name of the left-hand side signal is annotated to ??lhs_name??.
			 * 	- build the dependenceTable and annotate the code
			 We build a dependency table, not yet the final signal graph, so that it is possible to declare a signal only after using it.
			 */
			void flushAndParseAndBuildDependencyTable();


			/**
			 * Member function used to set the code resulted after a second parsing
			 * was performed
			 * @param[in] code the 2nd parse level code
			 */
			void setSecondLevelCode(string code);

			/**
			 * Returns the dependenceTable
			 */
			vector<triplet<string, string, int>> getDependenceTable();


			bool isEmpty();

			bool setOperator(Operator* op);

			/**
			 * The dependence table should contain pairs of the form (lhsName, RhsName),
			 * where lhsName and rhsName are the names of the left-hand side and right-hand side
			 * of an assignment.
			 * Because of the parsing stage, lhsName might be of the form (lhsName1, lhsName2, ...),
			 * which must be fixed.
			 */
			void cleanupDependenceTable();


			ostringstream vhdlCode;                                 /**< the vhdl code */
			ostringstream vhdlCodeBuffer;                           /**< the temporary vhdl code buffer */

			vector<triplet<string, string, int>> dependenceTable;   /**< table containing the left-hand side - right-hand side dependences, with the possible delay on the edge */

			//the lexing context
			string lexLhsName;
			vector<string> lexExtraRhsNames;
			vector<triplet<string, string, int>> lexDependenceTable;
			LexerContext::LexMode lexLexingMode;
			LexerContext::LexMode lexLexingModeOld;
			bool lexIsLhsSet;

		protected:

			Operator *op=0;
			bool codeParsed;
	};
}
#endif
