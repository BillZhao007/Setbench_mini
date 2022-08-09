/**
 * Author: Trevor Brown (me [at] tbrown [dot] pro).
 * Copyright 2018.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <iostream>
#include <limits>
#include <cassert>

#include "adapter.h"

#include "pim_exp.hpp"
#include "papi_exp.h"
#include "zipf.h"

using namespace std;

const int threadNum = 40;

bool errors[threadNum] = {false};

double skewness;
#define PIM_NR 2560

// #define DATA_STRUCTURE_ADAPTER ds_adapter<int64_t, void*>

template<class DATA_STRUCTURE_ADAPTER>
struct init_wrapper {
    int tid;
    DATA_STRUCTURE_ADAPTER *tree;
    int n;
    operation* ops;
};

template<class DATA_STRUCTURE_ADAPTER>
void* init_per_thread(void *ptr) {

    init_wrapper<DATA_STRUCTURE_ADAPTER> *input_wrapper = (init_wrapper<DATA_STRUCTURE_ADAPTER>*) ptr;

    int tid = input_wrapper->tid;
    auto tree = input_wrapper->tree;
    int n = input_wrapper->n;
    operation* ops = input_wrapper->ops;

    tree->initThread(tid);

    int64_t key;
    void *value = NULL;

    if(ops == NULL) {
        rand_distribution uni_dist;
        rand_uniform_init(&uni_dist, INT64_MAX);
        for(int i = 0; i < n; i++) {
            key = rand_dist(&uni_dist);
            tree->insert(tid, key, value);
        }
    }
    else {
        for(int i = 0; i < n; i++) {
            key = ops[i].tsk.i.key;
            // value = ops[i].tsk.i.value;
            tree->insert(tid, key, value);
        }
    }

    tree->deinitThread(tid);

    return NULL;
}

template<class DATA_STRUCTURE_ADAPTER>
bool run_init_threads(const int tnum, DATA_STRUCTURE_ADAPTER *tree, ops_array init_ops) {

    int init_n = init_ops.n;
    operation* ops =  init_ops.operation_map;
    int n_per_thread = init_n / tnum;

    pthread_t threads[tnum];
    init_wrapper<DATA_STRUCTURE_ADAPTER> input_wrappers[tnum];
    
    int result;
    for(int i=0; i<tnum; i++) {
        input_wrappers[i].tid = i;
        input_wrappers[i].tree = tree;
        if(ops == NULL)
            input_wrappers[i].ops = NULL;
        else
            input_wrappers[i].ops = &(ops[n_per_thread * i]);
        input_wrappers[i].n = n_per_thread;

        result = pthread_create(&(threads[i]), NULL, init_per_thread<DATA_STRUCTURE_ADAPTER>, &(input_wrappers[i]));
		if (result != 0) {
			printf("Thread creation error\n");
			return false;
		}
    }
    for (int i = 0; i < tnum; i++) {
		result = pthread_join(threads[i], NULL);
		if (result != 0) {
			printf("Thread join error\n");
			return false;
		}
	}
    return true;
}

template<class DATA_STRUCTURE_ADAPTER>
void* search_per_thread(void *ptr) {

    init_wrapper<DATA_STRUCTURE_ADAPTER> *input_wrapper = (init_wrapper<DATA_STRUCTURE_ADAPTER>*) ptr;

    int tid = input_wrapper->tid;
    auto tree = input_wrapper->tree;
    int n = input_wrapper->n;
    operation* ops = input_wrapper->ops;

    int64_t key;

    int papi_event = papi_exp_start_counter(tid);

    tree->initThread(tid);

    if(ops == NULL) {
        bool result;
        rand_distribution zipf_dist;
        rand_pim_init(&zipf_dist, PIM_NR, skewness, INT64_MAX);
        for(int i = 0; i < n; i++) {
            key = rand_dist(&zipf_dist);
            exp_start_timer(tid);
            result = tree->contains(tid, key);
            exp_stop_timer(tid);
        }
    }
    else {
        for(int i = 0; i < n; i++) {
            key = ops[i].tsk.p.key;
            exp_start_timer(tid);
            tree->find(tid, key);
            exp_stop_timer(tid);
        }
    }

    tree->deinitThread(tid);

    papi_exp_stop_counter(tid, papi_event);

    return NULL;
}

template<class DATA_STRUCTURE_ADAPTER>
void* insert_per_thread(void *ptr) {

    init_wrapper<DATA_STRUCTURE_ADAPTER> *input_wrapper = (init_wrapper<DATA_STRUCTURE_ADAPTER>*) ptr;

    int tid = input_wrapper->tid;
    auto tree = input_wrapper->tree;
    int n = input_wrapper->n;
    operation* ops = input_wrapper->ops;

    int64_t key;
    void *value = NULL;

    int papi_event = papi_exp_start_counter(tid);

    tree->initThread(tid);

    if(ops == NULL) {
        rand_distribution zipf_dist;
        rand_pim_init(&zipf_dist, PIM_NR, skewness, INT64_MAX);
        for(int i = 0; i < n; i++) {
            key = rand_dist(&zipf_dist);
            exp_start_timer(tid);
            tree->insert(tid, key, value);
            exp_stop_timer(tid);
        }
    }
    else {
        for(int i = 0; i < n; i++) {
            key = ops[i].tsk.i.key;
            exp_start_timer(tid);
            tree->insert(tid, key, value);
            exp_stop_timer(tid);
        }
    }

    tree->deinitThread(tid);

    papi_exp_stop_counter(tid, papi_event);

    return NULL;
}

template<class DATA_STRUCTURE_ADAPTER>
bool run_test_threads(const int tnum, DATA_STRUCTURE_ADAPTER *tree, ops_array test_ops, operation_t op_type) {

    int init_n = test_ops.n;
    operation* ops =  test_ops.operation_map;
    int n_per_thread = init_n / tnum;

    pthread_t threads[tnum];
    init_wrapper<DATA_STRUCTURE_ADAPTER> input_wrappers[tnum];
    
    int result;

    for(int i=0; i<tnum; i++) {
        input_wrappers[i].tid = i;
        input_wrappers[i].tree = tree;
        if(ops == NULL)
            input_wrappers[i].ops = NULL;
        else
            input_wrappers[i].ops = &(ops[n_per_thread * i]);
        input_wrappers[i].n = n_per_thread;
        
        if(op_type == operation_t::predecessor_t)
            result = pthread_create(&(threads[i]), NULL, search_per_thread<DATA_STRUCTURE_ADAPTER>, &(input_wrappers[i]));
        else if(op_type == operation_t::insert_t)
            result = pthread_create(&(threads[i]), NULL, insert_per_thread<DATA_STRUCTURE_ADAPTER>, &(input_wrappers[i]));
        else return false;

		if (result != 0) {
			printf("Thread creation error\n");
			return false;
		}
    }
    for (int i = 0; i < tnum; i++) {
		result = pthread_join(threads[i], NULL);
		if (result != 0) {
			printf("Thread join error\n");
			return false;
		}
	}
    papi_exp_print_counters(init_n, tnum);
    return true;
}

int main(int argc, char** argv) {

    const int64_t KEY_ANY = 0;
    const int64_t unused1 = 0;
    void* unused2 = NULL;
    Random64 * const unused3 = NULL;

    auto tree = new ds_adapter<int64_t, void*>(threadNum, KEY_ANY, unused1, unused2, unused3);

    seed_and_print();

    // ops_array init_ops = {.n = 400, .operation_map = NULL};
    // ops_array search_ops = {.n = 400, .operation_map = NULL};
    // ops_array insert_ops = {.n = 40, .operation_map = NULL};
    ops_array init_ops, search_ops, insert_ops;
    
    if(argc == 1) 
        init_ops = read_op_file(string("/usr0/home/yiweiz3/wiki_data/wiki_500M_init.binary"));
    else if(argc == 3) {
        init_ops.operation_map = NULL;
        init_ops.n = atoi(argv[1]);
        skewness = atof(argv[2]);
    }
    else return 1;
    cout<<"Read init file finished"<<endl;
    cout<<init_ops.n<<" "<<init_ops.operation_map<<endl;

    run_init_threads(threadNum, tree, init_ops);
    cout<<"Initialize finished."<<endl;

    if(argc == 1) {
        if ( munmap( (void*)(init_ops.operation_map), init_ops.n * sizeof(operation) ) == -1) {
            printf("munmap failed with error\n");
        }
    }

    papi_exp_init_lib();

    if(argc == 1) 
        search_ops = read_op_file(string("/usr0/home/yiweiz3/wiki_data/wiki_100M_predecessor.binary"));
    else {
        search_ops.operation_map = NULL;
        search_ops.n = atoi(argv[1]) / 5;
    }
    cout<<"Read init file finished"<<endl;
    cout<<search_ops.n<<" "<<search_ops.operation_map<<endl;

    run_test_threads(threadNum, tree, search_ops, operation_t::predecessor_t);
    cout<<"Search test finished."<<endl;

    if(argc == 1) 
    if ( munmap( (void*)(search_ops.operation_map), search_ops.n * sizeof(operation) ) == -1) {
        printf("munmap failed with error\n");
    }

    if(argc == 1)
        insert_ops = read_op_file(string("/usr0/home/yiweiz3/wiki_data/wiki_100M_insert.binary"));
    else {
        insert_ops.operation_map = NULL;
        insert_ops.n = atoi(argv[1]) / 5;
    }
    cout<<"Read init file finished"<<endl;
    cout<<insert_ops.n<<" "<<insert_ops.operation_map<<endl;

    run_test_threads(threadNum, tree, insert_ops, operation_t::insert_t);
    cout<<"Insert test finished."<<endl;

    if(argc == 1)
    if ( munmap( (void*)(insert_ops.operation_map), insert_ops.n * sizeof(operation) ) == -1) {
        printf("munmap failed with error\n");
    }

    delete tree;

    int threadID;
    for(threadID = 0; threadID < threadNum; threadID++) {
        if(errors[threadID])
            return 1;
    }

    std::cout<<"New test\nPassed quick tests."<<std::endl;

    return 0;
}
