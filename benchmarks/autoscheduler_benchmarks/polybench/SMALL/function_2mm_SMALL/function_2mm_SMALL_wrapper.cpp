#include "Halide.h"
#include "function_2mm_SMALL_wrapper.h"
#include "tiramisu/utils.h"
#include <iostream>
#include <time.h>
#include <fstream>
#include <chrono>
#include <omp.h>


int omp_do_par_for(void *user_context, int (*f)(void *, int, uint8_t *), int min, int extent, uint8_t *state) {
    int exit_status = 0;
    #pragma omp parallel for    
    for (int idx=min; idx<min+extent; idx++){
      int job_status = halide_do_task(user_context, f, idx, state);
      if (job_status) exit_status = job_status;
    }
    return exit_status;
}

using namespace std::chrono;
using namespace std;      

int main(int, char **argv)
{
    double *c_b_A = (double*)malloc(70*40*sizeof(double));
	parallel_init_buffer(c_b_A,70*40, (double)2);
	Halide::Buffer<double> b_A(c_b_A, 70,40);

	double *c_b_B = (double*)malloc(50*70*sizeof(double));
	parallel_init_buffer(c_b_B,50*70, (double)19);
	Halide::Buffer<double> b_B(c_b_B, 50,70);

	double *c_b_C = (double*)malloc(80*50*sizeof(double));
	parallel_init_buffer(c_b_C, 80*50, (double)29);
	Halide::Buffer<double> b_C(c_b_C,80,50);

	double *c_b_D = (double*)malloc(80*40*sizeof(double));
	parallel_init_buffer(c_b_D, 80*40, (double)29);
	Halide::Buffer<double> b_D(c_b_D,80,40);

	halide_set_custom_do_par_for(&omp_do_par_for);
	int nb_exec = get_nb_exec();

	for (int i = 0; i < nb_exec; i++) 
	{  
		auto begin = std::chrono::high_resolution_clock::now(); 
		function_2mm_SMALL(b_A.raw_buffer(),b_B.raw_buffer(),b_C.raw_buffer(),b_D.raw_buffer());
		auto end = std::chrono::high_resolution_clock::now(); 

		std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() / (double)1000000 << " " << std::flush; 
	}
		std::cout << std::endl;

	return 0; 
}