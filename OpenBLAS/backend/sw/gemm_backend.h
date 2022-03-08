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
#include "cblas.h"

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
	if (posix_memalign((void **)&a, 4096, size2) != 0) {
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

    char *pTmp = NULL;
    if (( pTmp = getenv( "VERBOSITY" )) != NULL )
        verbose_level = atoi(pTmp);
    size_t Data_Size_in = 0;
    size_t Data_Size_out = 0;
    char * mem_in = NULL;
    char * mem_out = NULL;
    char path_in[1000];
    char path_out[1000];


    #if defined(DOUBLE)
    	double *A     = (double*)a;
    	double *B     = (double*)b;
    	double *BETA  = (double*)beta;
    	double *ALPHA = (double*)alpha;
	double *A_T   = (double *)(alloc_mem(64, sizeof(double)*(m*k)));
	double *B_T   = (double *)(alloc_mem(64, sizeof(double)*(k*n)));
    	cblas_domatcopy( CblasRowMajor, CblasTrans, m, k, *ALPHA, A, k, A_T, m);
    	// cblas_domatcopy( CblasRowMajor, CblasTrans, k, n, *ALPHA, B, n, B_T, k); if transpose A is good to do
    #else
    	float *A = (float*)a;
    	float *B = (float*)b;
    	float *BETA=(float*)beta;
    	float *ALPHA=(float*)alpha;
	float *B_T = (float *)(alloc_mem(64, sizeof(float)*(k*n)));
    	cblas_somatcopy( CblasRowMajor, CblasTrans, k, n, *ALPHA, B, n, B_T, k);
    #endif

    // TODO(lledoux): pull such numbers at runtime from fpga register polling
    const uint8_t systolic_array_rows    = 8;
    const uint8_t systolic_array_columns = 7;

    const uint64_t entire_horizontal_bands_matrix_A = m / systolic_array_rows;
    const uint8_t rows_last_partial_band_matrix_A = m % systolic_array_rows;
    const uint64_t entire_vertical_bands_matrix_B = n / systolic_array_columns;
    const uint8_t cols_last_partial_band_matrix_B = n % systolic_array_columns;

    uint16_t fpga_bus_size = 128; // in bytes, 128 for opencapi, 64 for capi1 and capi2
    // TODO(lledoux): add border pad band
    char *aggregate_dma_memory = (char *)(alloc_mem(4096, sizeof(char)*(fpga_bus_size*k*entire_horizontal_bands_matrix_A*entire_vertical_bands_matrix_B)));

    for (uint64_t row_band_i=0 ; row_band_i < entire_horizontal_bands_matrix_A ; ++row_band_i) {
	for (uint64_t row_i=0 ; row_i < systolic_array_rows ; ++row_i) {
            for (uint64_t col_j=0 ; col_j < k ; ++col_j) { // place all A band_i (k-rolling loop). adjacent element will be together in $
		double tmp = A[(row_band_i*k*systolic_array_rows) + (k*row_i) + (col_j)]; // z order access per horizontal band in A in row major order and in native type
		for (uint64_t rewrite_i=0 ; rewrite_i < entire_vertical_bands_matrix_B ; ++rewrite_i) {
			memcpy( aggregate_dma_memory +
				(row_band_i*entire_vertical_bands_matrix_B*fpga_bus_size*k) +
				(rewrite_i*k*fpga_bus_size) +
				(fpga_bus_size*col_j) +
				(row_i*sizeof(double)), // end address calcultation
				&tmp,
				sizeof(double));
		}
	    }
	}
    }

    for (uint64_t row_band_i=0 ; row_band_i < entire_vertical_bands_matrix_B ; ++row_band_i) {
	for (uint64_t row_i=0 ; row_i < systolic_array_cols ; ++row_i) {
            for (uint64_t col_j=0 ; col_j < k ; ++col_j) { // place all transposed B band_i (k-rolling loop). adjacent element will be together in $
		double tmp = B_T[(row_band_i*k*systolic_array_cols) + (k*row_i) + (col_j)]; // z order access per vertical band in B (horizontal in transposed B) in row major order and in native type
		for (uint64_t rewrite_i=0 ; rewrite_i < entire_vertical_bands_matrix_B ; ++rewrite_i) {
			memcpy( aggregate_dma_memory +
				(row_band_i*entire_vertical_bands_matrix_B*fpga_bus_size*k) +
				(rewrite_i*k*fpga_bus_size) +
				(fpga_bus_size*col_j) +
				(row_i*sizeof(double)), // end address calcultation
				&tmp,
				sizeof(double));
		}
	    }
	}
    }

    for (uint64_t col_band_j=0 ; col_band_j < entire_vertical_bands_matrix_B ; ++col_band_j) {
	for (uint64_t col_j=0 ; col_j < systolic_array_cols ; ++col_j) {
            for (uint64_t row_i=0 ; row_i < k ; ++row_i) { // place all B band_j (k-rolling loop). adjacent element will be together in $
		double tmp = B_T[(col_band_j*k*systolic_array_cols) + (systolic_array_cols*col_j) + (row_i)]; // z order access per vertical band in B (horizontal in transposed B) in row major order and in native type
		for (uint64_t rewrite_i=0 ; rewrite_i < entire_vertical_bands_matrix_B ; ++rewrite_i) {
			memcpy( aggregate_dma_memory +
				(row_band_i*entire_vertical_bands_matrix_B*fpga_bus_size*k) +
				(rewrite_i*k*fpga_bus_size) +
				(fpga_bus_size*col_j) +
				(row_i*sizeof(double)), // end address calcultation
				&tmp,
				sizeof(double));
			// for cols, i should add N*sizeof(double) offset, and maybe col_j is different index
		}
	    }
	}
    }

    if (verbose_level > 3 ) {
        __hexdump(stdout, aggregate_dma_memory, (fpga_bus_size*k*entire_horizontal_bands_matrix_A*entire_vertical_bands_matrix_B));
    }

    for (uint8_t pad_row_i=0 ; pad_row_i<rows_last_partial_band_matrix_A ; ++pad_row_i) {

    }

    for (uint64_t col_band_j=0 ; col_band_j<entire_vertical_bands_matrix_B ; ++col_band_j) { // then, place all vertical bands of B. one fetch is sufficient for many writes

    }

    uint8_t  type_in = SNAP_ADDRTYPE_HOST_DRAM;
    uint64_t addr_in = 0x0ull;
    uint8_t  type_out = SNAP_ADDRTYPE_HOST_DRAM;
    uint64_t addr_out = 0x0ull;
    uint32_t read_burst_num = 128; // fpga has logic only for 64 arlen/awlen
    uint32_t write_burst_num = 128;
    uint32_t transfer_type = 4; // host to host

    int rc = 0;
    int card_no = 0;
    struct snap_card *card = NULL;
    struct snap_action *action = NULL;
    char device[128];
    struct snap_job cjob;
    struct action_job mjob;
    unsigned long timeout = 360;
    struct timeval etime, stime, etime1, stime1, etime2, stime2, etime3, stime3, etime0, stime0;

    unsigned int N = 32;
    unsigned int M = 31;
    unsigned int P = 32;
    unsigned int BLOCKS = 1;
    //strcpy(path_in, "/home/lledoux/Documents/high_end/oc-accel/actions/cgemm/sw/in_float_batched.raw");
    strcpy(path_in, "/home/binaryman/Documents/PhD/fpga/P9_OPC/snap/actions/SA_s3/sw/in_float_batched.raw");
    strcpy(path_out, "/tmp/tmp.txt");

    // Allocation of memories (in and out)
    gettimeofday(&stime0, NULL);
    // here we consider that input file has 2 square matrices
    // so, the output file is half size and contains 1 result matrix
    Data_Size_in  = mem_init_from_file(path_in, &mem_in, 4096);
    if (is_aligned(mem_in, 4096)) {
	printf("pointer from file alloc is 4096 aligned\n");
    } else {
	printf("pointer from file alloc is NOT 4096 aligned\n");
    }
    Data_Size_out = N*128*BLOCKS;  // if OpenCAPI replace 64 by 128
    // mem_in = alloc_mem(4096, Data_Size_in);
    mem_out = alloc_mem(4096, Data_Size_out);

    // memcpy(mem_in, mem_in_non_aligned, 512);
    gettimeofday(&etime0, NULL);
    if ( verbose_level > 2 ) {
        __hexdump(stdout, mem_in, Data_Size_in);
    }

    snap_action_flag_t action_irq = 0; //(SNAP_ACTION_DONE_IRQ | SNAP_ATTACH_IRQ); //no irq for now; snap_action_flag_t is an enum defined in snaplib

    // Offloading Action
    // Card Allocation
    gettimeofday(&stime1, NULL);
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
    gettimeofday(&etime1, NULL);

    VERBOSE3(stdout, "Card Allocated Successfully\n");

    // Attaching Action
    gettimeofday(&stime2, NULL);
    action = snap_attach_action(card, ACTION_TYPE, action_irq, 180);
    if (action == NULL) {
        VERBOSE0(stderr, "err: failed to attach action %u: %s\n", card_no, strerror(errno));
        goto out_error1;
    }
    gettimeofday(&etime2, NULL);
    VERBOSE3(stdout, "Action Attached Successfully\n");

    //-- Puting Data Addr and Size in cjob structure --
    gettimeofday(&stime3, NULL);
    addr_in  = (unsigned long)mem_in;
    addr_out = (unsigned long)mem_out;
    snap_prepare_action(&cjob,
                        &mjob,
                        (void *)addr_in,
                        Data_Size_in,
                        type_in,
                        (void *)addr_out,
                        Data_Size_out,
                        type_out,
                        read_burst_num,
                        write_burst_num,
                        transfer_type
    );
    gettimeofday(&etime3, NULL);

    // TimeStamp1 of action execution
    gettimeofday(&stime, NULL);

    //long a = get_nanos();
    // Set MMIO, Start Action, Wait for Idle
    rc = snap_action_sync_execute_job(action, &cjob, timeout);
    //long b = get_nanos();

    // TimeStamp2 of action execution
    gettimeofday(&etime, NULL);

    if (rc != 0) {
        VERBOSE0(stderr, "err: job execution %d: %s!\n", rc, strerror(errno));
        goto out_error2;
    }

    // test return code
    if (cjob.retc == SNAP_RETC_SUCCESS)
        VERBOSE3(stdout, "SUCCESS\n");
    else
    {
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

    uint64_t total_arithmetic_ops = (uint64_t)((M*N)+(N*M))*BLOCKS*P;
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
    VERBOSE2( stdout, "%zu %lld %f %f\n",
              Data_Size_in,
              (long long)timediff_usec(&etime,  &stime),
              (((float)(Data_Size_in+Data_Size_out)/((long long)timediff_usec(&etime,  &stime)))),
              ((double)(total_arithmetic_ops)/((long long)timediff_usec(&etime,  &stime)))
            );

    if (verbose_level > 2 ) {
        __hexdump(stdout, mem_out, Data_Size_out);
    }

    // Writing Matrix C to out
    save_file_from_memory(path_out, &mem_out, Data_Size_out);

    // Detach action
    snap_detach_action(action);

    // deallocate the card
    snap_card_free(card);
    // deallocate matrices in and out
    free(B_T);
    free(mem_out);
    free(aggregate_dma_memory);
    exit(EXIT_SUCCESS);

    out_error2:
        snap_detach_action(action);
    out_error1:
        snap_card_free(card);
    out_error:
        free(B_T);
        free(mem_out);
	free(aggregate_dma_memory);
        exit(EXIT_FAILURE);

}

#ifdef __cplusplus
}
#endif

#endif	// __GEMM_H__
