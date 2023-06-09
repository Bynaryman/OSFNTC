#ifndef IntSquarerS_HPP
#define IntSquarerS_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <gmpxx.h>
#include "Operator.hpp"
#include "IntAddSubCmp/IntAdder.hpp"
#include "IntMult//IntMultiplier.hpp"

namespace flopoco{

	/** The IntSquarer class for experimenting with squarers. 
	 */
	class IntSquarer : public Operator
	{
	public:
		/**
		 * The IntSquarer constructor
		 * @param[in] target the target device
		 * @param[in] wIn    the with of the inputs and output
		 * @param[in] inputDelays the delays for each input
		 **/
		IntSquarer(Target* target, int wIn, map<string, double> inputDelays = emptyDelayMap);
		/*IntSquarer(Target* target, int wIn);
		  void cmn(Target* target, int wIn, map<string, double> inputDelays);*/
	
		/**
		 *  Destructor
		 */
		~IntSquarer();

		/**
		 * Method belonging to the Operator class overloaded by the IntSquarer class
		 * @param[in,out] o     the stream where the current architecture will be outputed to
		 * @param[in]     name  the name of the entity corresponding to the architecture generated in this method
		 **/
		void outputVHDL(std::ostream& o, std::string name);

		void emulate(TestCase* tc);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();

	protected:
		int wIn_;                         /**< the width for X, Y and R*/

	private:
		map<string, double> inputDelays_; /**< a map between input signal names and their maximum delays */
		int bufferedInputs;               /**< variable denoting an initial buffering of the inputs */
		double maxInputDelay;             /**< the maximum delay between the inputs present in the map*/
		int nbOfChunks;                   /**< the number of chunks that the addition will be split in */
		int chunkSize_;                   /**< the suggested chunk size so that the addition can take place at the objective frequency*/
		int *cSize;                       /**< array containing the chunk sizes for all nbOfChunks*/
		/** A IntAdder subcomponent */
		IntAdder* intadder;
		IntAdder* intadd2;
		IntSquarer *intsquarer;
	};
}
#endif
