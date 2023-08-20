#include "Halide.h"
#include "function_cholesky_MINI_wrapper.h"
#include "tiramisu/utils.h"
#include <iostream>
#include <time.h>
#include <fstream>
#include <chrono>

using namespace std::chrono;
using namespace std;      

int main(int, char **argv)
{

	double *b_A = (double*)malloc(40*40*sizeof(double));
	parallel_init_buffer(b_A, 40*40, (double)19);
	Halide::Buffer<double> buf01(b_A, 40, 40);

	double *b_C = (double*)malloc(40* sizeof(double));
	parallel_init_buffer(b_C, 40, (double)36);
	Halide::Buffer<double> buf02(b_C, 40);

	double *b_D = (double*)malloc(40*sizeof(double));
	parallel_init_buffer(b_D, 40, (double)19);
	Halide::Buffer<double> buf03(b_D, 40);

	double *b_E = (double*)malloc(40* sizeof(double));
	parallel_init_buffer(b_E, 40, (double)36);
	Halide::Buffer<double> buf04(b_E, 40);

	double *b_F = (double*)malloc(40*sizeof(double));
	parallel_init_buffer(b_F, 40, (double)19);
	Halide::Buffer<double> buf05(b_F, 40);




	int nb_exec = get_nb_exec();

	for (int i = 0; i < nb_exec; i++) 
	{  
		auto begin = std::chrono::high_resolution_clock::now(); 
		function_cholesky_MINI(buf01.raw_buffer());
		auto end = std::chrono::high_resolution_clock::now(); 

		std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() / (double)1000000 << " " << std::flush; 
	}
		std::cout << std::endl;

	return 0; 
}