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


/*
 * Hpmv performance test cases
 */

#include <stdlib.h>             // srand()
#include <string.h>             // memcpy()
#include <gtest/gtest.h>
#include <clBLAS.h>

#include <common.h>
#include <clBLAS-wrapper.h>
#include <BlasBase.h>
#include <hpmv.h>
#include <blas-random.h>

#ifdef PERF_TEST_WITH_ACML
#include <blas-internal.h>
#include <blas-wrapper.h>
#endif

#include "PerformanceTest.h"

/*
 * NOTE: operation factor means overall number
 *       of multiply and add per each operation involving
 *       2 matrix elements
 */

using namespace std;
using namespace clMath;

#define CHECK_RESULT(ret)                                                   \
do {                                                                        \
    ASSERT_GE(ret, 0) << "Fatal error: can not allocate resources or "      \
                         "perform an OpenCL request!" << endl;              \
    EXPECT_EQ(0, ret) << "The OpenCL version is slower in the case" <<      \
                         endl;                                              \
} while (0)

namespace clMath {

template <typename ElemType> class HpmvPerformanceTest : public PerformanceTest
{
public:
    virtual ~HpmvPerformanceTest();

    virtual int prepare(void);
    virtual nano_time_t etalonPerfSingle(void);
    virtual nano_time_t clblasPerfSingle(void);

    static void runInstance(BlasFunction fn, TestParams *params)
    {
        HpmvPerformanceTest<ElemType> perfCase(fn, params);
        int ret = 0;
        int opFactor;
        BlasBase *base;

        base = clMath::BlasBase::getInstance();

		opFactor = 1; //FIX-ME

        if ((fn == FN_ZHPMV) &&
            !base->isDevSupportDoublePrecision()) {

            std::cerr << ">> WARNING: The target device doesn't support native "
                         "double precision floating point arithmetic" <<
                         std::endl << ">> Test skipped" << std::endl;
            return;
        }

        if (!perfCase.areResourcesSufficient(params)) {
            std::cerr << ">> RESOURCE CHECK: Skip due to unsufficient resources" <<
                        std::endl;
			return;
        }
        else {
            ret = perfCase.run(opFactor);
        }

        ASSERT_GE(ret, 0) << "Fatal error: can not allocate resources or "
                             "perform an OpenCL request!" << endl;
        EXPECT_EQ(0, ret) << "The OpenCL version is slower in the case" << endl;
    }

private:
    HpmvPerformanceTest(BlasFunction fn, TestParams *params);

    bool areResourcesSufficient(TestParams *params);

    TestParams params_;
    ElemType *AP_;
	ElemType *X_;
	ElemType *Y_;
    ElemType *backY_;
    cl_mem mobjAP_;
    cl_mem mobjX_;
	cl_mem mobjY_;
	ElemType alpha, beta;
    ::clMath::BlasBase *base_;
};

template <typename ElemType>
HpmvPerformanceTest<ElemType>::HpmvPerformanceTest(
    BlasFunction fn,
    TestParams *params) : PerformanceTest( fn, (problem_size_t)( ( ((2 * (( params->N * (params->N)) + params->N)) ) * sizeof(ElemType) ) ) ),
    params_(*params), mobjAP_(NULL), mobjX_(NULL)
{

    AP_ = new ElemType[((params_.N * (params_.N + 1)) / 2 ) + params_.offA];
    X_ = new ElemType[ 1 + (params_.N-1) * abs(params_.incx)  + params_.offBX];
	Y_ = new ElemType[ 1 + (params_.N-1) * abs(params_.incy)  + params_.offCY];
    backY_ = new ElemType[ 1 + (params_.N-1) * abs(params_.incy)  + params_.offCY];
	alpha = convertMultiplier<ElemType>(params_.alpha);
	beta  = convertMultiplier<ElemType>(params_.beta);

    base_ = ::clMath::BlasBase::getInstance();

	mobjAP_ = NULL;
	mobjX_ = NULL;
	mobjY_ = NULL;
}

template <typename ElemType>
HpmvPerformanceTest<ElemType>::~HpmvPerformanceTest()
{
	if(AP_ != NULL)
    {
        delete[] AP_;
    }
	if(X_ != NULL)
	{
        delete[] X_;
	}
	if(backY_ != NULL)
	{
		delete[] backY_;
	}
	if(Y_ != NULL)
	{
	    delete[] Y_;
	}

    if ( mobjAP_ != NULL )
		clReleaseMemObject(mobjAP_);
	if ( mobjX_ != NULL )
	    clReleaseMemObject(mobjX_);
	if ( mobjY_ != NULL )
		clReleaseMemObject(mobjY_);
}

/*
 * Check if available OpenCL resources are sufficient to
 * run the test case
 */
template <typename ElemType> bool
HpmvPerformanceTest<ElemType>::areResourcesSufficient(TestParams *params)
{
    clMath::BlasBase *base;
    size_t gmemSize, allocSize;
    size_t n = params->N;

	if((AP_ == NULL) || (X_ == NULL) || (Y_ == NULL) || (backY_ == NULL))
	{
		return 0;
	}

    base = clMath::BlasBase::getInstance();
    gmemSize = (size_t)base->availGlobalMemSize( 0 );
    allocSize = (size_t)base->maxMemAllocSize();

    bool suff = ( sizeof(ElemType)*((n*(n+1))/2) < allocSize ) && ((1 + (n-1)*abs(params->incx))*sizeof(ElemType) < allocSize); //for individual allocations
	suff = suff && ((( ((n*(n+1))/2) + (1 + (n-1)*abs(params->incx)) + (1 + (n-1)*abs(params->incy)))*sizeof(ElemType)) < gmemSize) ; //for total global allocations

    return suff ;
}

template <typename ElemType> int
HpmvPerformanceTest<ElemType>::prepare(void)
{
    size_t lenX, N, lenY;
	N = params_.N;
    lenX = 1 + (N-1) * abs(params_.incx);
	lenY = 1 + (N-1) * abs(params_.incy);

	randomHemvMatrices(params_.order, params_.uplo, N, true, &alpha, (AP_ + params_.offA), params_.lda,
                        (X_ + params_.offBX), params_.incx, true, &beta, (Y_ + params_.offCY), params_.incy);

	memcpy(backY_, Y_, (lenY+ params_.offCY )* sizeof(ElemType));

    mobjAP_ = base_->createEnqueueBuffer(AP_, (((params_.N * (params_.N + 1)) / 2 ) + params_.offA)* sizeof(*AP_), 0, CL_MEM_READ_ONLY);
    mobjX_ = base_->createEnqueueBuffer(X_, (lenX + params_.offBX )* sizeof(*X_), 0, CL_MEM_READ_ONLY);
	mobjY_ = base_->createEnqueueBuffer(Y_, (lenY + params_.offCY )* sizeof(*Y_), 0, CL_MEM_READ_WRITE);

    return ( (mobjAP_ != NULL) &&  (mobjX_ != NULL) && (mobjY_ != NULL) ) ? 0 : -1;
}

template <typename ElemType> nano_time_t
HpmvPerformanceTest<ElemType>::etalonPerfSingle(void)
{
    nano_time_t time = 0;
    clblasOrder order;
	clblasUplo fUplo;

#ifndef PERF_TEST_WITH_ROW_MAJOR
    if (params_.order == clblasRowMajor) {
        cerr << "Row major order is not allowed" << endl;
        return NANOTIME_ERR;
    }
#endif
    order = params_.order;
	fUplo = params_.uplo;

#ifdef PERF_TEST_WITH_ACML

	if (order != clblasColumnMajor)
    {
        order = clblasColumnMajor;
		fUplo =  (params_.uplo == clblasUpper)? clblasLower : clblasUpper;
		doConjugate( (AP_ + params_.offA), params_.N, params_.N, params_.lda );
        doConjugate( (AP_ + params_.offA), ((params_.N * (params_.N + 1)) / 2 ), 1, 1 );
   	}

   	time = getCurrentTime();
  	clMath::blas::hpmv(order, fUplo, params_.N, alpha, AP_, params_.offA,
							X_, params_.offBX, params_.incx, beta, Y_, params_.offCY, params_.incy);
  	time = getCurrentTime() - time;

#endif  // PERF_TEST_WITH_ACML

    return time;
}


template <typename ElemType> nano_time_t
HpmvPerformanceTest<ElemType>::clblasPerfSingle(void)
{
    nano_time_t time;
    cl_event event;
    cl_int status;
    cl_command_queue queue = base_->commandQueues()[0];
	int lenY = 1 + (params_.N-1) * abs(params_.incy);

    status = clEnqueueWriteBuffer(queue, mobjY_, CL_TRUE, 0,
                                  (lenY + params_.offCY )* sizeof(ElemType), backY_, 0, NULL, &event);
    if (status != CL_SUCCESS) {
        cerr << "Vector Y buffer object enqueuing error, status = " <<
                 status << endl;

        return NANOTIME_ERR;
    }

    status = clWaitForEvents(1, &event);
    if (status != CL_SUCCESS) {
        cout << "Wait on event failed, status = " <<
                status << endl;

        return NANOTIME_ERR;
    }

    event = NULL;

	time = getCurrentTime();
#define TIMING
#ifdef TIMING

	int iter = 20;
	for ( int i = 1; i <= iter; i++)
	{
#endif
		status = (cl_int)clMath::clblas::hpmv(params_.order, params_.uplo, params_.N, alpha, mobjAP_, params_.offA,
						mobjX_, params_.offBX, params_.incx, beta, mobjY_, params_.offCY, params_.incy,
						1, &queue, 0, NULL, &event);

    if (status != CL_SUCCESS) {
        cerr << "The CLBLAS HPMV function failed, status = " <<
                status << endl;

        return NANOTIME_ERR;
    }

#ifdef TIMING
	} // iter loop
	clFinish( queue);
    time = getCurrentTime() - time;
	time /= iter;
#else

	status = flushAll(1, &queue);
    if (status != CL_SUCCESS) {
        cerr << "clFlush() failed, status = " << status << endl;
        return NANOTIME_ERR;
    }

    time = getCurrentTime();
    status = waitForSuccessfulFinish(1, &queue, &event);
    if (status == CL_SUCCESS) {
        time = getCurrentTime() - time;
    }
    else {
        cerr << "Waiting for completion of commands to the queue failed, "
                "status = " << status << endl;
        time = NANOTIME_ERR;
    }

	//printf("Time elapsed : %lu\n", time);
#endif

    return time;
}

} // namespace clMath


TEST_P(HPMV, chpmv)
{
    TestParams params;

    getParams(&params);
    HpmvPerformanceTest<FloatComplex>::runInstance(FN_CHPMV, &params);
}

TEST_P(HPMV, zhpmv)
{
    TestParams params;

    getParams(&params);
    HpmvPerformanceTest<DoubleComplex>::runInstance(FN_ZHPMV, &params);
}

