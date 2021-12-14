#ifndef MULTIPARTITETABLE_H
#define MULTIPARTITETABLE_H


#include <mpfr.h>

#include <gmp.h>
#include <gmpxx.h>

#include <vector>

using namespace std;



#include "FixFunction.hpp"
#include "../Operator.hpp"


namespace flopoco
{
	class Multipartite;

	class FixFunctionByMultipartiteTable : public Operator
	{
		// Multipartite needs to access some private fields of FixFunctionByMultipartiteTable, so we declare it as a friend...
		friend class Multipartite;

	public:

		//----------------------------------------------------------------------------- Constructor/Destructor
		/**
		 * @brief The FixFunctionByMultipartiteTable constructor
		 * @param[string] functionName_		a string representing the function, input range should be [0,1) or [-1,1)
		 * @param[int]    lsbIn_		input LSB weight
		 * @param[int]    msbOut_		output MSB weight, used to determine wOut
		 * @param[int]    lsbOut_		output LSB weight
		 * @param[int]	nbTOi_	number of tables which will be created
		 * @param[bool]	signedIn_	true if the input range is [-1,1)
		 */
		FixFunctionByMultipartiteTable(OperatorPtr parentOp, Target* target, string function, int nbTOi, bool signedIn,
																	 int lsbIn, int msbOut, int lsbOut, bool compressTIV);

		virtual ~FixFunctionByMultipartiteTable();

		//---------------------------------------Public standard methods
		void buildStandardTestCases(TestCaseList* tcl);
		void emulate(TestCase * tc);
		static TestList unitTest(int index);
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args);
		static void registerFactory();

	private:


		//----------------------------------Private methods

		/**
		 * @brief buildOneTableError : pre-computes the error for every beta_i and gamma_i, to speed up exploration
		 */
		void buildOneTableError();

		/**
		 * @brief buildOneTableError : pre-computes the min gamma for for every beta_i, to speed up exploration
		 */
		void buildGammaiMin();

		/**
		 * @brief enumerateDec : This function enumerates all the decompositions and inserts the best ones in topTen
		 * @return true if no decomposition found with an acceptable error
		 */
		bool enumerateDec();


		/** Some needed methods, extracted from the article */

		static vector<vector<int>> betaenum (int sum, int size);

		static vector<vector<int>> alphaenum(int alpha, int m);
		static vector<vector<int>> alphaenum(int alpha, int m, vector<int> gammaimin);
		static vector<vector<int>> alphaenumrec(int alpha, int m, vector<int> gammaimin);

		double errorForOneTable(int pi, int betai, int gammai);

		double epsilon(int ci_, int gammai, int betai, int pi);
		double epsilon2ndOrder(int Ai, int gammai, int betai, int pi);


		FixFunction *f;
		double epsilonT;
		// The following is not very well encapsulated, but no need to fix it
		int nbTOi;		/**< The number of tables used */
		bool compressTIV; /**< use Hsiao TIV compression or not */
		vector<vector<vector<double>>> oneTableError;   /** for nbTOi fixed, the errors of each possible table configuration, precomputed  here to speed up exploration  */
		vector<vector<int>> gammaiMin;  /** for nbTOi fixed, the min value of gamma, precomputed  here to speed up exploration */

	private:
		const int ten=10;
		vector<Multipartite*> topTen; /**< the top 10 best candidates (for some value of ten), sorted by size */ 
		int guardBitsSlack; /* this allows first to try the exploration with one guard bit less than the safe value */ 
		void insertInTopTen(Multipartite* mp);
	};

}

#endif // MULTIPARTITETABLE_H
