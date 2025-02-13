/* ************************************************************************
 * Copyright 2013 Advanced Micro Devices, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * ************************************************************************/


#include <sys/types.h>
#include <stdio.h>
#include <string.h>

/* Include CLBLAS header. It automatically includes needed OpenCL header,
 * so we can drop out explicit inclusion of cl.h header.
 */
#include <clBLAS.h>

/* This example uses predefined matrices and their characteristics for
 * simplicity purpose.
 */
static const clblasOrder order = clblasRowMajor;
static const clblasSide side = clblasLeft;

static const size_t M = 4;
static const size_t N = 5;

static const FloatComplex alpha = { 10, 0 };

static const clblasTranspose transA = clblasNoTrans;
static const clblasUplo uploA = clblasUpper;
static const clblasDiag diagA = clblasNonUnit;
static const FloatComplex A[] = {
    { 11, 0 },{ 12, 0 },{ 13, 0 },{ 14, 0 },
    { 0, 0 },{ 22, 0 },{ 23, 0 },{ 24, 0 },
    { 0, 0 },{ 0, 0 },{ 33, 0 },{ 34, 0 },
    { 0, 0 },{ 0, 0 },{ 0, 0 },{ 44, 0 }
};
static const size_t lda = 4;        /* i.e. lda = M */

static FloatComplex B[] = {
    { 11, 0 },{ 12, 0 },{ 13, 0 },{ 14, 0 },{ 15, 0 },
    { 21, 0 },{ 22, 0 },{ 23, 0 },{ 24, 0 },{ 25, 0 },
    { 31, 0 },{ 32, 0 },{ 33, 0 },{ 34, 0 },{ 35, 0 },
    { 41, 0 },{ 42, 0 },{ 43, 0 },{ 44, 0 },{ 45, 0 },
};
static const size_t ldb = 5;        /* i.e. ldb = N */


static FloatComplex result[20];         /* ldb*M */

static const size_t off  = 1;
static const size_t offA = 4 + 1;   /* M + off */
static const size_t offB = 5 + 1;   /* N + off */

static void
printResult(const char* str)
{
    size_t i, j, nrows;

    printf("%s:\n", str);

    nrows = (sizeof(result) / sizeof(FloatComplex)) / ldb;
    for (i = 0; i < nrows; i++) {
        for (j = 0; j < ldb; j++) {
            // printf("%.5f ", result[i * ldb + j].x);
            printf("%.5f ", result[i * ldb + j].s[0]);
        }
        printf("\n");
    }
}

int
main(void)
{
    cl_int err;
    cl_platform_id platform[] = { 0, 0 };
    cl_device_id device = 0;
    cl_context_properties props[3] = { CL_CONTEXT_PLATFORM, 0, 0 };
    cl_context ctx = 0;
    cl_command_queue queue = 0;
    cl_mem bufA, bufB;
    cl_event event = NULL;
    int ret = 0;

    /* Setup OpenCL environment. */
    err = clGetPlatformIDs(sizeof( platform ), &platform, NULL);
    if (err != CL_SUCCESS) {
        printf( "clGetPlatformIDs() failed with %d\n", err );
        return 1;
    }

    err = clGetDeviceIDs(platform[0], CL_DEVICE_TYPE_CPU, 1, &device, NULL);
    if (err != CL_SUCCESS) {
        printf( "clGetDeviceIDs() failed with %d\n", err );
        return 1;
    }

    props[1] = (cl_context_properties)platform;
    ctx = clCreateContext(props, 1, &device, NULL, NULL, &err);
    if (err != CL_SUCCESS) {
        printf( "clCreateContext() failed with %d\n", err );
        return 1;
    }

    queue = clCreateCommandQueue(ctx, device, 0, &err);
    if (err != CL_SUCCESS) {
        printf( "clCreateCommandQueue() failed with %d\n", err );
        clReleaseContext(ctx);
        return 1;
    }

    /* Setup clblas. */
    err = clblasSetup();
    if (err != CL_SUCCESS) {
        printf("clblasSetup() failed with %d\n", err);
        clReleaseCommandQueue(queue);
        clReleaseContext(ctx);
        return 1;
    }

    /* Prepare OpenCL memory objects and place matrices inside them. */
    bufA = clCreateBuffer(ctx, CL_MEM_READ_ONLY, M * M * sizeof(*A),
                          NULL, &err);
    bufB = clCreateBuffer(ctx, CL_MEM_READ_WRITE, M * N * sizeof(*B),
                          NULL, &err);

    err = clEnqueueWriteBuffer(queue, bufA, CL_TRUE, 0,
        M * M * sizeof(*A), A, 0, NULL, NULL);
    err = clEnqueueWriteBuffer(queue, bufB, CL_TRUE, 0,
        M * N * sizeof(*B), B, 0, NULL, NULL);

    /* Call clblas function. Perform TRSM for the lower right sub-matrices */
    err = clblasCtrsm(order, side, uploA, transA, diagA, M - off, N - off,
                         alpha, bufA, offA, lda, bufB, offB, ldb, 1, &queue, 0,
                         NULL, &event);
    if (err != CL_SUCCESS) {
        printf("clblasStrsmEx() failed with %d\n", err);
        ret = 1;
    }
    else {
        /* Wait for calculations to be finished. */
        err = clWaitForEvents(1, &event);

        /* Fetch results of calculations from GPU memory. */
        err = clEnqueueReadBuffer(queue, bufB, CL_TRUE, 0,
                                  M * N * sizeof(*result),
                                  result, 0, NULL, NULL);

        /* At this point you will get the result of STRSM placed in 'result' array. */
        puts("");
        printResult("clblasCtrsmEx result");
    }

    /* Release OpenCL events. */
    clReleaseEvent(event);

    /* Release OpenCL memory objects. */
    clReleaseMemObject(bufB);
    clReleaseMemObject(bufA);

    /* Finalize work with clblas. */
    clblasTeardown();

    /* Release OpenCL working objects. */
    clReleaseCommandQueue(queue);
    clReleaseContext(ctx);

    return ret;
}
