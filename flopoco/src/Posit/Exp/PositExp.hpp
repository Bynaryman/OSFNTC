
#ifndef __POSITEXP_HPP
#define __POSITEXP_HPP
#include <vector>
#include <sstream>

#include "Operator.hpp"
#include "Table.hpp"
#include "DualTable.hpp"

class Fragment;


namespace flopoco{

	
	class PositExp : public Operator
	{
	public:
		/** @brief The magic dual table, that holds either (e^A, e^Z-1) or (e^A, e^Z-Z-1)
			   |.....e^A....||...expZpart.....|
			   <--sizeExpA--><--sizeExpZPart-->
		*/

		/** The table that holds the exponential of the high bits of the input */
		vector<mpz_class> ExpYTable(int width, int wES);

		
		/** @brief The constructor with manual control of all options
		* @param wE exponent size
		* @param wF fraction size
		* @param k size of the input to the first table
		* @param d  degree of the polynomial approximation (if k=d=0, the
		* 			constructor tries to compute sensible values)
		* @param guardBits number of gard bits. If -1, a default value (that
		* 				   depends of the size)  is computed inside the constructor.  
		* @param fullInput boolean, if true input mantissa is of size wE+wF+1, 
		*                  so that  input shift doesn't padd it with 0s (useful 
		*                  for FPPow)
		*/
		PositExp(
					OperatorPtr parentOp, 
					Target* target, 
					int width,
					int wES,
					int k,
					int d,
					int guardBits=-1,
					bool fullInput=false);

		~PositExp();

		// Overloading the virtual functions of Operator
		// void outputVHDL(std::ostream& o, std::string name);
		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();
		
		
	private:
		int width; /**< Posit width*/
		int wES; /**< Exponent size */
		int k;  /**< Size of the address bits for the first table  */
		int d;  /**< Degree of the polynomial approximation */
		int g;  /**< Number of guard bits */
	};
}
#endif
