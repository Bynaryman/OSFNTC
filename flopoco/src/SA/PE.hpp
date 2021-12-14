/*

  PE: Processing element
  One design of a PE for classical orthogonal Systolic Array.
  Delay left (row J of Mat A) by 1
  Delay top (col I of Mat B) by 1
  Instantiates a PDFDP and feeds it with current A in and B in
  Author: Ledoux Louis

 */

#ifndef PE_HPP
#define PE_HPP
#include <vector>

#include "Operator.hpp"

namespace flopoco{

	class PE : public Operator
	{
	public:

		/**
		 * @brief A PE<n,es> constructor
		 * @param[in] {Target*} target : the target device
		 * @param[in] {int} n : the word width of the posit
		 * @param[in] {int} es : the tunable exponent size
		 **/
		PE(OperatorPtr parentOp, Target* target, int n, int es);

		/**
		 * PE destructor
		 */
		~PE();

		/**
		 * @param tc a TestCase partially filled with input values
		 */
		void emulate(TestCase * tc);

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		static void registerFactory();

	protected:

	};
}

#endif  // PE_HPP
