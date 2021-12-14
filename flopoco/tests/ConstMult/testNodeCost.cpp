#define BOOST_TEST_DYN_LINK   
#define BOOST_TEST_MODULE IntConstMultShiftAddAdderCostTest

#include <boost/test/unit_test.hpp>
#include "ConstMult/adder_cost.hpp"
#include "pagsuite/adder_graph.h"

using namespace std;
using namespace PAGSuite;

BOOST_AUTO_TEST_CASE(SignedShiftedAdd) {
	adder_graph_t adder_graph;
	string graph_descr = "{{'R',[1],1,[1],0},{'A',[3],1,[1],0,0,[1],0,1},{'A',[11],2,[1],1,3,[3],1,0},{'O',[11],2,[11],2,0}}";

	adder_graph.parse_to_graph(graph_descr);

	int adder_cost = IntConstMultShiftAdd_TYPES::getGraphAdderCost(
			adder_graph,
			8,
			false
		);
	
	BOOST_REQUIRE_MESSAGE(adder_cost == 16 , "AdderCost for graph should be 16, got " <<
			adder_cost << "instead.");
}

BOOST_AUTO_TEST_CASE(UnsignedOverlappingAdd) {
	adder_graph_t adder_graph;
	string graph_descr = "{{'A',[17],1,[1],0,0,[1],0,4},{'O',[17],1,[17],1,0}}";

	adder_graph.parse_to_graph(graph_descr);

	int adder_cost = IntConstMultShiftAdd_TYPES::getGraphAdderCost(
			adder_graph,
			2,
			true	
		);
	
	BOOST_REQUIRE_MESSAGE(
			adder_cost == 0, 
			"AdderCost Non overlapping unsigned addition should be 0, got " <<
				adder_cost << "instead."
		);
}
