#include "PrimitiveComponents/Xilinx/XilinxGPC.hpp"
#include "PrimitiveComponents/Xilinx/Xilinx_LUT6.hpp"
#include "PrimitiveComponents/Xilinx/Xilinx_CARRY4.hpp"
#include "PrimitiveComponents/Xilinx/Xilinx_LUT_compute.h"
#include <string>

using namespace std;

namespace flopoco{

XilinxGPC::XilinxGPC(Operator* parentOp, Target * target, vector<int> heights) : Compressor(parentOp, target)
{
    this->heights = heights;
    ostringstream name;

    //compressors are supposed to be combinatorial
    setCombinatorial();
    setShared();

    //remove the zero columns at the lsb
    while(heights[0] == 0)
    {
        heights.erase(heights.begin());
    }

    setWordSizes();
    createInputsAndOutputs();

    //set name:
    name << "XilinxGPC_";
    for(int i=heights.size()-1; i>=0; i--)
        name << heights[i];

    name << "_" << wOut;
    setNameWithFreqAndUID(name.str());

	declare("cc_di",4);
	declare("cc_s",4);

	vector<string> lutOp(4);
	vector<vector<string> > lutInputMappings(4,vector<string>(6));
	vector<string> ccDInputMappings(4);
	string carryInMapping;

	const string LUTConfigA = "69966996AAAAAAAA";
	const string LUTConfigB = "566A566AAAAAAAAA";
	const string LUTConfigC = "96696996AAAAAAAA";
	const string LUTConfigD = "3FFCFCC0FFF0F000";
	const string LUTConfigH = "6996966996696996";
	const string LUTConfigI = "177E7EE8FFF0F000";
	const string LUTConfigJ = "69966996C33CC33C";
	const string LUTConfigK = "9996966666666666";
	const string LUTConfigL = "78878778F00F0FF0";
    const string LUTConfigM = "E8818117177E7EE8";
    const string LUTConfigN = "177F7FFFE8808000";

	if(heights.size() == 3)
	{

        if((heights[2] == 6) && (heights[1] == 0) && (heights[0] == 6))
        {
            //(6,0,6;5) GPC:
            lutOp = {LUTConfigH, LUTConfigI, LUTConfigH, LUTConfigI};

            lutInputMappings[0] = {"X0" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"X0" + of(4),"X0" + of(5)};
            lutInputMappings[1] = {"X0" + of(1),"X0" + of(2),"X0" + of(3),"X0" + of(4),"X0" + of(5),"'1'"};
            lutInputMappings[2] = {"X2" + of(0),"X2" + of(1),"X2" + of(2),"X2" + of(3),"X2" + of(4),"X2" + of(5)};
            lutInputMappings[3] = {"X2" + of(1),"X2" + of(2),"X2" + of(3),"X2" + of(4),"X2" + of(5),"'1'"};

            ccDInputMappings = {"X0" + of(0),"O5","X2" + of(0),"O5"};

            carryInMapping="'0'";
        }


        else if((heights[2] == 6) && (heights[1] == 0) && (heights[0] == 7))
        {
            //(6,0,7;5) GPC:
                        lutOp = {LUTConfigH, LUTConfigI, LUTConfigH, LUTConfigI};

            lutInputMappings[0] = {"X0" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"X0" + of(4),"X0" + of(5)};
            lutInputMappings[1] = {"X0" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"X0" + of(4),"'1'"};
            lutInputMappings[2] = {"X2" + of(0),"X2" + of(1),"X2" + of(2),"X2" + of(3),"X2" + of(4),"X2" + of(5)};
            lutInputMappings[3] = {"X2" + of(1),"X2" + of(2),"X2" + of(3),"X2" + of(4),"X2" + of(5),"'1'"};

            ccDInputMappings = {"O5","O5","X2" + of(0),"O5"};

            carryInMapping="X0" + of(6);
        }

		else if((heights[2] == 6) && (heights[1] == 1) && (heights[0] == 5))
		{
			//(6,1,5;5) GPC:
			lutOp = {LUTConfigA, LUTConfigB, LUTConfigH, LUTConfigI};

			lutInputMappings[0] = {"X0" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"'0'","'1'"};
			lutInputMappings[1] = {"X1" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"'0'","'1'"};
			lutInputMappings[2] = {"X2" + of(0),"X2" + of(1),"X2" + of(2),"X2" + of(3),"X2" + of(4),"X2" + of(5)};
			lutInputMappings[3] = {"X2" + of(1),"X2" + of(2),"X2" + of(3),"X2" + of(4),"X2" + of(5),"'1'"};

			ccDInputMappings = {"O5","O5","X2" + of(0),"O5"};

			carryInMapping="X0" + of(4);
		}

		else if((heights[2] == 6) && (heights[1] == 2) && (heights[0] == 3))
		{
			//(6,2,3;5) GPC:
			lutOp = {LUTConfigA, LUTConfigA, LUTConfigH, LUTConfigI};

			lutInputMappings[0] = {"X0" + of(0),"X0" + of(1),"'0'","'0'","'0'","'1'"};
			lutInputMappings[1] = {"X1" + of(0),"X1" + of(1),"'0'","'0'","'0'","'1'"};
			lutInputMappings[2] = {"X2" + of(0),"X2" + of(1),"X2" + of(2),"X2" + of(3),"X2" + of(4),"X2" + of(5)};
			lutInputMappings[3] = {"X2" + of(1),"X2" + of(2),"X2" + of(3),"X2" + of(4),"X2" + of(5),"'1'"};

			ccDInputMappings = {"O5","O5","X2" + of(0),"O5"};

			carryInMapping="X0" + of(2);
		}
		else
		{
			stringstream s;
			s << "Unsupported GPC with column heights: ";
			for(int i=heights.size()-1; i >= 0; i--)
			{
				s << heights[i] << " ";
			}
			THROWERROR(s.str());
		}
	}
	else if(heights.size() == 4)
	{
		if((heights[3] == 1) && (heights[2] == 3) && (heights[1] == 2) && (heights[0] == 5))
		{
			//(1,3,2,5;5) GPC:
			lutOp = {LUTConfigJ, LUTConfigK, LUTConfigL, LUTConfigB};

			lutInputMappings[0] = {"X0" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"'0'","'1'"};
			lutInputMappings[1] = {"X1" + of(0),"X1" + of(1),"X0" + of(1),"X0" + of(2),"X0" + of(3),"'1'"};
			lutInputMappings[2] = {"X1" + of(0),"X1" + of(1),"X2" + of(0),"X2" + of(1),"X2" + of(2),"'1'"};
			lutInputMappings[3] = {"X3" + of(0),"X2" + of(0),"X2" + of(1),"X2" + of(2),"'0'","'1'"};

			ccDInputMappings = {"O5","O5","O5","O5"};

			carryInMapping="X0" + of(4);

		}
		else if((heights[3] == 1) && (heights[2] == 4) && (heights[1] == 1) && (heights[0] == 5))
		{
			//(1,4,1,5;5) GPC:
			lutOp = {LUTConfigA, LUTConfigB, LUTConfigA, LUTConfigB};

			lutInputMappings[0] = {"X0" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"'0'","'1'"}; //Only uses Configs 15-0 (AAAA) and 47-32 (6996) of LUTA
			lutInputMappings[1] = {"X1" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"'0'","'1'"}; //Only uses Configs 15-0 (AAAA) and 47-32 (566A) of LUTB
			lutInputMappings[2] = {"X2" + of(0),"X2" + of(1),"X2" + of(2),"X2" + of(3),"'0'","'1'"}; //Only uses Configs 15-0 (AAAA) and 47-32 (6996) of LUTA
			lutInputMappings[3] = {"X3" + of(0),"X2" + of(1),"X2" + of(2),"X2" + of(3),"'0'","'1'"}; //Only uses Configs 15-0 (AAAA) and 47-32 (566A) of LUTB

			ccDInputMappings = {"O5","O5","O5","O5"};

			carryInMapping="X0" + of(4);

		}
		else if((heights[3] == 1) && (heights[2] == 4) && (heights[1] == 0) && (heights[0] == 6))
		{
			//(1,4,0,6;5) GPC:
			lutOp = {LUTConfigC, LUTConfigD, LUTConfigA, LUTConfigB};

			lutInputMappings[0] = {"X0" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"X0" + of(4),"'1'"};
			lutInputMappings[1] = {"'0'","X0" + of(1),"X0" + of(2),"X0" + of(3),"X0" + of(4),"'1'"};
			lutInputMappings[2] = {"X2" + of(0),"X2" + of(1),"X2" + of(2),"X2" + of(3),"'0'","'1'"};
			lutInputMappings[3] = {"X3" + of(0),"X2" + of(1),"X2" + of(2),"X2" + of(3),"'0'","'1'"};

			ccDInputMappings = {"O5","O5","O5","O5"};

			carryInMapping="X0" + of(5);

		}

        else if((heights[3] == 1) && (heights[2] == 4) && (heights[1] == 0) && (heights[0] == 7))
        {
            //(1,4,0,7;5) GPC:
            lutOp = {LUTConfigH, LUTConfigI, LUTConfigA, LUTConfigB};

            lutInputMappings[0] = {"X0" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"X0" + of(4),"X0" + of(5)};
            lutInputMappings[1] = {"X0" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"X0" + of(4),"'1'"};
            lutInputMappings[2] = {"X2" + of(0),"X2" + of(1),"X2" + of(2),"X2" + of(3),"'0'","'1'"};
            lutInputMappings[3] = {"X3" + of(0),"X2" + of(1),"X2" + of(2),"X2" + of(3),"'0'","'1'"};

            ccDInputMappings = {"O5","O5","O5","O5"};

            carryInMapping="X0" + of(6);

        }

		else if((heights[3] == 2) && (heights[2] == 1) && (heights[1] == 1) && (heights[0] == 7))
        {
            //(2,1,1,7;5) GPC:
            lutOp = {LUTConfigH, LUTConfigM, LUTConfigN, LUTConfigA};

            lutInputMappings[0] = {"X0" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"X0" + of(4),"X0" + of(5)};  //done
            lutInputMappings[1] = {"X0" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"X0" + of(4),"X1" + of(0)};  //done
            lutInputMappings[2] = {"X0" + of(0),"X0" + of(1),"X0" + of(2),"X0" + of(3),"X0" + of(4),"X2" + of(0)};  //done
            lutInputMappings[3] = {"X3" + of(0),"X3" + of(1),"'0'","'0'","'0'","'1'"};                              //done

            ccDInputMappings = {"O5","X1" + of(0),"X2" + of(0),"X3" + of(1)};

            carryInMapping="X0" + of(6);

        }
		else
		{
			stringstream s;
			s << "Unsupported GPC with column heights: ";
			for(int i=heights.size()-1; i >= 0; i--)
			{
				s << heights[i] << " ";
			}
			THROWERROR(s.str());
		}
	}
	else
	{
		THROWERROR("Unsupported GPC with " << heights.size() << "columns");
	}
	//build LUTs for one slice
	for(int i=0; i <= 3; i++)
    {
        //LUT content of the LUTs exept the last LUT:
//        lut_op lutop_o6 = lut_in(0) ^ lut_in(1) ^ lut_in(2) ^ lut_in(3); //sum out of full adder xor input 3
//        lut_op lutop_o5 = (lut_in(0) & lut_in(1)) | (lut_in(0) & lut_in(2)) | (lut_in(1) & lut_in(2)); //carry out of full adder

        Xilinx_LUT6_2 *cur_lut = new Xilinx_LUT6_2(this,target);
		cur_lut->setGeneric( "init", "x\"" + lutOp[i] + "\"", 64 );

		for(int j=0; j <= 5; j++)
		{
			if(lutInputMappings[i][j].find("'") == string::npos)
			{
				//input is a dynamic signal
				inPortMap("i" + to_string(j),lutInputMappings[i][j]);
			}
			else
			{
				//input is a constant
				inPortMapCst("i" + to_string(j),lutInputMappings[i][j]);
			}

		}

		if(ccDInputMappings[i] == "O5")
		{
			outPortMap("o5","cc_di" + of(i));
		}
		else
		{
			outPortMap("o5","open");
			vhdl << tab << "cc_di(" << i << ") <= " << ccDInputMappings[i] << ";" << endl;
		}
        outPortMap("o6","cc_s" + of(i));

        vhdl << cur_lut->primitiveInstance(join("lut",i)) << endl;
    }

    Xilinx_CARRY4 *cur_cc = new Xilinx_CARRY4(this,target);

	inPortMapCst("ci", "'0'" );

	if(carryInMapping.find("'") == string::npos)
	{
		//input is a dynamic signal
		inPortMap("cyinit", carryInMapping);
	}
	else
	{
		//input is a constant
		inPortMapCst("cyinit", carryInMapping);
	}

    inPortMap( "di", "cc_di");
    inPortMap( "s", "cc_s");
    outPortMap( "co", "cc_co");
    outPortMap( "o", "cc_o");

    vhdl << cur_cc->primitiveInstance("cc") << endl;

	vhdl << tab << "R <= cc_co(3) & cc_o;" << endl;
}

OperatorPtr XilinxGPC::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args)
{
    string in;
    vector<int> heights;

    UserInterface::parseString(args, "columnHeights", &in);

    // tokenize the string, with ':' as a separator
    stringstream ss(in);
    while(ss.good())
    {
        string substr;

        getline(ss, substr, ',');
        heights.insert(heights.begin(), stoi(substr));
    }

    return new XilinxGPC(parentOp,target,heights);
}

void XilinxGPC::emulate(TestCase *tc, vector<int> heights) {

    this -> heights=heights;
    vector<mpz_class> sX (heights.size());

    mpz_class s=0;

    int maxheight = 0;

    for ( int i=0; i<(int)heights.size(); i++){
        if (heights[i]>maxheight)
            maxheight=heights[i];
    }
    int maxsum = 0;
    for ( int i=0; i<(int)heights.size(); i++){
        maxsum+=heights[i]*pow(2,i);
    }

    int maxbits=0;
    for(;maxsum!=0;maxsum=maxsum/2){
        maxbits++;
    }

    mpz_t int1;
    mpz_t int2;
    mpz_t curnbr;
    mpz_t add;
    mpz_t quotient;
    mpz_t sum;
    mpz_init2(int2,1);
    mpz_init2(int1,1);
    mpz_init2(curnbr, maxheight);
    mpz_init2(add, maxheight);
    mpz_init2(quotient, maxheight);
    mpz_init2(sum, maxbits);

    mpz_set_ui(int1,1);
    mpz_set_ui(int2,2);

    for ( int i=0; i <= (int)sX.size(); i++) {
        if (heights[i] != 0) {
            sX[i] = tc->getInputValue(join("X", i));
        }
    }

    for ( int i=0; i < (int)sX.size();i++){
        mpz_set_ui(curnbr,mpz_get_ui(sX[i].get_mpz_t()));
        for (int j=heights[i];j>=0;j--){
            mpz_fdiv_q_2exp(quotient, curnbr, j);
            if(mpz_cmp(quotient,int1)==0 ){
                sX[i]-= pow(2,j);
                mpz_pow_ui(add, int2, i);
                mpz_add(sum, sum, add);
            }
        }
    }
    s=(mpz_get_ui(sum));


    tc->addExpectedOutput("R", s);

}


void XilinxGPC::registerFactory(){
    UserInterface::add("XilinxGPC", // name
                       "Implements Xilinx optimized GPCs \
                       Available GPC sizes are: \
                       (6,0,6;5), (6,0,7;5), (6,1,5;5), (6,2,3;5) \
                       (1,3,2,5;5), (1,4,1,5;5), (1,4,0,6;5), (1,4,0,7;5), (2,1,1,7;5)",
                       "Primitives", // categories
                       "",
                       "columnHeights(string): comma separated list of heights for the columns of the compressor, \
in decreasing order of the weight. For example, columnHeights=\"6,0,6\" produces a (6,0,6:5) GPC",
                       "",
                       XilinxGPC::parseArguments
                       ) ;
}

BasicXilinxGPC::BasicXilinxGPC(Operator* parentOp_, Target * target, vector<int> heights) : BasicCompressor(parentOp_,target,heights)
{
	area = 4.0; //every target specific GPC uses 4 LUTs (so far)
}

Compressor* BasicXilinxGPC::getCompressor(){
	compressor = new XilinxGPC(parentOp, target, heights);
	return compressor;
}


} //namespace
