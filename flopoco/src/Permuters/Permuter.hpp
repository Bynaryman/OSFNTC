#ifndef PERMUTER_HPP
#define PERMUTER_HPP

#include "../Operator.hpp"

namespace flopoco{
	/** 
	 *  Abstract class for permuters
	 */
	class Permuter : public Operator{
	public:

		//* functions int -> int

		typedef int (*f)(int) fint;

		/**
		 * The permuter constructor
		 * @param[in] parentOp the parent operator of this ocmponent
		 * @param[in] target the target device
		 * @param[in] wIn the with of the inputs and outputs
		 * @param[in] nbInputs the number of inputs
		 * @param[in] perm the permutation applied by the permuter
		 */
		Permuter(OperatorPtr parentOp, Target* target, int wIn, int nbInputs,
		         fint perm);
		
		~Permuter();

		/**
		 * The emulate function.
		 * @param[in] tc a list of test-cases
		 */
		void emulate (TestCase* tc);

		static void registeFactory();
		
	private:
		perm perm_;
		int wIn_;
		int nbInputs_;
	};
}

#endif
