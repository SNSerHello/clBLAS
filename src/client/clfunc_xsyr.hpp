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


// $Id

#ifndef CLBLAS_BENCHMARK_XSYR_HXX__
#define CLBLAS_BENCHMARK_XSYR_HXX__

#include "clfunc_common.hpp"

template <typename T>
struct xSyrBuffer
{
	clblasOrder order;
  clblasUplo uplo;
  size_t N;
  T alpha;
  T* cpuX;
  cl_mem X;
  size_t offx;
  int incx;
  T* cpuA;
  cl_mem A;
  size_t offa;
  size_t lda;
}; // struct buffer

template <typename T>
class xSyr : public clblasFunc
{
public:
  xSyr(StatisticalTimer& timer, cl_device_type devType) : clblasFunc(timer,  devType)
  {
    timer.getUniqueID("clSyr", 0);
  }

  ~xSyr()
  {
    delete buffer.cpuA;
    delete buffer.cpuX;
    OPENCL_V_THROW( clReleaseMemObject(buffer.A), "releasing buffer A");
    OPENCL_V_THROW( clReleaseMemObject(buffer.X), "releasing buffer C");
  }

  double gflops()
  {
    return static_cast<double>((buffer.N * buffer.N)/time_in_ns());
  }

  std::string gflops_formula()
  {
    return "N*N/time";
  }

  void setup_buffer(int order_option, int side_option, int
                    uplo_option, int diag_option, int transA_option, int
                    transB_option, size_t M, size_t N, size_t K,
                    size_t lda, size_t ldb, size_t ldc,size_t offA,
					          size_t offB, size_t offC, double alpha,
                    double beta);
  void initialize_cpu_buffer();
  void initialize_gpu_buffer();
  void reset_gpu_write_buffer();
  void call_func();
  	void read_gpu_buffer()
	{
		//cl_int err;
		//to-do need to fill up
	}
	void roundtrip_func()
	{//to-do need to fill up
	}
	void roundtrip_setup_buffer(int order_option, int side_option, int uplo_option,
                      int diag_option, int transA_option, int  transB_option,
                      size_t M, size_t N, size_t K, size_t lda, size_t ldb,
                      size_t ldc, size_t offA, size_t offBX, size_t offCY,
                      double alpha, double beta)
		{}

protected:
protected:
  void initialize_scalars(double alpha, double beta)
  {
    buffer.alpha = alpha;
  }

private:
  xSyrBuffer<T> buffer;
};

template <typename T>
void xSyr<T>::setup_buffer(int order_option, int side_option, int
                    uplo_option, int diag_option, int transA_option, int
                    transB_option, size_t M, size_t N, size_t K,
                    size_t lda, size_t ldb, size_t ldc,size_t offA,
					          size_t offB, size_t offC, double alpha,
                    double beta)
{
  initialize_scalars(alpha, beta);
  buffer.offa = offA;
  buffer.offx = offB;
  buffer.incx = 1;
  buffer.N = M;
  if (order_option == 0)
  {
  buffer.order = clblasRowMajor;
  }
  else
  {
  buffer.order = clblasColumnMajor;
  }
  if (uplo_option == 0)
  {
      buffer.uplo = clblasUpper;
  }
  else
  {
      buffer.uplo = clblasLower;
  }
  if (lda == 0)
  {
    buffer.lda = buffer.N;
  }
  else if (lda < buffer.N)
  {
    std::cerr << "lda:wrong size\n";
    exit(1);
  }
  else
  {
    buffer.lda = lda;
  }
  buffer.cpuX = new T[buffer.N];
  buffer.cpuA = new T[buffer.N * buffer.lda];
  cl_int err;
  buffer.A = clCreateBuffer(ctx_, CL_MEM_READ_ONLY,
                                buffer.N * buffer.lda*sizeof(T),
                                NULL, &err);

  buffer.X = clCreateBuffer(ctx_, CL_MEM_READ_WRITE,
                                    buffer.N*sizeof(T),
                                    NULL, &err);
}

template <typename T>
void xSyr<T>::initialize_cpu_buffer()
{
  srand(10);
  for (size_t i = 0; i < buffer.N; ++i)
  {
    for (size_t j = 0; j < buffer.lda; ++j)
    {
        buffer.cpuA[i*buffer.lda+j] = random<T>(UPPER_BOUND<T>()) /
                                      randomScale<T>();
    }
  }

  for (size_t i = 0; i < buffer.N; ++i)
  {
    buffer.cpuX[i] = random<T>(UPPER_BOUND<T>()) /
                                      randomScale<T>();
  }
}

template <typename T>
void xSyr<T>::initialize_gpu_buffer()
{
  cl_int err;

  err = clEnqueueWriteBuffer(queue_, buffer.A, CL_TRUE,
                              buffer.offa * sizeof(T),
                              buffer.N * buffer.lda*sizeof(T),
                              buffer.cpuA, 0, NULL, NULL);

  err = clEnqueueWriteBuffer(queue_, buffer.X, CL_TRUE, 0,
                              buffer.N*sizeof(T),
                              buffer.cpuX, 0, NULL, NULL);
}

template <typename T>
void xSyr<T>::reset_gpu_write_buffer()
{
  cl_int err;
  err = clEnqueueWriteBuffer(queue_, buffer.A, CL_TRUE,
                              buffer.offa * sizeof(T),
                              buffer.N * buffer.lda*sizeof(T),
                              buffer.cpuA, 0, NULL, NULL);;
}

template <>
void xSyr<cl_float>::call_func()
{
  timer.Start(timer_id);
  clblasSsyr(buffer.order, buffer.uplo, buffer.N, buffer.alpha, buffer.X, buffer.offx,
              buffer.incx, buffer.A, buffer.offa, buffer.lda, 1, &queue_, 0, NULL,&event_);
  clWaitForEvents(1, &event_);
  timer.Stop(timer_id);
}

template <>
void xSyr<cl_double>::call_func()
{
  timer.Start(timer_id);
  clblasSsyr(buffer.order, buffer.uplo, buffer.N, buffer.alpha, buffer.X, buffer.offx,
              buffer.incx, buffer.A, buffer.offa, buffer.lda, 1, &queue_, 0, NULL,&event_);
  clWaitForEvents(1, &event_);
  timer.Stop(timer_id);
}

#endif // ifndef CLBLAS_BENCHMARK_XSYR_HXX__