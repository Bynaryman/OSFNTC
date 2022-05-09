#define min(x,y) (((x) < (y)) ? (x) : (y))

#include <stdio.h>
#include <stdlib.h>
#include "cblas.h"
#include "f77blas.h"
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>


#define CBLAS 1

double get_time() {
  struct timeval t;
  struct timezone tzp;
  gettimeofday(&t, &tzp);
  return t.tv_sec + t.tv_usec*1e-6;
}

int main()
{
  double *A, *B, *C;
  int m, n, k, i, j;
  double alpha, beta;
  double start, end;

  // m = 20000, k = 2000, n = 1000;
  m = 784, k = 25, n = 6;

  alpha = 1.0; beta = 0.0;

  posix_memalign((void**) &A, 64, m*k*sizeof( double ));
  posix_memalign((void**) &B, 64, k*n*sizeof( double ));
  posix_memalign((void**) &C, 64, m*n*sizeof( double ));

  if (A == NULL || B == NULL || C == NULL) {
    printf( "\n ERROR: Can't allocate memory for matrices. Aborting... \n\n");
    free(A);
    free(B);
    free(C);
    return 1;
  }

  for (i = 0; i < (m*k); i++) {
    A[i] = (double)(i+1);
  }

  for (i = 0; i < (k*n); i++) {
    B[i] = (double)(-i-1);
  }

  for (i = 0; i < (m*n); i++) {
    C[i] = 0.0;
  }
  // double *B_T;
  // posix_memalign((void**) &B_T, 64, k*n*sizeof( double ));
  // cblas_domatcopy( CblasRowMajor, CblasTrans, k, n, 1.0f, B, n, B_T, k);
  // printf("transpose B[2]=%lf\n", B[2]);
  // printf("transpose B_T[484]=%lf\n", B_T[484]);
  // printf("transpose B_T[3]=%lf\n", B_T[3]);
  // printf("transpose B_T[4]=%lf\n", B_T[4]);
  // free(B_T);

  start = get_time();
  //cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
  //      m, n, k, alpha, A, k, B, n, beta, C, n);
  cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
        m, n, k, alpha, A, k, B, n, beta, C, n);
  end = get_time();

  printf("%f s\n",  (end - start));


  free(A);
  free(B);
  free(C);

return 0;
}

