#define BOOST_TEST_DYN_LINK   
#define BOOST_TEST_MODULE IntConstMultShiftAddAdderCostTest

#include <boost/test/unit_test.hpp>
#include "pagsuite/adder_graph.h"
#include "ConstMult/error_comp_graph.hpp"

#include <sstream>
#include <iostream>

using namespace std;
using namespace PAGSuite;
using namespace IntConstMultShiftAdd_TYPES;

BOOST_AUTO_TEST_CASE(SimpleUntruncatedAddGraph) {
	adder_graph_t adder_graph;
	string graph_descr = "{{'R',[1],1,[1],0},{'A',[3],1,[1],0,0,[1],0,1},{'A',[11],2,[1],1,3,[3],1,0},{'O',[11],2,[11],2,0}}";

	adder_graph.parse_to_graph(graph_descr);
	ostringstream output_stream;
	
	print_aligned_word_graph(adder_graph, "", 8, output_stream);

	string TestVal = "Decomposition for 11:\n\n xxxxxxxxxxxx 11X\n (\n  xxxxxxxx    1X\n"
		"   xxxxxxxxxx 3X\n   (\n     xxxxxxxx 1X\n    xxxxxxxx  1X\n   )\n )\n\n";

	string errorMsg = "Bad message. [\n" + TestVal + "\n] was expected, got [\n" 
			+ output_stream.str() + "\n]";

	
	BOOST_REQUIRE_MESSAGE(TestVal == output_stream.str(), errorMsg);
}

BOOST_AUTO_TEST_CASE(SimpleTruncatedGraph) {
	adder_graph_t adder_graph;
	string graph_descr = "{{'R',[1],1,[1],0},{'A',[3],1,[1],0,0,[1],0,1},{'A',[11],2,[1],1,3,[3],1,0},{'O',[11],2,[11],2,0}}";
	string truncations = "3,1:2,1;11,2:0,2";

	adder_graph.parse_to_graph(graph_descr);
	ostringstream output_stream;
	
	print_aligned_word_graph(adder_graph, truncations, 8, output_stream);

	string TestVal = "Decomposition for 11:\n\n xxxxxxxxxxxx 11X\n (\n  xxxxxxxx    1X\n"
		"   xxxxxxxx-- 3X\n   (\n     xxxxxx-- 1X\n    xxxxxxx-  1X\n   )\n )\n\n";

	string errorMsg = "Bad message. [\n" + TestVal + "\n] was expected, got [\n" 
			+ output_stream.str() + "\n]";

	
	BOOST_REQUIRE_MESSAGE(TestVal == output_stream.str(), errorMsg);
}

BOOST_AUTO_TEST_CASE(LongGraphUntruncated) {
	adder_graph_t adder_graph;
	string graph_descr = "{{'A',[9],1,[1],0,0,[1],0,3},{'A',[17],1,[1],0,0,[1],0,4},{'A',[59],2,[17],1,2,[-9],1,0},{'A',[559],2,[9],1,6,[-17],1,0},{'A',[295],3,[59],2,0,[59],2,2},{'A',[3217],3,[59],2,6,[-559],2,0},{'A',[105414361],4,[3217],3,15,[-295],3,0},{'O',[105414361],4,[105414361],4,0}}";

	string truncations = "";

	string expexted_result = "Decomposition for 105414361:\n\n"
		" xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx 105414361X\n"
		" (\n"
		" xxxxxxxxxxxxxxxxxxxx                3217X\n"
		" (\n"
		" xxxxxxxxxxxxxx                      59X\n"
		" (\n"
		"xxxxxxxxxxxxx                        17X\n"
		"(\n"
		"     xxxxxxxx                        1X\n"
		" xxxxxxxx                            1X\n"
		")\n"
		"   xxxxxxxxxxxx                      9X\n"
		"   (\n"
		"       xxxxxxxx                      1X\n"
		"    xxxxxxxx                         1X\n"
		"   )\n"
		" )\n"
		"   xxxxxxxxxxxxxxxxxx                559X\n"
		"   (\n"
		"   xxxxxxxxxxxx                      9X\n"
		"   (\n"
		"       xxxxxxxx                      1X\n"
		"    xxxxxxxx                         1X\n"
		"   )\n"
		"        xxxxxxxxxxxxx                17X\n"
		"        (\n"
		"             xxxxxxxx                1X\n"
		"         xxxxxxxx                    1X\n"
		"        )\n"
		"   )\n"
		" )\n"
		"                   xxxxxxxxxxxxxxxxx 295X\n"
		"                   (\n"
		"                      xxxxxxxxxxxxxx 59X\n"
		"                      (\n"
		"                     xxxxxxxxxxxxx   17X\n"
		"                     (\n"
		"                          xxxxxxxx   1X\n"
		"                      xxxxxxxx       1X\n"
		"                     )\n"
		"                        xxxxxxxxxxxx 9X\n"
		"                        (\n"
		"                            xxxxxxxx 1X\n"
		"                         xxxxxxxx    1X\n"
		"                        )\n"
		"                      )\n"
		"                    xxxxxxxxxxxxxx   59X\n"
		"                    (\n"
		"                   xxxxxxxxxxxxx     17X\n"
		"                   (\n"
		"                        xxxxxxxx     1X\n"
		"                    xxxxxxxx         1X\n"
		"                   )\n"
		"                      xxxxxxxxxxxx   9X\n"
		"                      (\n"
		"                          xxxxxxxx   1X\n"
		"                       xxxxxxxx      1X\n"
		"                      )\n"
		"                    )\n"
		"                   )\n"
		" )\n\n";
	adder_graph.parse_to_graph(graph_descr);
	ostringstream output_stream;
	
	print_aligned_word_graph(adder_graph, truncations, 8, output_stream);
	
	BOOST_REQUIRE_MESSAGE(output_stream.str() == expexted_result, 
			"Expecting [\n" +expexted_result + "\n] got [\n" + output_stream.str() +"\n]" );
}

BOOST_AUTO_TEST_CASE(ErrorCostNoTruncation) {
	adder_graph_t adder_graph;
	string graph_descr = "{{'R',[1],1,[1],0},{'A',[3],1,[1],0,0,[1],0,1},{'A',[11],2,[1],1,3,[3],1,0},{'O',[11],2,[11],2,0}}";

	adder_graph.parse_to_graph(graph_descr);
	string truncations = "";
	
	TruncationRegister trunc(truncations);
	output_node_t* out_node;
	for (auto nodePtr : adder_graph.nodes_list) {
		if (is_a<output_node_t>(*nodePtr)) {
			out_node = (output_node_t*) nodePtr;
		}
	}

	auto error = getAccumulatedErrorFor(out_node, trunc);

	BOOST_REQUIRE_MESSAGE(
			error.positive_error == mpz_class(0), 
			"The positive error of an untruncated graph should be zero"
		);
	
	BOOST_REQUIRE_MESSAGE(
			error.negative_error == mpz_class(0), 
			"The negative error of an untruncated graph should be zero"
		);
}

BOOST_AUTO_TEST_CASE(ErrorCostSimpleTruncation) {
	adder_graph_t adder_graph;
	string graph_descr = "{{'R',[1],1,[1],0},{'A',[3],1,[1],0,0,[1],0,1},{'A',[11],2,[1],1,3,[3],1,0},{'O',[11],2,[11],2,0}}";

	adder_graph.parse_to_graph(graph_descr);
	string truncations = "3,1:2,1;11,2:0,2";
	
	TruncationRegister trunc(truncations);
	output_node_t* out_node;
	for (auto nodePtr : adder_graph.nodes_list) {
		if (is_a<output_node_t>(*nodePtr)) {
			out_node = (output_node_t*) nodePtr;
		}
	}

	auto error = getAccumulatedErrorFor(out_node, trunc);

	BOOST_REQUIRE_MESSAGE(
			error.positive_error == mpz_class(5), 
			"Error when counting the positive error. Got " << error.positive_error <<
		   "instead of 5."	
		);
	
	BOOST_REQUIRE_MESSAGE(
			error.negative_error == mpz_class(0), 
			"The negative error of graph without substraction should be zero"
		);
}

BOOST_AUTO_TEST_CASE(ErrorCostAccumulativeTruncation) {
	adder_graph_t adder_graph;
	string graph_descr = "{{'R',[1],1,[1],0},{'A',[3],1,[1],0,0,[1],0,1},{'A',[11],2,[1],1,3,[3],1,0},{'O',[11],2,[11],2,0}}";

	adder_graph.parse_to_graph(graph_descr);
	string truncations = "3,1:2,1;11,2:0,3";
	
	TruncationRegister trunc(truncations);
	output_node_t* out_node;
	for (auto nodePtr : adder_graph.nodes_list) {
		if (is_a<output_node_t>(*nodePtr)) {
			out_node = (output_node_t*) nodePtr;
		}
	}

	auto error = getAccumulatedErrorFor(out_node, trunc);

	BOOST_REQUIRE_MESSAGE(
			error.positive_error == mpz_class(9), 
			"Error when counting the positive error. Got " << error.positive_error <<
		   "instead of 9."	
		);
	
	BOOST_REQUIRE_MESSAGE(
			error.negative_error == mpz_class(0), 
			"The negative error of graph without substraction should be zero"
		);
}

BOOST_AUTO_TEST_CASE(ErrorCostUnderTruncation) {
	adder_graph_t adder_graph;
	string graph_descr = "{{'R',[1],1,[1],0},{'A',[3],1,[1],0,0,[1],0,1},{'A',[11],2,[1],1,3,[3],1,0},{'O',[11],2,[11],2,0}}";

	adder_graph.parse_to_graph(graph_descr);
	string truncations = "3,1:2,1;11,2:0,1";
	
	TruncationRegister trunc(truncations);
	output_node_t* out_node;
	for (auto nodePtr : adder_graph.nodes_list) {
		if (is_a<output_node_t>(*nodePtr)) {
			out_node = (output_node_t*) nodePtr;
		}
	}

	auto error = getAccumulatedErrorFor(out_node, trunc);

	BOOST_REQUIRE_MESSAGE(
			error.positive_error == mpz_class(5), 
			"Error when counting the positive error. Got " << error.positive_error <<
		   "instead of 5."	
		);
	
	BOOST_REQUIRE_MESSAGE(
			error.negative_error == mpz_class(0), 
			"The negative error of graph without substraction should be zero"
		);
}

BOOST_AUTO_TEST_CASE(ErrorCostSimpleNegTruncation) {
	adder_graph_t adder_graph;
	string graph_descr = "{{'R',[1],1,[1],0},{'A',[15],1,[1],0,4,[-1],0,0},{'A',[79],2,[1],1,6,[15],1,0},{'O',[79],2,[79],2,0}}";

	adder_graph.parse_to_graph(graph_descr);
	string truncations = "15,1:0,3;79,2:0,3";
	
	TruncationRegister trunc(truncations);
	output_node_t* out_node;
	for (auto nodePtr : adder_graph.nodes_list) {
		if (is_a<output_node_t>(*nodePtr)) {
			out_node = (output_node_t*) nodePtr;
		}
	}

	auto error = getAccumulatedErrorFor(out_node, trunc);

	BOOST_REQUIRE_MESSAGE(
			error.positive_error == mpz_class(0), 
			"Expecting zero loss for addition"
		);
	
	BOOST_REQUIRE_MESSAGE(
			error.negative_error == mpz_class(7), 
			"Expecting 7 ulp of negative error, got " << error.negative_error 
		);
}

BOOST_AUTO_TEST_CASE(test_error_sub_propagate)
{
	adder_graph_t adder_graph;
	string graph_descr = "{{'R',[1],1,[1],0},{'A',[15],1,[1],0,4,[-1],0,0},"
		"{'A',[127],1,[1],0,7,[-1],0,0},{'A',[112],2,[127],1,0,[-15],1,0},"
		"{'A',[624],3,[112],2,0,[1],1,9},{'O',[624],3,[624],3,0}}";

	adder_graph.parse_to_graph(graph_descr);
	string truncations = "15,1:0,2";
	
	TruncationRegister trunc(truncations);
	output_node_t* out_node;
	for (auto nodePtr : adder_graph.nodes_list) {
		if (is_a<output_node_t>(*nodePtr)) {
			out_node = (output_node_t*) nodePtr;
		}
	}

	auto error = getAccumulatedErrorFor(out_node, trunc);

	BOOST_REQUIRE_MESSAGE(error.negative_error == mpz_class(0), 
			"Negative error should be zero, got " << error.negative_error);

	BOOST_REQUIRE_MESSAGE(error.positive_error == mpz_class(3),
			"Positive error should be 3, got " << error.positive_error); 
}

BOOST_AUTO_TEST_CASE(test_swap)
{
	ErrorStorage err;
	err.positive_error = mpz_class(1);
	err.negative_error = mpz_class(0);
	err.swap();
	BOOST_REQUIRE_MESSAGE(err.positive_error == mpz_class(0), "Error when swapping");
	BOOST_REQUIRE_MESSAGE(err.negative_error == mpz_class(1), "Error when swapping");
}


