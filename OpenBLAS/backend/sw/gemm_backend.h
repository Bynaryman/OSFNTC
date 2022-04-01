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

#include "../../../oc-accel/software/include/osnap_tools.h"
#include "../../../oc-accel/software/include/osnap_hls_if.h"
#include "../../../oc-accel/software/include/libosnap.h"
#include "../../../oc-accel/software/include/osnap_types.h"

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
		void *alpha,
		void *beta,
		void *a,
		void *b,
		void *c,
		uint64_t lda,
		uint64_t ldb,
		uint64_t ldc) {



    // Set verbosity
    char *pTmp = NULL;
    if (( pTmp = getenv( "VERBOSITY" )) != NULL )
        verbose_level = atoi(pTmp);


    int rc = 0;
    int card_no = 0;
    struct snap_card *card = NULL;
    struct snap_action *action = NULL;
    char device[128];
    struct snap_job cjob;
    struct action_job mjob;
    unsigned long timeout = 360;
    struct timeval etime_card_allocation, stime_card_allocation,
		   etime_attach_action, stime_attach_action,
		   etime_memory_allocation, stime_memory_allocation,
		   etime_prepare_action, stime_prepare_action,
		   etime_action_execution, stime_action_execution;


    snap_action_flag_t action_irq = 0; //(SNAP_ACTION_DONE_IRQ | SNAP_ATTACH_IRQ); //no irq for now; snap_action_flag_t is an enum defined in snaplib


    // Transposition of input matrix B
    #if defined(DOUBLE)
    	double *A     = (double*)a;
    	double *B     = (double*)b;
    	double *BETA  = (double*)beta;
    	double *ALPHA = (double*)alpha;
	double *B_T   = (double *)(alloc_mem(64, sizeof(double)*(k*n)));
    	cblas_domatcopy( CblasRowMajor, CblasTrans, k, n, *ALPHA, B, n, B_T, k);
    #else
    	float *A = (float*)a;
    	float *B = (float*)b;
    	float *BETA=(float*)beta;
    	float *ALPHA=(float*)alpha;
	float *B_T = (float *)(alloc_mem(64, sizeof(float)*(k*n)));
    	cblas_somatcopy( CblasRowMajor, CblasTrans, k, n, *ALPHA, B, n, B_T, k);
    #endif


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
    int reg=0;
    //snap_action_read32 (card, ACTION_RELEASE_REG, &reg);
    VERBOSE3(stdout, "test version SA from register polling %d\n", reg);
    // TODO(lledoux): pull such numbers at runtime from fpga register polling
    uint8_t systolic_array_rows    = 8;
    uint8_t systolic_array_columns = 7;


    // Allocate memories (in and out)
    uint16_t fpga_bus_size = 128; // in bytes, 128 for opencapi, 64 for capi1 and capi2
    uint64_t entire_horizontal_bands_matrix_A = m / systolic_array_rows;
    uint8_t rows_last_partial_band_matrix_A = m % systolic_array_rows;
    uint64_t entire_vertical_bands_matrix_B = n / systolic_array_columns;
    uint8_t cols_last_partial_band_matrix_B = n % systolic_array_columns;
    VERBOSE3(stdout, "entire horizontal bands matrix A: %d\n", entire_horizontal_bands_matrix_A);
    VERBOSE3(stdout, "entire vertical bands matrix b: %d\n", entire_vertical_bands_matrix_B);
    VERBOSE3(stdout, "rows last band matrix A: %d\n", rows_last_partial_band_matrix_A);
    VERBOSE3(stdout, "columns last band matrix b: %d\n", cols_last_partial_band_matrix_B);

    // TODO(lledoux): add border pad band
    bool horizontal_padding_case      = (rows_last_partial_band_matrix_A>0);
    entire_horizontal_bands_matrix_A += horizontal_padding_case? 1 : 0;
    bool vertical_padding_case        = (cols_last_partial_band_matrix_B>0);
    entire_vertical_bands_matrix_B   += (vertical_padding_case)? 1 : 0;
    VERBOSE3(stdout, "case vertical padding: %d\n", vertical_padding_case);
    VERBOSE3(stdout, "case horizontal padding: %d\n", horizontal_padding_case);

    size_t aggregate_dma_memory_size = (fpga_bus_size*k*entire_horizontal_bands_matrix_A*entire_vertical_bands_matrix_B);
    size_t mem_out_size = systolic_array_rows*fpga_bus_size*entire_horizontal_bands_matrix_A*entire_vertical_bands_matrix_B;  // TODO(lledoux): add pad bands
    VERBOSE3(stdout,"size in: %zu\n", aggregate_dma_memory_size);
    VERBOSE3(stdout,"size out: %zu\n", mem_out_size);


    gettimeofday(&stime_memory_allocation, NULL);
    char *aggregate_dma_memory = (char *)(alloc_mem(8192, sizeof(char)*aggregate_dma_memory_size));
    char *mem_out = (char*)(alloc_mem(8192, sizeof(char)*mem_out_size));


    #if defined(DOUBLE)
        double arith_scratchpad;
        for (uint64_t row_band_i=0 ; row_band_i < entire_horizontal_bands_matrix_A ; ++row_band_i) {
            for (uint64_t row_i=0 ; row_i < systolic_array_rows ; ++row_i) {
                for (uint64_t col_j=0 ; col_j < k ; ++col_j) { // place all A band_i (k-rolling loop). adjacent element will be together in $
            	    if (horizontal_padding_case && row_band_i==(entire_horizontal_bands_matrix_A-1)) {
            	        if (row_i<rows_last_partial_band_matrix_A) {
            	            arith_scratchpad = A[(row_band_i*k*systolic_array_rows) + (k*row_i) + (col_j)]; // z order access per horizontal band in A in row major order and in native type
            	        } else {
            	            arith_scratchpad = 0.0f;
            	        }
            	    } else {
            	    	arith_scratchpad = A[(row_band_i*k*systolic_array_rows) + (k*row_i) + (col_j)]; // z order access per horizontal band in A in row major order and in native type
            	    }
            	    for (uint64_t rewrite_i=0 ; rewrite_i < entire_vertical_bands_matrix_B ; ++rewrite_i) {
            	    	memcpy( aggregate_dma_memory +
            	    		(row_band_i*entire_vertical_bands_matrix_B*fpga_bus_size*k) +
            	    		(rewrite_i*k*fpga_bus_size) +
            	    		(fpga_bus_size*col_j) +
            	    		(row_i*sizeof(double)), // end address calculation
            	    		&arith_scratchpad,
            	    		sizeof(double));
            	    }
                }
            }
        }
        for (uint64_t row_band_i=0 ; row_band_i < entire_vertical_bands_matrix_B ; ++row_band_i) {
            for (uint64_t row_i=0 ; row_i < systolic_array_columns ; ++row_i) {
                for (uint64_t col_j=0 ; col_j < k ; ++col_j) { // place all transposed B band_i (k-rolling loop). adjacent element will be together in $
            	    if (vertical_padding_case && row_band_i==(entire_vertical_bands_matrix_B-1)) {
            	        if (row_i<cols_last_partial_band_matrix_B) {
            	            arith_scratchpad = B_T[(row_band_i*k*systolic_array_columns) + (k*row_i) + (col_j)]; // z order access per vertical band in B (horizontal in transposed B) in row major order and in native type
            	        } else {
            	            arith_scratchpad = 0.0f;
            	        }
            	    } else {
            	        arith_scratchpad = B_T[(row_band_i*k*systolic_array_columns) + (k*row_i) + (col_j)]; // z order access per vertical band in B (horizontal in transposed B) in row major order and in native type
            	    }
            	    for (uint64_t rewrite_i=0 ; rewrite_i < entire_horizontal_bands_matrix_A ; ++rewrite_i) {
            	    	memcpy( aggregate_dma_memory +
            	    		(row_band_i*fpga_bus_size*k) +
            	    		(rewrite_i*entire_vertical_bands_matrix_B*k*fpga_bus_size) +
            	    		(fpga_bus_size*col_j) +
            	    		(systolic_array_rows*sizeof(double)) + // offset
            	    		(row_i*sizeof(double)), // end address calculation
            	    		&arith_scratchpad,
            	    		sizeof(double));
            	    }
                }
            }
        }
    #else
        float arith_scratchpad;
        for (uint64_t row_band_i=0 ; row_band_i < entire_horizontal_bands_matrix_A ; ++row_band_i) {
            for (uint64_t row_i=0 ; row_i < systolic_array_rows ; ++row_i) {
                for (uint64_t col_j=0 ; col_j < k ; ++col_j) { // place all A band_i (k-rolling loop). adjacent element will be together in $
            	    if (horizontal_padding_case && row_band_i==(entire_horizontal_bands_matrix_A-1)) {
            	        if (row_i<rows_last_partial_band_matrix_A) {
            	            arith_scratchpad = A[(row_band_i*k*systolic_array_rows) + (k*row_i) + (col_j)]; // z order access per horizontal band in A in row major order and in native type
            	        } else {
            	            arith_scratchpad = 0.0f;
            	        }
            	    } else {
            	    	arith_scratchpad = A[(row_band_i*k*systolic_array_rows) + (k*row_i) + (col_j)]; // z order access per horizontal band in A in row major order and in native type
            	    }
            	    for (uint64_t rewrite_i=0 ; rewrite_i < entire_vertical_bands_matrix_B ; ++rewrite_i) {
            	    	memcpy( aggregate_dma_memory +
            	    		(row_band_i*entire_vertical_bands_matrix_B*fpga_bus_size*k) +
            	    		(rewrite_i*k*fpga_bus_size) +
            	    		(fpga_bus_size*col_j) +
            	    		(row_i*sizeof(float)), // end address calculation
            	    		&arith_scratchpad,
            	    		sizeof(float));
            	    }
                }
            }
        }
        for (uint64_t row_band_i=0 ; row_band_i < entire_vertical_bands_matrix_B ; ++row_band_i) {
            for (uint64_t row_i=0 ; row_i < systolic_array_columns ; ++row_i) {
                for (uint64_t col_j=0 ; col_j < k ; ++col_j) { // place all transposed B band_i (k-rolling loop). adjacent element will be together in $
            	    if (vertical_padding_case && row_band_i==(entire_vertical_bands_matrix_B-1)) {
            	        if (row_i<cols_last_partial_band_matrix_B) {
            	            arith_scratchpad = B_T[(row_band_i*k*systolic_array_columns) + (k*row_i) + (col_j)]; // z order access per vertical band in B (horizontal in transposed B) in row major order and in native type
            	        } else {
            	            arith_scratchpad = 0.0f;
            	        }
            	    } else {
            	        arith_scratchpad = B_T[(row_band_i*k*systolic_array_columns) + (k*row_i) + (col_j)]; // z order access per vertical band in B (horizontal in transposed B) in row major order and in native type
            	    }
            	    for (uint64_t rewrite_i=0 ; rewrite_i < entire_horizontal_bands_matrix_A ; ++rewrite_i) {
            	    	memcpy( aggregate_dma_memory +
            	    		(row_band_i*fpga_bus_size*k) +
            	    		(rewrite_i*entire_vertical_bands_matrix_B*k*fpga_bus_size) +
            	    		(fpga_bus_size*col_j) +
            	    		(systolic_array_rows*sizeof(float)) + // offset
            	    		(row_i*sizeof(float)), // end address calculation
            	    		&arith_scratchpad,
            	    		sizeof(float));
            	    }
                }
            }
        }
    #endif

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
    gettimeofday(&etime_memory_allocation, NULL);
    if (verbose_level > 3 ) {
        __hexdump(stdout, aggregate_dma_memory, aggregate_dma_memory_size);
    }


    // Prepare action for DMA
    uint8_t  type_in  = SNAP_ADDRTYPE_HOST_DRAM;
    uint8_t  type_out = SNAP_ADDRTYPE_HOST_DRAM;
    uint32_t read_burst_num  = 64; // fpga has logic only for 7 arlen
    uint32_t write_burst_num = 64; // fpga has logic only for 7 awlen
    uint32_t transfer_type = 4; // host to host
    gettimeofday(&stime_prepare_action, NULL);
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
    gettimeofday(&etime_prepare_action, NULL);


    // Execute Action
    if (verbose_level > 3 ) {
        __hexdump(stdout, aggregate_dma_memory, aggregate_dma_memory_size);
    }
    gettimeofday(&stime_action_execution, NULL);
    rc = snap_action_sync_execute_job(action, &cjob, timeout);
    gettimeofday(&etime_action_execution, NULL);
    if (rc != 0) {
        VERBOSE0(stderr, "err: job execution %d: %s!\n", rc, strerror(errno));
        goto out_error2;
    }
    if (cjob.retc == SNAP_RETC_SUCCESS) {
        VERBOSE3(stdout, "SUCCESS\n");
    }
    else {
        VERBOSE3(stdout, "FAILED\n");
        VERBOSE0(stderr, "err: Unexpected RETC=%x!\n", cjob.retc);
        goto out_error2;
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
    #if defined(DOUBLE)
        double *C = (double*) c;
        for (uint32_t vertical_band_j=0 ; vertical_band_j < entire_horizontal_bands_matrix_A ; ++vertical_band_j) {
            for (uint32_t horizontal_block_i=0 ; horizontal_block_i < entire_vertical_bands_matrix_B ; ++horizontal_block_i) {
                for (uint32_t row_i=0 ; row_i < systolic_array_rows ; ++row_i) {  // row_i is reversed as the array exits from the bottom
                    for (uint32_t col_j=0 ; col_j < systolic_array_columns ; ++col_j) {
			char * c_tmp = mem_out +
                                (vertical_band_j * entire_vertical_bands_matrix_B * fpga_bus_size * systolic_array_rows) +
                                (horizontal_block_i * fpga_bus_size * systolic_array_rows) +
                                (fpga_bus_size * row_i) +
                                (sizeof(double)*col_j);
			if ( !(horizontal_padding_case &&
			       vertical_band_j==entire_horizontal_bands_matrix_A-1 &&
			       row_i <= systolic_array_rows-1-rows_last_partial_band_matrix_A) &&
			     !(vertical_padding_case   &&
			       horizontal_block_i==entire_vertical_bands_matrix_B-1 &&
			       col_j >= cols_last_partial_band_matrix_B)
			   ) {
                            memcpy(&arith_scratchpad, c_tmp, sizeof(double));
			    C[(vertical_band_j*n*systolic_array_rows)+
			      (horizontal_block_i*systolic_array_columns)+
			      ((systolic_array_rows-1-row_i)*n)+
			      col_j
			    ] = arith_scratchpad;
			}
                    }
                }
	    }
	}
        if (verbose_level > 3 ) {
            __hexdump(stdout, C, sizeof(double)*n*m);
        }
    #else
        float *C = (float*) c;
        for (uint32_t vertical_band_j=0 ; vertical_band_j < entire_horizontal_bands_matrix_A ; ++vertical_band_j) {
            for (uint32_t horizontal_block_i=0 ; horizontal_block_i < entire_vertical_bands_matrix_B ; ++horizontal_block_i) {
                for (uint32_t row_i=0 ; row_i < systolic_array_rows ; ++row_i) {  // row_i is reversed as the array exits from the bottom
                    for (uint32_t col_j=0 ; col_j < systolic_array_columns ; ++col_j) {
			char * c_tmp = mem_out +
                                (vertical_band_j * entire_vertical_bands_matrix_B * fpga_bus_size * systolic_array_rows) +
                                (horizontal_block_i * fpga_bus_size * systolic_array_rows) +
                                (fpga_bus_size * row_i) +
                                (sizeof(float)*col_j);
			if ( !(horizontal_padding_case &&
			       vertical_band_j==entire_horizontal_bands_matrix_A-1 &&
			       row_i <= systolic_array_rows-1-rows_last_partial_band_matrix_A) &&
			     !(vertical_padding_case   &&
			       horizontal_block_i==entire_vertical_bands_matrix_B-1 &&
			       col_j >= cols_last_partial_band_matrix_B)
			   ) {
                            memcpy(&arith_scratchpad, c_tmp, sizeof(float));
			    C[(vertical_band_j*n*systolic_array_rows)+
			      (horizontal_block_i*systolic_array_columns)+
			      ((systolic_array_rows-1-row_i)*n)+
			      col_j
			    ] = arith_scratchpad;
			}
                    }
                }
	    }
	}
        if (verbose_level > 3 ) {
            __hexdump(stdout, C, sizeof(float)*n*m);
        }
    #endif



    // Detach action
    snap_detach_action(action);


    // Deallocate the card
    snap_card_free(card);


    // Deallocate input and output matrices
    free(B_T);
    free(mem_out);
    free(aggregate_dma_memory);
    //exit(EXIT_SUCCESS);
    return 0;

    out_error2:
        snap_detach_action(action);
	free(aggregate_dma_memory);
        free(mem_out);
    out_error1:
        snap_card_free(card);
    out_error:
        free(B_T);
        // exit(EXIT_FAILURE);
	return 1;

}

#ifdef __cplusplus
}
#endif

#endif	// __GEMM_H__
