#include "../OPVariable.hpp"
#include "../OPOperation.hpp"
#include "../OPFunction.hpp"
#include "../OPExpression.hpp"
#include "../File.hpp"

#include <vector>
#include <map>
#include <iostream>

using namespace OperatorPipeline;

void usage(const char *name)
{
	std::cout << name << " <sample_name>" << std::endl;
	exit(2);
}

int main(int argc, char **argv)
{
	int sample = -1;
	if (argc < 2)
	{
		std::string sample_name = argv[1];
		if (sample_name == "assign") sample = 1;
	}
	if (sample == -1) usage(argv[0]);

	OPVariable *v1, *v2, *r1, *r2, *a, *b, *five, *six, *seven, *eight;
	std::vector<int> float_param;
	std::vector<int> int_param;

	File file;

	float_param.push_back(5);
	float_param.push_back(12);
	int_param.push_back(32);

	five = new OPVariable("5", SINT, nullptr, nullptr);
	six  = new OPVariable("6", SINT, nullptr, nullptr);
	seven = new OPVariable("7", SINT, nullptr, nullptr);

	a = new OPVariable("a", SINT, &int_param, nullptr);
	b = new OPVariable("b", SINT, &int_param, nullptr);

	file.variables["v1"] = &v1;
	file.variables["v2"] = &v2;
	file.variables["r2"] = &r2;
	file.variables["r1"] = &r1;
	file.variables["b"] = &b;
	file.variables["a"] = &a;


	switch (sample)
	{
		case 1: // assign
		{
			v1 = new OPVariable("v1", SINT, &int_param, a);

			file.returns.push_back(v1);
			break;
		}
		case 2: // plus_minus
		{
			OPOperation* op1 = new OPOperation(five, b, PLUS);
			OPOperation* op2 = new OPOperation(a, op1, MOINS);
			v1 = new OPVariable("", FLOAT, &float_param, op2);
			file.expressions.push_back(v1);

			file.returns.push_back(v1);
			break;
		}
		case 3: // long_plus
		{
			OPOperation* a_5 = new OPOperation(a, five, PLUS);
			OPOperation* a_5_b = new OPOperation(a_5, b, PLUS);
			OPOperation* a_5_b_6 = new OPOperation(a_5_b, six, PLUS);
			OPOperation* a_5_b_6_7 = new OPOperation(a_5_b_6, seven, PLUS);
			r1 = new OPVariable("r1", FLOAT, &float_param, a_5_b_6_7);
			file.expressions.push_back(r1);

			OPOperation* r1_6 = new OPOperation(r1, six, PLUS);
			OPOperation* r1_6_7 = new OPOperation(r1_6, seven, PLUS);
			OPOperation* r1_6_7_8 = new OPOperation(r1_6_7, eight, PLUS);
			v1 = new OPVariable("v1", FLOAT, &float_param, r1_6_7_8);
			file.expressions.push_back(v1);

			file.returns.push_back(v1);
		}
		case 4: // double_return
		{
			OPOperation* op_r1 = new OPOperation(a, b, PLUS);
			r1 = new OPVariable("r1", FLOAT, &float_param, op_r1);
			file.expressions.push_back(r1);

			OPOperation* op_v1 = new OPOperation(r1, b, PLUS);
			v1 = new OPVariable("v1", FLOAT, &float_param, op_v1);
			file.expressions.push_back(v1);

			OPOperation* op_v2 = new OPOperation(r1, a, PLUS);
			v2 = new OPVariable("v2", FLOAT, &float_param, op_v2);
			file.expressions.push_back(v2);

			file.returns.push_back(v1);
			file.returns.push_back(v2);
		}
	}
}
