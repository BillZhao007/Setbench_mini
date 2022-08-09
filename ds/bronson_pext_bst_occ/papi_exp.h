#pragma once

#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>

#include <chrono>
#include <iostream>
#include <sys/time.h>
#include <ctime>

using std::cout; using std::endl;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;

#include "papi.h"

using namespace std;

#define MAX_CPU 64
#define PAPI_MEASUREMENTS 4
long long papi_values[MAX_CPU][PAPI_MEASUREMENTS];
int64_t timer_values[MAX_CPU] = {0};

void papi_exp_init_lib() {
    if(PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) {
		printf("PAPI_library_init fail\n");
		exit(1);
	}
}

int papi_exp_start_counter(int tid) {
		timer_values[tid] = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        if(PAPI_thread_init(pthread_self) != PAPI_OK) {
			printf("PAPI_thread_init fail\n");
			exit(1);
		}
		int papi_event = PAPI_NULL;
		int papi_retval = PAPI_create_eventset(&papi_event);
		if(papi_retval != PAPI_OK){
			printf("PAPI create event fail\n");
			exit(-1);
		}
		papi_retval = PAPI_add_event(papi_event, PAPI_L3_TCM);
		papi_retval = PAPI_add_event(papi_event, PAPI_REF_CYC);
		papi_retval = PAPI_add_event(papi_event, PAPI_TOT_INS);
		// papi_retval = PAPI_add_event(papi_event, PAPI_L1_TCM);
		papi_retval = PAPI_add_event(papi_event, PAPI_L2_TCM);
		if(papi_retval != PAPI_OK){
			printf("PAPI add event fail: %d\n", papi_retval);
			exit(-1);
		}
		if(PAPI_start(papi_event) != PAPI_OK)
			papi_retval = PAPI_start(papi_event);
		PAPI_read(papi_event, papi_values[tid]);
		if(PAPI_read(papi_event, papi_values[tid]) != PAPI_OK){
			printf("PAPI read fail\n");
			exit(-1);
		}
        return papi_event;
}

void papi_exp_stop_counter(int tid, int papi_event) {
        long long papi_values_1[PAPI_MEASUREMENTS];
        if(PAPI_stop(papi_event, papi_values_1) != PAPI_OK){
			printf("PAPI_stop fail\n");
			exit(1);
		}
		if(PAPI_cleanup_eventset(papi_event) != PAPI_OK){
			printf("PAPI_cleanup_eventset fail\n");
			exit(1);
		}
		if(PAPI_destroy_eventset(&papi_event) != PAPI_OK){
			printf("PAPI_destroy_eventset fail\n");
			exit(1);
		}
		for(int i=0; i<PAPI_MEASUREMENTS; i++)
			papi_values[tid][i] = papi_values_1[i] - papi_values[tid][i];
		int64_t tmp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
		timer_values[tid] = tmp - timer_values[tid];
}

void papi_exp_print_counters(int opsNum, int threadNum) {
	long long print_values[PAPI_MEASUREMENTS] = {0};
	double throughput = 0.0;
	for(int i = 0; i < MAX_CPU; i++) {
		if(timer_values[i] > 0) 
			throughput += (double)opsNum / (double)threadNum / (double)(timer_values[i])/ 1000;
		for(int j = 0; j < PAPI_MEASUREMENTS; j++)
			print_values[j] += papi_values[i][j];
	}
	// throughput = (double)opsNum / throughput * (double)threadNum / 1000;
	cout << "PAPI_L3_TCM:  " << ((double)print_values[0] / opsNum) << endl;
	cout << "PAPI_REF_CYC: " << ((double)print_values[1] / opsNum) << endl;
	cout << "PAPI_TOT_INS: " << ((double)print_values[2] / opsNum) << endl;
	cout << "PAPI_L2_TCM:  " << ((double)print_values[3] / opsNum) << endl;
	cout << "Throughput:   " << throughput << endl;
}