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

#include <osnap_tools.h>
#include <libosnap.h>
#include "gemm.h"
//#include <snap_s_regs.h>
#include <osnap_hls_if.h>


int main (int argc, char* argv[])
{

    size_t Data_Size_in = 0;
    size_t Data_Size_out = 0;
    char * mem_in = NULL;
    char * mem_out = NULL;
    char path_in[1000];
    char path_out[1000];
    unsigned int N=0;
    unsigned int M=0;
    unsigned int P=0;
    unsigned int B=0;
   // unsigned int posit_width=0;

    uint8_t  type_in = SNAP_ADDRTYPE_HOST_DRAM;
    uint64_t addr_in = 0x0ull;
    uint8_t  type_out = SNAP_ADDRTYPE_HOST_DRAM;
    uint64_t addr_out = 0x0ull;
    uint32_t read_burst_num = 64; // fpga has logic only for 64 arlen/awlen
    uint32_t write_burst_num = 64;
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

    int cmd;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            { "verbose", no_argument, NULL, 'v' },
            { "N", required_argument, NULL, 'N' },
            { "M", required_argument, NULL, 'M' },
            { "P", required_argument, NULL, 'P' },
            { "B", required_argument, NULL, 'B' },
     //       { "posit_width", required_argument, NULL, 'W' },
            { "path_in", required_argument, NULL, 'I' },
            { "path_out", required_argument, NULL, 'O' },
        };
        cmd = getopt_long (argc, argv, "N:M:P:B:I:O:v", long_options, &option_index);

        if (cmd == -1) { /* all params processed ? */
            break;
        }

        switch (cmd) {

        case 'v':   /* show src_addr_data */
            verbose_level++;
            break;

        case 'I':
            strcpy(path_in, optarg);
            break;

       case 'O':
           strcpy(path_out, optarg);
           break;

       case 'N':
           N = atoi(optarg);
           break;

       case 'M':
           M = atoi(optarg);
           break;

       case 'P':
           P = atoi(optarg);
           break;

       case 'B':
           B = atoi(optarg);
           break;

        default:
            break;
        }

    }

    // Allocation of memories (in and out)
    gettimeofday(&stime0, NULL);
    // here we consider that input file has 2 square matrices
    // so, the output file is half size and contains 1 result matrix
    Data_Size_in  = mem_init_from_file(path_in, &mem_in, 4096);
    //double inverse_ratio_out_occupancy = 512.0f/((double)M*(double)posit_width);
    Data_Size_out = N*64*B;  // if OpenCAPI replace 64 by 128
    //Data_Size_out = Data_Size_in;
    //Data_Size_out = ((N*M*posit_width)*inverse_ratio_out_occupancy)/8;
    //VERBOSE2(stdout, "ratio occupancy: %f\n", inverse_ratio_out_occupancy);
    //VERBOSE2(stdout, "size out for mem alloc is: %zu\n", Data_Size_out);
    // Data_Size_in = 512; //(N*P*posit_width) + (P*M*posit_width);
    // Data_Size_out = 512; //N*M*posit_width;
    // mem_in = alloc_mem(4096, Data_Size_in);
    mem_out = alloc_mem(4096, Data_Size_out);

    // char mem_in_non_aligned[]  = {
    //     0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // WORD 1
    //     0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,
    //     0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,  // WORD 2
    //     0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,  // WORD 3
    //     0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,  // WORD 4
    //     0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,  // WORD 5
    //     0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,  // WORD 6
    //     0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,  // WORD 7
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,  // WORD 8
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    //     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    // } ;

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

    uint64_t total_arithmetic_ops = (uint64_t)((M*N)+(N*M))*B*P;
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
    // free(mem_in);
    free(mem_out);
    exit(EXIT_SUCCESS);

    out_error2:
        snap_detach_action(action);
    out_error1:
        snap_card_free(card);
    out_error:
        // free(mem_in);
        free(mem_out);
        exit(EXIT_FAILURE);

} // main end
