# to be completed

# freq, arith_in, N, M, bias, wS, wF, msb, lsb, ovf

# configs as first TERAS proposal. cartesian product between {alpha,beta,gamma} and ARITHS
configs_proposal1 = [
        (200,"posit:4:0"      ,128, 127,  4 , 3, 1,  3 , -4 , 0 ),  # posit<4,0> x alpha
        (200,"posit:4:0"      ,128, 127,  4 , 3, 1,  4 , -4 , 3 ),  # posit<4,0> x beta
        (200,"posit:4:0"      ,128, 127,  4 , 3, 1,  40, -50, 9 ),  # posit<4,0> x gamma

        (200,"posit:8:0"      ,64 ,  63,  6 , 4, 5,  5 , -8 , 2 ),  # posit<8,0> x alpha
        (200,"posit:8:0"      ,64 ,  63,  6 , 4, 5,  12, -12, 7 ),  # posit<8,0> x beta
        (200,"posit:8:0"      ,64 ,  63,  6 , 4, 5,  40, -50, 9 ),  # posit<8,0> x gamma

        (200,"posit:8:1"      ,64 ,  63,  12, 5, 4,  5 ,  -8, 2 ),  # posit<8,1> x alpha
        (200,"posit:8:1"      ,64 ,  63,  12, 5, 4,  24, -24, 15),  # posit<8,1> x beta
        (200,"posit:8:1"      ,64 ,  63,  12, 5, 4,  40, -50, 9 ),  # posit<8,1> x gamma

        (200,"posit:8:2"      ,64 ,  63,  24, 6, 3,  5 ,  -8, 2 ),  # posit<8,2> x alpha
        (200,"posit:8:2"      ,64 ,  63,  24, 6, 3,  48, -48, 31),  # posit<8,2> x beta
        (200,"posit:8:2"      ,64 ,  63,  24, 6, 3,  40, -50, 9 ),  # posit<8,2> x gamma

		(200,"posit:16:0"     ,32 ,  31,  14, 5,13,   5, -24, 2 ),  # posit<16,0> x alpha
        (200,"posit:16:0"     ,32 ,  31,  14, 5,13,  28, -28, 7 ),  # posit<16,0> x beta
        (200,"posit:16:0"     ,32 ,  31,  14, 5,13,  40, -50, 9 ),  # posit<16,0> x gamma

        (200,"posit:16:1"     ,32 ,  31,  28, 6,12,   5, -24, 2 ),  # posit<16,1> x alpha
        (200,"posit:16:1"     ,32 ,  31,  28, 6,12,  56, -56,15 ),  # posit<16,1> x beta
        (200,"posit:16:1"     ,32 ,  31,  28, 6,12,  40, -50, 9 ),  # posit<16,1> x gamma

        (200,"posit:16:2"     ,32 ,  31,  56, 7,11,   5, -24, 2 ),  # posit<16,2> x alpha
        (200,"posit:16:2"     ,32 ,  31,  56, 7,11, 112,-112,31 ),  # posit<16,2> x beta
        (200,"posit:16:2"     ,32 ,  31,  56, 7,11,  40, -50, 9 ),  # posit<16,2> x gamma

        (200,"posit:32:0"     ,16 ,  15,  30, 6,29,   5, -56, 2 ),  # posit<32,0> x alpha
        (200,"posit:32:0"     ,16 ,  15,  30, 6,29,  60, -60, 7 ),  # posit<32,0> x beta
        (200,"posit:32:0"     ,16 ,  15,  30, 6,29,  40, -50, 9 ),  # posit<32,0> x gamma

        (200,"posit:32:1"     ,16 ,  15,  60, 7,28,   5, -56, 2 ),  # posit<32,1> x alpha
        (200,"posit:32:1"     ,16 ,  15,  60, 7,28, 120,-120, 15),  # posit<32,1> x beta
        (200,"posit:32:1"     ,16 ,  15,  60, 7,28,  40, -50, 9 ),  # posit<32,1> x gamma

        (200,"posit:32:2"     ,16 ,  15, 120, 8,27,   5, -56, 2 ),  # posit<32,2> x alpha
        (200,"posit:32:2"     ,16 ,  15, 120, 8,27, 240,-240, 31),  # posit<32,2> x beta
        (200,"posit:32:2"     ,16 ,  15, 120, 8,27,  40, -50, 9 ),  # posit<32,2> x gamma

        (200,"posit:64:1"     ,8  ,   7, 124, 8,60,   5,-121, 2 ),  # posit<64,1> x alpha
        (200,"posit:64:1"     ,8  ,   7, 124, 8,60, 248,-248, 15),  # posit<64,1> x beta
        (200,"posit:64:1"     ,8  ,   7, 124, 8,60,  40, -50, 9 ),  # posit<64,1> x gamma

        (200,"posit:64:2"     ,8  ,   7, 248, 9,59,   5,-121, 2 ),  # posit<64,2> x alpha
        (200,"posit:64:2"     ,8  ,   7, 248, 9,59, 496,-496, 31),  # posit<64,2> x beta
        (200,"posit:64:2"     ,8  ,   7, 248, 9,59,  40, -50, 9 ),  # posit<64,2> x gamma

        (200,"posit:64:3"     ,8  ,   7, 496,10,58,   5,-121, 2 ),  # posit<64,3> x alpha
        (200,"posit:64:3"     ,8  ,   7, 496,10,58, 992,-992, 63),  # posit<64,3> x beta
        (200,"posit:64:3"     ,8  ,   7, 496,10,58,  40, -50, 9 ),  # posit<64,3> x gamma

        (200,"bfloat16"       ,32 ,  31, 127, 8, 7,   5, -24, 2 ),  # bfloat16 x alpha
        (200,"bfloat16"       ,32 ,  31, 127, 8, 7, 254,-266, 7 ),  # bfloat16 x beta
        (200,"bfloat16"       ,32 ,  31, 127, 8, 7,  40, -50, 9 ),  # bfloat16 x gamma

		(200,"ieee:5:10"      ,32 ,  31,  15, 5,10,   5, -24, 2 ),  # IEEE 754 HP x alpha
        (200,"ieee:5:10"      ,32 ,  31,  15, 5,10,  30, -48, 7 ),  # IEEE 754 HP x beta
        (200,"ieee:5:10"      ,32 ,  31,  15, 5,10,  40, -50, 9 ),  # IEEE 754 HP x gamma

		(200,"ieee:8:23"      ,16 ,  15, 127, 8,23,   5, -56, 2 ),  # IEEE 754 SP x alpha
		(200,"ieee:8:23"      ,16 ,  15, 127, 8,23, 254,-298, 7 ),  # IEEE 754 SP x beta
        (200,"ieee:8:23"      ,16 ,  15, 127, 8,23,  40, -50, 9 ),  # IEEE 754 SP x gamma

		(200,"ieee:11:52"     ,8  ,   7,1023,11,52,   5, -121,2 ),  # IEEE 754 DP x alpha
        (200,"ieee:11:52"     ,8  ,   7,1023,11,52,2046,-2148,7 ),  # IEEE 754 DP x beta
        (200,"ieee:11:52"     ,8  ,   7,1023,11,52,  40,  -50,9 ),  # IEEE 754 DP x gamma

		(200,"tfp:5:10"       ,32 ,  31,  15, 5,10,   5,  -24,2 ),  # TFP HP x alpha
        (200,"tfp:5:10"       ,32 ,  31,  15, 5,10,  30,  -28,5 ),  # TFP HP x beta
        (200,"tfp:5:10"       ,32 ,  31,  15, 5,10,  40,  -50,9 ),  # TFP HP x gamma

        (200,"tfp:8:23"       ,16 ,  15, 127, 8,23,   5,  -56,2 ),  # TFP SP x alpha
        (200,"tfp:8:23"       ,16 ,  15, 127, 8,23, 254, -252,5 ),  # TFP SP x beta
        (200,"tfp:8:23"       ,16 ,  15, 127, 8,23,  40,  -50,5 ),  # TFP SP x gamma

        (200,"tfp:11:52"      ,8  ,   7,1023,11,52,   5, -121,2 ),  # TFP DP x alpha
        (200,"tfp:11:52"      ,8  ,   7,1023,11,52,2046,-2044,5 ),  # TFP DP x beta
		(200,"tfp:11:52"      ,8  ,   7,1023,11,52,  40,  -50,9 ),  # TFP DP x gamma
]

configs_proposal2 = [
		## posits
        #(200,"posit:4:0"      , 80,  80,  4 , 3, 1,  3 , -4 , 0 ),  # posit<4,0> x approximate
        #(200,"posit:4:0"      , 80,  80,  4 , 3, 1,  4 , -4 , 3 ),  # posit<4,0> x exact (theory)
        #(200,"posit:4:0"      , 64,  64,  4 , 3, 1,  6 , -4 , 6 ),  # posit<4,0> x enhanced intuition

        #(200,"posit:8:2"      ,48 ,  48,  24, 6, 3,   6,  -8, 6 ),  # posit<8,2> x approximate
        #(200,"posit:8:2"      ,28 ,  28,  24, 6, 3,  48, -48, 31),  # posit<8,2> x exact (quire)

        #(200,"posit:16:2"     ,32 ,  31,  56, 7,11,   6, -24, 9 ),  # posit<16,2> x approximate
        #(200,"posit:16:2"     ,16 ,  16,  56, 7,11, 112,-112,31 ),  # posit<16,2> x exact (quire)

        (200,"posit:32:2"     ,12 ,  12, 120, 8,27,   9, -48, 9 ),  # posit<32,2> x approximate
        (200,"posit:32:2"     ,12 ,  12, 120, 8,27,   9, -10, 9 ),  # posit<32,2> x approximate
        #(200,"posit:32:2"     ,10 ,  10, 120, 8,27, 240,-240, 31),  # posit<32,2> x exact (quire)

        #(200,"posit:64:2"     ,6  ,   6, 248, 9,59,   6,-112, 9 ),  # posit<64,2> x approximate
        #(200,"posit:64:2"     ,6  ,   6, 248, 9,59, 496,-496, 31),  # posit<64,2> x exact (quire)

		## IEEE
		#(200,"ieee:5:10"      ,32 ,  31,  15, 5,10,   6, -24, 9 ),  # IEEE 754 HP x approximate
        #(200,"ieee:5:10"      ,28 ,  28,  15, 5,10,  30, -48, 7 ),  # IEEE 754 HP x exact (kulisch)

		#(200,"ieee:8:23"      ,16 ,  15, 127, 8,23,   6, -48, 9 ),  # IEEE 754 SP x approximate
		#(200,"ieee:8:23"      ,16 ,  15, 127, 8,23,   6, -10, 9 ),  # IEEE 754 SP x approximate
		(200,"ieee:8:23"      ,16 ,  15, 127, 8,23,   6, -38, 9 ),  # IEEE 754 SP x approximate
		(200,"ieee:8:23"      ,16 ,  15, 127, 8,23,   6, -28, 9 ),  # IEEE 754 SP x approximate
		(200,"ieee:8:23"      ,16 ,  15, 127, 8,23,   6,  -18, 9 ),  # IEEE 754 SP x approximate
		(200,"ieee:8:23"      ,16 ,  15, 127, 8,23,   7,  -7, 11 ),  # IEEE 754 SP x approximate
		(200,"ieee:8:23"      ,10 ,  10, 127, 8,23,   127, -128, 9 ),  # IEEE 754 SP x approximate
		#(200,"ieee:8:23"      ,10 ,  10, 127, 8,23, 254,-298, 7 ),  # IEEE 754 SP x exact (kulisch)

		#(200,"ieee:11:52"     ,8  ,   7,1023,11,52,   6, -112,9 ),  # IEEE 754 DP x approximate
        #(200,"ieee:11:52"     ,4  ,   4,1023,11,52,2046,-2148,7 ),  # IEEE 754 DP x exact (kulisch)

		## others
		#(200,"ieee:4:3"       ,48 ,  48,   7, 4, 3,   6,   -7,6 ),  # FP8 E4M3 x approximate
		#(200,"ieee:4:3"       ,48 ,  48,   7, 4, 3,  14,  -18,13),  # FP8 E4M3 x exact (kulisch)

		#(200,"ieee:5:2"       ,48 ,  48,  15, 5, 2,   6,  -7, 6),  # FP8 E5M2 x approximate
		#(200,"ieee:5:2"       ,48 ,  48,  15, 5, 2,  30, -40, 5),  # FP8 E5M2 x exact (kulisch)

        #(200,"bfloat16"       ,32 ,  31, 127, 8, 7,   5, -24, 2 ),  # bfloat16 x approximate
        (200,"bfloat16"       ,32 ,  31, 127, 8, 7,   6, -12, 10 ),  # bfloat16 x approximate
        (200,"bfloat16"       ,32 ,  31, 127, 8, 7,   6, -18, 2 ),  # bfloat16 x approximate
        (200,"bfloat16"       ,32 ,  31, 127, 8, 7,   10, -10, 2 ),  # bfloat16 x approximate
        #(200,"bfloat16"       ,10 ,  10, 127, 8, 7, 254,-266, 7 ),  # bfloat16 x exact (kulisch)

]


configs = configs_proposal2
