// Copyright (c)       2017 Madhavan Seshadri
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

#include <CL/cl.h>

//add this line for compiling with Visual Studio 
#pragma comment(lib, "OpenCL.lib")

//###########################################################################
//Properties for reading the kernel
//###########################################################################

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

#define SOURCE_SIZE_MAX (0x100000)

///////////////////////////////////////////////////////////////////////////////
double mysecond() {
	clock_t time;
	time = clock();
	return ((double) time * 1e-9);
}

//###########################################################################
//checktick function
//###########################################################################
int checktick() {
	static const size_t M = 20;
	int minDelta, Delta;
	double t1, t2, timesfound[20];
	size_t i;
	// Collect a sequence of M unique time values from the system.
	for (i = 0; i < M; i++) {
		t1 = mysecond();
		while (((t2 = mysecond()) - t1) < 1.0E-6)
			;
		timesfound[i] = t1 = t2;
	}

	// Determine the minimum difference between these M values.
	// This result will be our estimate (in microseconds) for the
	// clock granularity.
	minDelta = 1000000;
	for (i = 1; i < M; i++) {
		Delta = (int) (1.0E6 * (timesfound[i] - timesfound[i - 1]));
		minDelta = (int) fmin(minDelta, (int) fmax(Delta, 0));
	}

	return (minDelta);
}

void checkKernel(int err, cl_program program, cl_device_id deviceId){
	 if (err != CL_SUCCESS) {
	char *buff_erro;
	cl_int errcode;
	size_t build_log_len;
	errcode = clGetProgramBuildInfo(program, deviceId, CL_PROGRAM_BUILD_LOG, 0, NULL, &build_log_len);
	if (errcode) {
            printf("clGetProgramBuildInfo failed at line %d\n", __LINE__);
            exit(-1);
        }

    buff_erro = malloc(build_log_len);
    if (!buff_erro) {
        printf("malloc failed at line %d\n", __LINE__);
        exit(-2);
    }

    errcode = clGetProgramBuildInfo(program, deviceId, CL_PROGRAM_BUILD_LOG, build_log_len, buff_erro, NULL);
    if (errcode) {
        printf("clGetProgramBuildInfo failed at line %d\n", __LINE__);
        exit(-3);
    }

    fprintf(stderr,"Build log: \n%s\n", buff_erro); //Be careful with  the fprint
    free(buff_erro);
	}
}
//###########################################################################
//stream benchmark
//###########################################################################

double** stream_benchmark(int size, int iterations) {

	cl_device_id deviceId = NULL;
	cl_context context = NULL;
	cl_command_queue commandQueue = NULL;

	//Memory objects for kernel parameters
	cl_mem aMemobj = NULL;
	cl_mem bMemobj = NULL;
	cl_mem cMemobj = NULL;
	cl_mem sMemobj = NULL;
	cl_mem scaleMemobj = NULL;
	cl_mem sizeMemobj = NULL;

	cl_program program = NULL;
	cl_kernel kernel = NULL;
	cl_platform_id platformId = NULL;
	cl_uint numDevices;
	cl_uint numPlatforms;
	cl_int ret;
	size_t contextDescriptorSize;

	//Define vectors to be used by the kernel
	double* a;
	double* b;
	double* c;
	int* s;
	double* scale;
	double quantum;
	int iteration;
	size_t i;
	
	//reading kernel from file
	FILE *file;
	char fileName[] = "./streamKernel.cl";
	char *kernelSource;
	size_t sourceSize;
	
	//Load the source code from file
	file = fopen(fileName, "r");
	if(!file) {
		printf("Failed to load the kernel file. \n");
		exit(1);
	}

	a = malloc(sizeof(double) * size);
	b = malloc(sizeof(double) * size);
	c = malloc(sizeof(double) * size);
	scale = malloc(sizeof(double));
	s = malloc(sizeof(size_t));

	s[0] = size;
	scale[0] = 2.0;

	//Initialize all the arrays
	for(i = 0 ; i < size ; i++)
	{
		a[i] = 1.0;
		b[i] = 2.0;
		c[i] = 0.0;
	}

	kernelSource = (char *) malloc(SOURCE_SIZE_MAX);
	sourceSize = fread(kernelSource, 1, SOURCE_SIZE_MAX, file);
	fclose(file);

	//Get platform information
	ret = clGetPlatformIDs(1, &platformId, &numPlatforms);

	//get list of devices
	ret = clGetDeviceIDs(platformId, CL_DEVICE_TYPE_DEFAULT, 1, &deviceId, &numDevices);
	
	//create opencl device context
	context = clCreateContext(NULL, 1, &deviceId, NULL, NULL, &ret);

	//get device context
	clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, 0, &contextDescriptorSize); 

	//command queue for the first device
	commandQueue = clCreateCommandQueue(context, deviceId, CL_QUEUE_PROFILING_ENABLE, &ret);

	//Create memory object
	aMemobj = clCreateBuffer(context, CL_MEM_READ_WRITE,sizeof(double) * size, a, &ret);
	bMemobj = clCreateBuffer(context, CL_MEM_READ_WRITE,sizeof(double) * size, b, &ret);
	cMemobj = clCreateBuffer(context, CL_MEM_READ_WRITE,sizeof(double) * size, c, &ret);
	sMemobj = clCreateBuffer(context, CL_MEM_READ_WRITE,sizeof(int) , s, &ret);
	scaleMemobj = clCreateBuffer(context, CL_MEM_READ_WRITE,sizeof(double), scale, &ret);
	sizeMemobj = clCreateBuffer(context, CL_MEM_READ_WRITE,sizeof(int), &size, &ret);

	//Create kernel program from source
	program = clCreateProgramWithSource(context, 1, (const char **)&kernelSource,(const size_t *)&sourceSize, &ret);

	const size_t global_work_size = max(pow(2,ceil(log(size)/log(2))),512);
	const size_t local_work_size = 512;

	//Build the kernel program
	ret = clBuildProgram(program, 1, &deviceId, "-I ./", NULL, NULL);

	//Create a opencl kernel
	kernel = clCreateKernel(program, "STREAM_Scale", &ret);
	checkKernel(ret, program, deviceId);
	
	//Pass arguments to kernel
	ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&aMemobj);
	ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&bMemobj);
	ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&scaleMemobj);
	ret = clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *)&sizeMemobj);


	double time = mysecond();
	//Execute opencl kernel
	cl_event event = NULL;
	cl_ulong time_start = 0, time_end = 0;
	ret = clEnqueueNDRangeKernel (commandQueue, kernel, 1, 0, &global_work_size, &local_work_size, 0, NULL, &event);
	clWaitForEvents(1, &event);

    clFinish(commandQueue);
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);

    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);
	
	time = 1.0E-3 * (time_end-time_start);

	quantum = checktick();
	if (quantum >= 1) {
		printf("Clock precision is %lf microseconds.\n", quantum);
	} else {
		printf("Clock precision is less than one microsecond.\n", quantum);
		quantum = 1;
	}

	printf("Each test below will take on the order of %lf microseconds. (= %lf clock ticks)\nIncrease the size of the arrays if this shows that you are not getting at least 20 clock ticks per test.\n", time, (time/quantum));
	printf("WARNING -- The above is only a rough guideline.\nFor best results, please be sure you know the precision of your system timer.\n");

	ret = clReleaseKernel(kernel);
	ret = clReleaseMemObject(scaleMemobj);
	ret = clFlush(commandQueue);

	scale[0] = 3.0;
	scaleMemobj = clCreateBuffer(context, CL_MEM_READ_WRITE,sizeof(double), scale, &ret);

	double *timing[4];
	int j;
	for (j = 0; j < 4; j++)
		timing[j] = malloc(sizeof(double) * 4 * iterations);

	for (iteration = 0; iteration != iterations; ++iteration) {

		// Copy
		timing[0][iteration] = mysecond();

		//Create a opencl kernel
		kernel = clCreateKernel(program, "STREAM_Copy", &ret);

		//Pass arguments to kernel
		ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&aMemobj);
		ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&bMemobj);
		ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&sizeMemobj);

		event = NULL;
		time_start = 0, time_end = 0;

		ret = clEnqueueNDRangeKernel (commandQueue, kernel, 1, 0, &global_work_size, &local_work_size, 0, NULL, &event);
		clWaitForEvents(1, &event);

	    clFinish(commandQueue);
	    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
	    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);


		timing[0][iteration] = (time_end-time_start) * 1e-9;

		ret = clFlush(commandQueue);
		ret = clReleaseKernel(kernel);

		//Scale
		timing[1][iteration] = mysecond();

		//Create a opencl kernel
		kernel = clCreateKernel(program, "STREAM_Scale", &ret);

		//Pass arguments to kernel
		ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&aMemobj);
		ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&bMemobj);
		ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&scaleMemobj);
		ret = clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *)&sizeMemobj);

		event = NULL;
		time_start = 0, time_end = 0;

		ret = clEnqueueNDRangeKernel (commandQueue, kernel, 1, 0, &global_work_size, &local_work_size, 0, NULL, &event);
		clWaitForEvents(1, &event);

	    clFinish(commandQueue);
	    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
	    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);

		timing[1][iteration] = (time_end-time_start) * 1e-9;

		ret = clFlush(commandQueue);
		ret = clReleaseKernel(kernel);

		// Add
		timing[2][iteration] = mysecond();

		//Create a opencl kernel
		kernel = clCreateKernel(program, "STREAM_Add", &ret);

		ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&aMemobj);
		ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&bMemobj);
		ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&cMemobj);
		ret = clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *)&sizeMemobj);

		event = NULL;
		time_start = 0, time_end = 0;

		ret = clEnqueueNDRangeKernel (commandQueue, kernel, 1, 0, &global_work_size, &local_work_size, 0, NULL, &event);
		clWaitForEvents(1, &event);

	    clFinish(commandQueue);
	    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
	    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);

		timing[2][iteration] = (time_end-time_start)* 1e-9;

		ret = clFlush(commandQueue);
		ret = clReleaseKernel(kernel);

		// Triad
		timing[3][iteration] = mysecond();

		//Create a opencl kernel
		kernel = clCreateKernel(program, "STREAM_Triad", &ret);

		ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&aMemobj);
		ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&bMemobj);
		ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&cMemobj);
		ret = clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *)&scaleMemobj);
		ret = clSetKernelArg(kernel, 4, sizeof(cl_mem), (void *)&sizeMemobj);

		event = NULL;
		time_start = 0, time_end = 0;

		ret = clEnqueueNDRangeKernel (commandQueue, kernel, 1, 0, &global_work_size, &local_work_size, 0, NULL, &event);
		clWaitForEvents(1, &event);

	    clFinish(commandQueue);
	    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
	    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);

		timing[3][iteration] = (time_end-time_start) * 1e-9;

		ret = clFlush(commandQueue);
		ret = clReleaseKernel(kernel);
	}

	//Before program termination
	ret = clFlush(commandQueue);
	ret = clFinish(commandQueue);
	ret = clReleaseProgram(program);
	ret = clReleaseMemObject(aMemobj);
	ret = clReleaseMemObject(bMemobj);
	ret = clReleaseMemObject(cMemobj);
	ret = clReleaseMemObject(scaleMemobj);
	ret = clReleaseMemObject(sizeMemobj);
	ret = clReleaseCommandQueue(commandQueue);
	ret = clReleaseContext(context);

	return timing;
}

//###########################################################################
//main
//###########################################################################

int main(int argc, char*argv[]) {

	if (argc != 3) {
		printf("Usage: %s #elements #iterations", argv[0]);
		exit(1);
	}

	int size = atoi(argv[1]);
	int iterations = atoi(argv[2]);
	int iteration;
	/* --- SUMMARY --- */
	const char *label[4] = {
		"Copy:      ",
		"Scale:     ",
		"Add:       ",
		"Triad:     "
	};

	const double bytes[4] = {
		(double)(2 * sizeof(double) * size),
		(double)(2 * sizeof(double) * size),
		(double)(3 * sizeof(double) * size),
		(double)(3 * sizeof(double) * size)
	};

	double minTime[4];
	double maxTime[4];
	double avgTime[4];

	int j;

	double time_total = mysecond();
	double **timing = stream_benchmark(size,iterations);
	time_total = mysecond() - time_total;

	for(j = 0;j<4;j++)
		minTime[j] = 999999.9;

	for(iteration = 1; iteration < iterations; iteration++) {
		for(j = 0;j<4;j++) {
			minTime[j] = fmin(minTime[j], timing[j][iteration]);
			maxTime[j] = fmax(maxTime[j], timing[j][iteration]);
			avgTime[j] = avgTime[j] + timing[j][iteration];
		}
	}

	printf("Function    Best Rate MB/s  Avg time     Min time     Max time\n");
	for (j=0; j<4; j++) {
		avgTime[j] = avgTime[j]/(double)(iterations-1);

		printf("%s%12.1f  %11.6f  %11.6f  %11.6f\n", label[j],
				1.0E-06 * bytes[j]/minTime[j],
				avgTime[j],
				minTime[j],
				maxTime[j]);
	}

	return 0;
}
 