#ifndef __GEMM_H__
#define __GEMM_H__

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <getopt.h>
#include <ctype.h>


// #include <osnap_tools.h>
// #include <osnap_hls_if.h>
// #include <libosnap.h>
// #include <osnap_types.h>

//#include "../../common.h"
// #define CBLAS 1 // so we have c prototypes
#include "../../cblas.h"

#ifdef __cplusplus
extern "C" {
#endif
#define is_aligned(POINTER, BYTE_COUNT) \
	(((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

// add our oc-accel
#include "../../../oc-accel/software/include/osnap_tools.h"
#include "../../../oc-accel/software/include/osnap_hls_if.h"
#include "../../../oc-accel/software/include/libosnap.h"
#include "../../../oc-accel/software/include/osnap_types.h"

// add soft posit from cerlane to get the cast functions
#include "../../../SoftPosit/source/include/softposit.h"

/**
	@brief performs gemm blas3 routine
	@param m:
	@param n:
	@param k:
  */
//int gemm_backend_test(m, n, k, lda, ldb, ldc, transa, transb, sa, sb, gemm_p, gemm_q);
//int gemm_backend_test();


/* This number is unique and is declared in ~snap/ActionTypes.md */
#define ACTION_TYPE 0x86868604

static uint8_t verbose_level = 0;

#define VERBOSE0(file, fmt, ...) do {       \
		fprintf(file, fmt, ## __VA_ARGS__);  \
} while (0)

#define VERBOSE1(file, fmt, ...) do {       \
	if (verbose_level > 0)            \
		fprintf(file, fmt, ## __VA_ARGS__);  \
} while (0)

#define VERBOSE2(file, fmt, ...) do {       \
    if (verbose_level > 1)            \
        fprintf(file, fmt, ## __VA_ARGS__);  \
} while (0)


#define VERBOSE3(file, fmt, ...) do {       \
    if (verbose_level > 2)            \
        fprintf(file, fmt, ## __VA_ARGS__);  \
} while (0)

#define VERBOSE4(file, fmt, ...) do {       \
    if (verbose_level > 3)            \
        fprintf(file, fmt, ## __VA_ARGS__);  \
} while (0)

/* Data structure used to exchange information between action and application */
/* Size limit is 108 Bytes */
typedef struct action_job {
    struct snap_addr in;    /* input data */
    struct snap_addr out;   /* offset table */
    uint32_t read_burst_num;
    uint32_t write_burst_num;
    uint32_t transfer_type;
} action_job_t;

// static long get_nanos(void) {
//     struct timespec ts;
//     timespec_get(&ts, TIME_UTC);
//     return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
// }

static void *alloc_mem(int align, size_t size)
{
	void *a;
	size_t size2 = size + align;

	VERBOSE3(stdout, "%s Enter Align: %d Size: %zu (malloc Size: %zu)\n",
		__func__, align, size, size2);
	if (posix_memalign((void **)&a, align, size2) != 0) {
		perror("FAILED: posix_memalign()");
		return NULL;
	}
	VERBOSE3(stdout, "%s Exit %p\n", __func__, a);
	return a;
}

/**
 * @brief fills the MMIO registers / data structure
 *
 */
static void snap_prepare_action(struct snap_job *cjob,
                                struct action_job *mjob,
                                void *addr_in,
                                uint32_t size_in,
                                uint8_t type_in,
                                void *addr_out,
                                uint32_t size_out,
                                uint8_t type_out,
                                uint32_t read_burst_num,
                                uint32_t write_burst_num,
                                uint32_t transfer_type)
{
    assert(sizeof(*mjob) <= SNAP_JOBSIZE);
    memset(mjob, 0, sizeof(*mjob));

    // Setting input params : where text is located in host memory
    snap_addr_set(&mjob->in, addr_in, size_in, type_in, SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_SRC);

    // Setting output params : where result will be written in host memory
    snap_addr_set(&mjob->out, addr_out, size_out, type_out, SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_DST | SNAP_ADDRFLAG_END);

    mjob->read_burst_num = read_burst_num;
    mjob->write_burst_num = write_burst_num;
    mjob->transfer_type = transfer_type;

    snap_job_set(cjob, mjob, sizeof(*mjob), NULL, 0);
}

/**
 *
 * @brief load file into RAM
 *
 */
static size_t mem_init_from_file(const char *filename, char **result, int align ) {

    size_t size;

    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        *result = NULL;
        VERBOSE0(stderr, "Error: Could not read file %s\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // binary / raw data
    *result = (char *)(alloc_mem(align, sizeof(char)*(size)));

    if (size != fread(*result, sizeof(char), size, f)) {
        free(*result);
        VERBOSE0(stderr,"Error: read of file failed\n");
        exit(EXIT_FAILURE);
    }

    fclose(f);

    return size;

}

/**
 *
 * @brief save RAM buffer to file
 *
 */
static void save_file_from_memory(const char *filename, char **output_data, size_t size_out) {

    // open file
    FILE *FD_dst = fopen(filename, "wb");
    if (FD_dst == NULL) {
        VERBOSE0(stderr, "Error: Could not write to file %s\n", filename);
        exit(EXIT_FAILURE);
    }
    //VERBOSE0(stdout, "%p %zu", output_data, size_out);
    // write buffer
    if (fwrite(*output_data, sizeof(char), size_out, FD_dst) != size_out) {
        VERBOSE0(stderr, "Error: Failed to write %zu bytes to file\n", size_out);
        exit(EXIT_FAILURE);
    }

    // close file
    fclose(FD_dst);
}

// arithmetic conversions
//typedef unsigned short ushort;
typedef unsigned int uint;

static uint as_uint(const float x) {
    return *(uint*)&x;
}
static float as_float(const uint x) {
    return *(float*)&x;
}

static float half_to_float(const uint16_t x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint e = (x&0x7C00)>>10; // exponent
    const uint m = (x&0x03FF)<<13; // mantissa
    const uint v = as_uint((float)m)>>23; // evil log2 bit hack to count leading zeros in denormalized format
    return as_float((x&0x8000)<<16 | (e!=0)*((e+112)<<23|m) | ((e==0)&(m!=0))*((v-37)<<23|((m<<(150-v))&0x007FE000))); // sign : normalized : denormalized
}
static uint16_t float_to_half(const float x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint b = as_uint(x)+0x00001000; // round-to-nearest-even: add last bit after truncated mantissa
    const uint e = (b&0x7F800000)>>23; // exponent
    const uint m = b&0x007FFFFF; // mantissa; in line below: 0x007FF000 = 0x00800000-0x00001000 = decimal indicator flag - initial rounding
    return (b&0x80000000)>>16 | (e>112)*((((e-112)<<10)&0x7C00)|m>>13) | ((e<113)&(e>101))*((((0x007FF000+m)>>(125-e))+1)>>1) | (e>143)*0x7FFF; // sign : normalized : denormalized : saturate
}

static char* from_IFLOAT_to_bytes(
		IFLOAT* arith_in,
		uint8_t arith_type,
		uint8_t arithmetic_bitwidth,
		uint8_t arithmetic_param1,
		uint8_t arithmetic_param2,
		char* bytes_out) {
	if (arith_type == 0) {  // ieee
		//if (arithmetic_bitwidth == 1) {
		//	if (sizeof(IFLOAT)==4) { // from single to ieee8
		//		return (void*)(arith_in);
		//	}
		//	if (sizeof(IFLOAT)==8) { // from double to ieee8
		//		return (void*)(arith_in);
		//	}
		//}
		if (arithmetic_bitwidth == 2) {
			if (sizeof(IFLOAT)==4) { // from single to half
				VERBOSE3(stdout, "incoming float: %f\n", *arith_in);
				uint16_t tmp = float_to_half(*arith_in);
				memcpy(bytes_out, &tmp, 2);
				VERBOSE3(stdout, "half float in as uint16: %u\n", tmp);
			}
			if (sizeof(IFLOAT)==8) { // from double to half
				VERBOSE3(stdout, "incoming float as double: %f\n", *arith_in);
				float f_tmp = (float)(*arith_in);
				uint16_t tmp = float_to_half(f_tmp);
				VERBOSE3(stdout, "half float in as uint16: %u\n", tmp);
				memcpy(bytes_out, &tmp, 2);
			}
		}
		if (arithmetic_bitwidth == 4) {
			if (sizeof(IFLOAT)==4) { // from single to single
				memcpy(bytes_out, arith_in, 4);
			}
			if (sizeof(IFLOAT)==8) { // from double to single
				float tmp = (float)(*arith_in);  // cast IFLOAT to float
				memcpy(bytes_out, &tmp, 4);
			}

		}
		if (arithmetic_bitwidth == 8) {
			if (sizeof(IFLOAT)==4) { // from single to double
				double tmp = (double)(*arith_in);
				memcpy(bytes_out, &tmp, 8);
			}
			if (sizeof(IFLOAT)==8) { // from double to double
				memcpy(bytes_out, arith_in, 8);
			}
		}
	}
	// if (arith_type == 1) {  // tfp
	// 	return NULL;  // we do not implement tfp for the moment
	// }
	if (arith_type == 2) {  // bfloat16
		if (sizeof(IFLOAT)==4) { // from single to bfloat16
			memcpy(bytes_out, arith_in, 2); // truncate taking the first half
		}
		if (sizeof(IFLOAT)==8) { // from double to bfloat16
			float tmp_f = (float)(*arith_in);
			memcpy(bytes_out, &tmp_f, 2); // truncate taking the first half
		}
	}
	if (arith_type == 3) {  // posit
		// if (arithmetic_bitwidth == 1) {
		// 	if (sizeof(IFLOAT)==4) { // from single to posit82
		// 	return (void*)(arith_in);
		// 	}
		// 	if (sizeof(IFLOAT)==8) { // from double to posit82
		// 	return (void*)(arith_in);
		// 	}
		// }
		// // if (arithmetic_bitwidth == 2) {
		// 	if (sizeof(IFLOAT)==4) { // from single to posit162
		// 	return (void*)(arith_in);
		// 	}
		// 	if (sizeof(IFLOAT)==8) { // from double to posit162
		// 	return (void*)(arith_in);
		// 	}

		// }
		if (arithmetic_bitwidth == 4) {
			if (sizeof(IFLOAT)==4) { // from single to posit322
				double tmp_d = (double)(*arith_in);
				VERBOSE3(stdout, "incoming float as double: %f\n", tmp_d);
				posit_2_t tmp_px2 = convertDoubleToPX2(tmp_d, 32);
				//posit_2_t tmp_px2_2 = convertDoubleToPX2(tmp_d, 16);
				//posit_2_t tmp_px2_3 = convertDoubleToPX2(tmp_d, 8);
				//uint64_t tmp_ui64_2 = pX2_int(tmp_px2_2);
				//uint64_t tmp_ui64_3 = pX2_int(tmp_px2_3);
				VERBOSE4(stdout, "posit32 2 as uint32 in struct: %u\n", tmp_px2.v);
				memcpy(bytes_out, &(tmp_px2.v), 4);

				posit_2_t tmp_px2_2 = { .v = 0 };
				memcpy(&(tmp_px2_2.v), bytes_out, 4);
				double tmp_d_2 = convertPX2ToDouble(tmp_px2_2);
				VERBOSE4(stdout, "double back to verify: %f\n", tmp_d_2);

			}
			if (sizeof(IFLOAT)==8) { // from double to posit322
			return (void*)(arith_in);
			}

		}
		// if (arithmetic_bitwidth == 8) {
		// 	if (sizeof(IFLOAT)==4) { // from single to posit642
		// 	return (void*)(arith_in);
		// 	}
		// 	if (sizeof(IFLOAT)==8) { // from double to posit642
		// 	return (void*)(arith_in);
		// 	}

		// }
	}
	return bytes_out;
}

static IFLOAT from_bytes_to_IFLOAT(
		char* bytes_in,
		uint8_t arith_type,
		uint8_t arithmetic_bitwidth,
		uint8_t arithmetic_param1,
		uint8_t arithmetic_param2) {
	if (arith_type == 0) {  // ieee
		//if (arithmetic_bitwidth == 1) {
		//	if (sizeof(IFLOAT)==4) {
		//		return (IFLOAT*)arith_in;
		//	}
		//	if (sizeof(IFLOAT)==8) {
		//		return (IFLOAT*)(arith_in);
		//	}
		//}
		if (arithmetic_bitwidth == 2) {
			if (sizeof(IFLOAT)==4) {
				uint16_t tmp_u16;
				memcpy(&tmp_u16, bytes_in, 2);
				VERBOSE3(stdout, "incoming bytes as uint16: %u\n", tmp_u16);
				float tmp_f = half_to_float(tmp_u16);
				VERBOSE3(stdout, "outgoing float: %f\n", tmp_f);
				return tmp_f;
			}
			if (sizeof(IFLOAT)==8) {
				uint16_t tmp_u16;
				memcpy(&tmp_u16, bytes_in, 2);
				float tmp_f = half_to_float(tmp_u16);
				double tmp_d = (double)tmp_f;
				return tmp_d;
			}

		}
		if (arithmetic_bitwidth == 4) {
			if (sizeof(IFLOAT)==4) {
				IFLOAT tmp=0.0f;
				memcpy(&tmp, bytes_in, 4);
				return tmp;
			}
			if (sizeof(IFLOAT)==8) { // from single to double
				float tmp_f;
				double tmp_d;
				memcpy(&tmp_f, bytes_in, 4);
				tmp_d=(double)tmp_f;
				return tmp_d;
			}

		}
		//if (arithmetic_bitwidth == 8) {
		//	if (sizeof(IFLOAT)==4) {
		//		return (IFLOAT*)(arith_in);
		//	}
		//	if (sizeof(IFLOAT)==8) {
		//		return (IFLOAT*)(arith_in);
		//	}

		//}
	}
	// if (arith_type == 1) {  // tfp
	// 	return NULL;  // we do not implement tfp for the moment
	// }
	if (arith_type == 2) {  // bfloat16
		if (sizeof(IFLOAT)==4) {
			float tmp_f=0.0f;
			memcpy(&tmp_f,bytes_in,2);
			return tmp_f;
		}
		if (sizeof(IFLOAT)==8) {
			float tmp_f=0.0f;
			memcpy(&tmp_f,bytes_in,2);
			double tmp_d=(double)tmp_f;
			return tmp_d;
		}
	}
	if (arith_type == 3) {  // posit
		// if (arithmetic_bitwidth == 1) {
		// 	if (sizeof(IFLOAT)==4) {
		// 	return (IFLOAT*)(arith_in);
		// 	}
		// 	if (sizeof(IFLOAT)==8) {
		// 	return (IFLOAT*)(arith_in);
		// 	}
		// }
		// if (arithmetic_bitwidth == 2) {
		// 	if (sizeof(IFLOAT)==4) {
		// 	return (IFLOAT*)(arith_in);
		// 	}
		// 	if (sizeof(IFLOAT)==8) {
		// 	return (IFLOAT*)(arith_in);
		// 	}
		// }
		if (arithmetic_bitwidth == 4) {
			if (sizeof(IFLOAT)==4) {
				posit_2_t tmp_px2 = { .v = 0 };
				memcpy(&(tmp_px2.v), bytes_in, 4);
				double tmp_d = convertPX2ToDouble(tmp_px2);
				float tmp_f = (float)tmp_d;
				return tmp_f;
			}
			if (sizeof(IFLOAT)==8) {
				posit_2_t tmp_px2 = { .v = 0 };
				memcpy(&(tmp_px2.v), bytes_in, 4);
				double tmp_d = convertPX2ToDouble(tmp_px2);
				return tmp_d;
			}
		}
		// if (arithmetic_bitwidth == 8) {
		// 	if (sizeof(IFLOAT)==4) {
		// 	return (IFLOAT*)(arith_in);
		// 	}
		// 	if (sizeof(IFLOAT)==8) {
		// 	return (IFLOAT*)(arith_in);
		// 	}
		// }
	}
}

/**
  @param void *a: pointer to input matrix A(where op( A ) is m*k)
  @param void *b: pointer to input matrix B(where op( B ) is k*n)
  @param void *c: pointer to output matrix C(m*k)
  @param void *alpha: scalar alpha of matrix A
  @param void *beta: scalar beta of matrix B
  @param BLASLONG m: specifies  the number  of rows  of the  matrix
  	   op( A )  and of the  matrix  C.  M  must  be at least  zero
  @param BLASLONG n: specifies the number  of columns of the matrix
           op( B ) and the number of columns of the matrix C. N must be
           at least zero.
  @param BLASLONG k: specifies  the number of columns of the matrix
           op( A ) and the number of rows of the matrix op( B ). K must
           be at least  zero.
	   It is the so called (by me) common dimension.
  @param BLASLONG lda
  @param BLASLONG ldb
  @param BLASLONG ldc
*/
static int gemm_backend_test (
		uint64_t m,
		uint64_t n,
		uint64_t k,
		IFLOAT *ALPHA,
		IFLOAT *BETA,
		IFLOAT *A,
		IFLOAT *B,
		IFLOAT *C,
		uint64_t lda,
		uint64_t ldb,
		uint64_t ldc,
		int transA,
		int transB) {



    // Set verbosity
    char *pTmp = NULL;
    if (( pTmp = getenv( "VERBOSITY" )) != NULL )
        verbose_level = atoi(pTmp);

    //if (is_aligned(A, 8192)) {printf("8192 aligned\n");}
    //else {printf("not 8192 aligned\n");}
    //if (is_aligned(A, 4096)) {printf("4096 aligned\n");}
    //else {printf("not 4096 aligned\n");}
    //if (is_aligned(A, 2048)) {printf("2048 aligned\n");}
    //else {printf("not 2048 aligned\n");}
    //if (is_aligned(A, 1024)) {printf("1024 aligned\n");}
    //else {printf("not 1024 aligned\n");}
    //if (is_aligned(A, 512)) {printf("512 aligned\n");}
    //else {printf("not 512 aligned\n");}
    //if (is_aligned(A, 256)) {printf("256 aligned\n");}
    //else {printf("not 256 aligned\n");}
    //if (is_aligned(A, 128)) {printf("128 aligned\n");}
    //else {printf("not 128 aligned\n");}
    //if (is_aligned(A, 64)) {printf("64 aligned\n");}
    //else {printf("not 64 aligned\n");}

    VERBOSE2(stdout, "m=%lld, n=%lld, k=%lld\n", m, n, k);
    VERBOSE2(stdout, "transA=%d, transB=%d\n", transA, transB);

#if defined(DOUBLE)
    VERBOSE3(stdout, "float type is double\n");
#else
    VERBOSE3(stdout, "float type is single\n");
#endif

    int rc = 0;
    int card_no = 0;
    struct snap_card *card = NULL;
    struct snap_action *action = NULL;
    char device[128];
    struct snap_job cjob;
    struct action_job mjob;
    unsigned long timeout = 360*2;  // 12 min
    //unsigned long timeout = 180;  // 3 min
    struct timeval etime_card_allocation, stime_card_allocation,
		   etime_attach_action, stime_attach_action,
		   etime_memory_allocation, stime_memory_allocation,
		   etime_memory_prepare, stime_memory_prepare,
		   etime_action_prepare, stime_action_prepare,
		   etime_action_execution, stime_action_execution;


    snap_action_flag_t action_irq = 0; //(SNAP_ACTION_DONE_IRQ | SNAP_ATTACH_IRQ); //no irq for now; snap_action_flag_t is an enum defined in snaplib


    // Allocate Card
    gettimeofday(&stime_card_allocation, NULL);
    if(card_no == 0) {
        snprintf(device, sizeof(device)-1, "IBM,oc-snap");
    } else {
        snprintf(device, sizeof(device)-1, "/dev/ocxl/IBM,oc-snap.000%d:00:00.1.0", card_no);
    }
    card = snap_card_alloc_dev(device, SNAP_VENDOR_ID_IBM, SNAP_DEVICE_ID_SNAP); //Constants defined in snaplib
    if (card == NULL) {
        VERBOSE0(stderr, "err: failed to open card %u: %s\n", card_no, strerror(errno));
        goto out_error;
    }
    gettimeofday(&etime_card_allocation, NULL);
    VERBOSE3(stdout, "Card Allocated Successfully\n");


    // Attach Action
    gettimeofday(&stime_attach_action, NULL);
    action = snap_attach_action(card, ACTION_TYPE, action_irq, 180);
    if (action == NULL) {
        VERBOSE0(stderr, "err: failed to attach action %u: %s\n", card_no, strerror(errno));
        goto out_error1;
    }
    gettimeofday(&etime_attach_action, NULL);
    VERBOSE3(stdout, "Action Attached Successfully\n");


    // Retrieve HW-accelerated kernel informations
    uint32_t reg=0;

    // fetch fromm HW registers the arithmetic type of the systolic kernel
    snap_action_read32 (card, ACTION_TYPE_REG, &reg);
    VERBOSE3(stdout, "test TYPE SA from register polling %u\n", reg);
    uint8_t arithmetic_bitwidth_bits = (reg & 0x00FF0000) >> 16; // in bits
    uint8_t arithmetic_bitwidth      = arithmetic_bitwidth_bits >> 3;
    uint8_t arithmetic_type          = (reg & 0x0000FF00) >> 8;  // ieee=0;tfp=1;bf16=2;posit=3
    VERBOSE3(stdout, "Arith bitwidth in bytes is: %u\n", arithmetic_bitwidth);
    VERBOSE3(stdout, "Arith type is: %u\n", arithmetic_type);

    // fetch fromm HW registers the dimensions of the systolic kernel
    snap_action_read32 (card, ACTION_RELEASE_REG, &reg);
    VERBOSE3(stdout, "test RELEASE SA from register polling %u\n", reg);
    uint8_t systolic_array_rows    = (reg & 0xFF000000) >> 24;
    uint8_t systolic_array_columns = (reg & 0x00FF0000) >> 16;
    uint8_t arithmetic_param1      = (reg & 0x0000FF00) >>  8;
    uint8_t arithmetic_param2      = (reg & 0x000000FF) >>  0;
    VERBOSE3(stdout, "SA rows: %u\n", systolic_array_rows);
    VERBOSE3(stdout, "SA cols: %u\n", systolic_array_columns);
    VERBOSE3(stdout, "arithmetic param1: %u\n", arithmetic_param1);
    VERBOSE3(stdout, "arithmetic param2: %u\n", arithmetic_param2);
    if (systolic_array_rows == 0) {
	    rc = 0x86;
	    goto out_error2;  // certainly a bad bistream
    }
    if (k < systolic_array_rows) {
        rc = 0x86;
        goto out_error2; //  signal interface we can't ; to continue process in cpu
    }


    // Allocate memories (in and out)
    // Reallocation is needed for alignment and data conversion
    uint16_t fpga_bus_size = 128; // in bytes, 128 for opencapi, 64 for capi1 and capi2
    uint64_t entire_horizontal_bands_matrix_op_A = m / systolic_array_rows;
    uint8_t rows_last_partial_band_matrix_op_A = m % systolic_array_rows;
    uint64_t entire_vertical_bands_matrix_op_B = n / systolic_array_columns;
    uint8_t cols_last_partial_band_matrix_op_B = n % systolic_array_columns;
    VERBOSE3(stdout, "entire horizontal bands matrix A: %d\n", entire_horizontal_bands_matrix_op_A);
    VERBOSE3(stdout, "entire vertical bands matrix b: %d\n", entire_vertical_bands_matrix_op_B);
    VERBOSE3(stdout, "rows last band matrix A: %d\n", rows_last_partial_band_matrix_op_A);
    VERBOSE3(stdout, "columns last band matrix b: %d\n", cols_last_partial_band_matrix_op_B);

    bool horizontal_padding_case         = (rows_last_partial_band_matrix_op_A>0);
    entire_horizontal_bands_matrix_op_A += horizontal_padding_case? 1 : 0;
    bool vertical_padding_case           = (cols_last_partial_band_matrix_op_B>0);
    entire_vertical_bands_matrix_op_B   += (vertical_padding_case)? 1 : 0;
    VERBOSE3(stdout, "case vertical padding: %d\n", vertical_padding_case);
    VERBOSE3(stdout, "case horizontal padding: %d\n", horizontal_padding_case);

    size_t aggregate_dma_memory_size = (fpga_bus_size*k*entire_horizontal_bands_matrix_op_A*entire_vertical_bands_matrix_op_B);
    size_t mem_out_size = systolic_array_rows*fpga_bus_size*entire_horizontal_bands_matrix_op_A*entire_vertical_bands_matrix_op_B;
    VERBOSE3(stdout,"size in: %zu\n", aggregate_dma_memory_size);
    VERBOSE3(stdout,"size out: %zu\n", mem_out_size);


    gettimeofday(&stime_memory_allocation, NULL);
    // we perform 8192 bytes alignment to match arsize / arlen of fpga logic. bursts of 64 transfers of 128B
    char *aggregate_dma_memory = (char *)(alloc_mem(8192, sizeof(char)*aggregate_dma_memory_size));
    char *mem_out = (char*)(alloc_mem(8192, sizeof(char)*mem_out_size));
    gettimeofday(&etime_memory_allocation, NULL);

    if (verbose_level > 3 ) {
        __hexdump(stdout, (IFLOAT*)A,m*k*sizeof(IFLOAT));
        __hexdump(stdout, (IFLOAT*)B,k*n*sizeof(IFLOAT));
        __hexdump(stdout, (IFLOAT*)C,m*n*sizeof(IFLOAT));
    }

    IFLOAT arith_scratchpad=0.0f;  // incoming arithmetic word from high level software (generally single or double precision float)
    char* arithmetic_bytes_scratchpad = (char*)malloc(arithmetic_bitwidth*sizeof(char));


    // take, cast and place elements of A
    gettimeofday(&stime_memory_prepare, NULL);
    for (uint64_t row_band_i=0 ; row_band_i < entire_horizontal_bands_matrix_op_A ; ++row_band_i) {
        for (uint64_t row_i=0 ; row_i < systolic_array_rows ; ++row_i) {
            for (uint64_t col_j=0 ; col_j < k ; ++col_j) {
                if (horizontal_padding_case && row_band_i==(entire_horizontal_bands_matrix_op_A-1) && (row_i>=rows_last_partial_band_matrix_op_A)) {
                    arith_scratchpad = 0.0f;
                } else {
                    if (transA==0) {
                                arith_scratchpad = A[(row_band_i*systolic_array_rows) + (col_j*lda) + (row_i)];
                    } else {
                                arith_scratchpad = A[(row_band_i*lda*systolic_array_rows) + (lda*row_i) + (col_j)];
                    }
                }
                from_IFLOAT_to_bytes(&arith_scratchpad, arithmetic_type, arithmetic_bitwidth, arithmetic_param1, arithmetic_param2, arithmetic_bytes_scratchpad);
                for (uint64_t rewrite_i=0 ; rewrite_i < entire_vertical_bands_matrix_op_B ; ++rewrite_i) {
                    memcpy( aggregate_dma_memory +
                        (row_band_i*entire_vertical_bands_matrix_op_B*fpga_bus_size*k) +
                        (rewrite_i*k*fpga_bus_size) +
                        (fpga_bus_size*col_j) +
                        (row_i*arithmetic_bitwidth), // end address calculation
                        arithmetic_bytes_scratchpad,
                        arithmetic_bitwidth);
                }
            }
        }
    }
    // take, cast and place elements of B
    for (uint64_t col_band_i=0 ; col_band_i < entire_vertical_bands_matrix_op_B ; ++col_band_i) {
        for (uint64_t col_i=0 ; col_i < systolic_array_columns ; ++col_i) {
            for (uint64_t row_j=0 ; row_j < k ; ++row_j) {
                if (vertical_padding_case && col_band_i==(entire_vertical_bands_matrix_op_B-1) && (col_i>=cols_last_partial_band_matrix_op_B)) {
                    arith_scratchpad = 0.0f;
                } else {
                    if(transB==0) {
                        arith_scratchpad = B[(col_band_i*ldb*systolic_array_columns) + (ldb*col_i) + (row_j)];
                    } else {
                        arith_scratchpad = B[(col_band_i*systolic_array_columns) + (ldb*row_j) + (col_i)];
                    }
                }
                from_IFLOAT_to_bytes(&arith_scratchpad, arithmetic_type, arithmetic_bitwidth, arithmetic_param1, arithmetic_param2, arithmetic_bytes_scratchpad);
                for (uint64_t rewrite_i=0 ; rewrite_i < entire_horizontal_bands_matrix_op_A ; ++rewrite_i) {
                    memcpy( aggregate_dma_memory +
                        (col_band_i*fpga_bus_size*k) +
                        (rewrite_i*entire_vertical_bands_matrix_op_B*k*fpga_bus_size) +
                        (fpga_bus_size*row_j) +
                        //(systolic_array_rows*arithmetic_bitwidth) + // offset
                        (fpga_bus_size >> 1) + // offset
                        (col_i*arithmetic_bitwidth), // end address calculation
                        arithmetic_bytes_scratchpad,
                        arithmetic_bitwidth);
                }
            }
        }
    }

    // place Start Of Block (SOB) and End Of Block (EOB) bits
    for (uint64_t i=0 ; i < aggregate_dma_memory_size ; ++i) {
	uint64_t bus_index = i % fpga_bus_size;
	uint64_t col_index = (i/fpga_bus_size) % k;
	if (bus_index == 127) {
	    if (col_index == 0) {
                aggregate_dma_memory[i] = 0x40;  // SOB
	    } else if (col_index==k-1) {
                aggregate_dma_memory[i] = 0x80;  // EOB
	    } else {
                aggregate_dma_memory[i] = 0x00;
	    }
	}
    }
    gettimeofday(&etime_memory_prepare, NULL);
    if (verbose_level > 3 ) {
        __hexdump(stdout, aggregate_dma_memory, aggregate_dma_memory_size);
    }


    // Prepare action for DMA
    uint8_t  type_in  = SNAP_ADDRTYPE_HOST_DRAM;
    uint8_t  type_out = SNAP_ADDRTYPE_HOST_DRAM;
    uint32_t read_burst_num  = 64; // fpga has logic only for 7 arlen
    uint32_t write_burst_num = 64; // fpga has logic only for 7 awlen
    uint32_t transfer_type = 4; // host to host
    gettimeofday(&stime_action_prepare, NULL);
    uint64_t addr_in = 0x0ull;
    uint64_t addr_out = 0x0ull;
    addr_in  = (unsigned long)aggregate_dma_memory;
    addr_out = (unsigned long)mem_out;
    snap_prepare_action(&cjob,
                        &mjob,
                        (void *)addr_in,
                        aggregate_dma_memory_size,
                        type_in,
                        (void *)addr_out,
                        mem_out_size,
                        type_out,
                        read_burst_num,
                        write_burst_num,
                        transfer_type
    );
    gettimeofday(&etime_action_prepare, NULL);


    // Execute Action
    if (verbose_level > 3 ) {
        __hexdump(stdout, aggregate_dma_memory, aggregate_dma_memory_size);
    }
    gettimeofday(&stime_action_execution, NULL);
    rc = snap_action_sync_execute_job(action, &cjob, timeout);
    gettimeofday(&etime_action_execution, NULL);
    if (rc != 0) {
        VERBOSE0(stdout, "err: job execution %d: %s!\n", rc, strerror(errno));
        goto out_error3;
    }
    if (cjob.retc == SNAP_RETC_SUCCESS) {
        VERBOSE3(stdout, "SUCCESS\n");
    }
    else {
        VERBOSE0(stdout, "FAILED\n");
        VERBOSE0(stdout, "err: Unexpected RETC=%x!\n", cjob.retc);
        goto out_error3;
    }


    // Printing Results if enough verbosity
    //VERBOSE2( stdout, "Sent (%zu Bytes) took %lld usec, so BW = %lld MiBps \n",
    //          Data_Size_in,
    //          (long long)timediff_usec(&etime,  &stime),
    //          ((Data_Size_in/((long long)timediff_usec(&etime,  &stime)))/1)
    //        );

    //uint64_t total_arithmetic_ops = (uint64_t)((M*N)+(N*M))*BLOCKS*P;
    // VERBOSE2 ( stdout, "%lld %lld %lld %lld %lld %lld %f %lld %lld\n",
    //            (long long)N*1,
    //            (long long)M*1,
    //            (long long)B*1,
    //            (long long)P*1,
    //            (long long)N*M*B*P,
    //            (long long)N*M*B*P*2,
    //            (double)N*M*B*P*2,
    //            (long long) b-a,
    //            (long long)timediff_usec(&etime,  &stime)

    //         );
    // VERBOSE2( stdout, "%zu %lld %f %f\n",
    //           Data_Size_in,
    //           (long long)timediff_usec(&etime,  &stime),
    //           (((float)(Data_Size_in+Data_Size_out)/((long long)timediff_usec(&etime,  &stime)))),
    //           ((double)(total_arithmetic_ops)/((long long)timediff_usec(&etime,  &stime)))
    //         );

    if (verbose_level > 3 ) {
        __hexdump(stdout, mem_out, mem_out_size);
    }


    // Write Matrix C to out
    for (uint32_t vertical_band_j=0 ; vertical_band_j < entire_horizontal_bands_matrix_op_A ; ++vertical_band_j) {
        for (uint32_t horizontal_block_i=0 ; horizontal_block_i < entire_vertical_bands_matrix_op_B ; ++horizontal_block_i) {
            for (uint32_t row_i=0 ; row_i < systolic_array_rows ; ++row_i) {  // row_i is reversed as the array exits from the bottom
                for (uint32_t col_j=0 ; col_j < systolic_array_columns ; ++col_j) {
    		    char * c_tmp = mem_out +
                                (vertical_band_j * entire_vertical_bands_matrix_op_B * fpga_bus_size * systolic_array_rows) +
                                (horizontal_block_i * fpga_bus_size * systolic_array_rows) +
                                (fpga_bus_size * row_i) +
                                (arithmetic_bitwidth*col_j);
    		    if ( !(horizontal_padding_case &&
    		           vertical_band_j==entire_horizontal_bands_matrix_op_A-1 &&
    		           row_i <= systolic_array_rows-1-rows_last_partial_band_matrix_op_A) &&
    		         !(vertical_padding_case   &&
    		           horizontal_block_i==entire_vertical_bands_matrix_op_B-1 &&
    		           col_j >= cols_last_partial_band_matrix_op_B)
    		       ) {
    		            arith_scratchpad = from_bytes_to_IFLOAT(c_tmp, arithmetic_type, arithmetic_bitwidth, arithmetic_param1, arithmetic_param2);
		            if (*BETA == 0.0f) {  // we consider beta is 0 or 1 to avoid a multiplication
    		            	C[(horizontal_block_i*ldc*systolic_array_columns)+
    		            	  (vertical_band_j*systolic_array_rows)+
    		            	  ((systolic_array_rows-1-row_i))+
    		            	  col_j*ldc
    		            	] = arith_scratchpad;
    		            } else if (*BETA == 1.0f) {
    		            	C[(horizontal_block_i*ldc*systolic_array_columns)+
    		            	  (vertical_band_j*systolic_array_rows)+
    		            	  ((systolic_array_rows-1-row_i))+
    		            	  col_j*ldc
    		            	] += arith_scratchpad;
    		            }
    		    }
                }
            }
        }
    }
    if (verbose_level > 3) {
        __hexdump(stdout, C, sizeof(IFLOAT)*n*m);
    }

    // print out the different times
    if (verbose_level > 2) {
        uint64_t time_card_allocation = timediff_usec(&etime_card_allocation,  &stime_card_allocation);
        uint64_t time_attach_action = timediff_usec(&etime_attach_action,  &stime_attach_action);
        uint64_t time_memory_allocation = timediff_usec(&etime_memory_allocation,  &stime_memory_allocation);
        uint64_t time_memory_preparation = timediff_usec(&etime_memory_prepare,  &stime_memory_prepare);
        uint64_t time_prepare_action = timediff_usec(&etime_action_prepare,  &stime_action_prepare);
        uint64_t time_action_execute = timediff_usec(&etime_action_execution,  &stime_action_execution);
	uint64_t time_total = time_card_allocation + time_attach_action + time_memory_allocation + time_memory_preparation + time_prepare_action + time_action_execute;
	VERBOSE3(stdout, "time card allocation (us): %lld, %lld\%\n",time_card_allocation, (100*time_card_allocation/time_total));
	VERBOSE3(stdout, "time attach action (us): %lld, %lld\%\n",time_attach_action, (100*time_attach_action/time_total));
	VERBOSE3(stdout, "time memory allocation (us): %lld, %lld\%\n",time_memory_allocation, (100*time_memory_allocation/time_total));
	VERBOSE3(stdout, "time memory preparation (us): %lld, %lld\%\n",time_memory_preparation, (100*time_memory_preparation/time_total));
	VERBOSE3(stdout, "time action prepare(us): %lld, %lld\%\n",time_prepare_action, (100*time_prepare_action/time_total));
	VERBOSE3(stdout, "time action execute(us): %lld, %lld\%\n",time_action_execute, (100*time_action_execute/time_total));

    }


    // Detach action
    snap_detach_action(action);


    // Deallocate the card
    snap_card_free(card);


    // Deallocate input and output matrices
    //if (transB==0) {
    //	free(B_T);
    //}
    //if (transA==1) {
    //    free(A);
    //}
    free(arithmetic_bytes_scratchpad);
    free(mem_out);
    free(aggregate_dma_memory);
    //exit(EXIT_SUCCESS);
    return 0;

    out_error3:
    	free(arithmetic_bytes_scratchpad);
	free(aggregate_dma_memory);
        free(mem_out);
    out_error2:
        snap_detach_action(action);
    out_error1:
        snap_card_free(card);
    out_error:
    	// if (transB==0) {
    	//     free(B_T);
    	// }
        // if (transA==1) {
        //     free(A);
        // }
        // exit(EXIT_FAILURE);
	return rc;

}

#ifdef __cplusplus
}
#endif

#endif	// __GEMM_H__
