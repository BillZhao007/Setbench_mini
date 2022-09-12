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

const int threadNum = 20;

bool errors[threadNum] = {false};

double skewness;
#define PIM_NR 2048

#define BATCH_NUM 100
rand_distribution uni_dist;
rand_distribution zipf_dist[BATCH_NUM];

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
        for(int i = 0; i < n; i++) {
            key = rand_dist(&uni_dist, tid);
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

    if(ops == NULL) rand_uniform_init(&uni_dist, INT64_MAX);

    for(int i=0; i<tnum; i++) {
        input_wrappers[i].tid = i;
        input_wrappers[i].tree = tree;
        if(ops == NULL) {
            seed_and_print(i);
            input_wrappers[i].ops = NULL;
        }
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
        int bbb;
        for(int i = 0; i < n; i++) {
            bbb = i * BATCH_NUM / n;
            key = rand_dist(&(zipf_dist[bbb]), tid);
            exp_start_timer(tid);
            tree->find(tid, key);
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
        int bbb;
        for(int i = 0; i < n; i++) {
            bbb = i * BATCH_NUM / n;
            key = rand_dist(&(zipf_dist[bbb]), tid);
            exp_start_timer(tid);
            tree->insertIfAbsent(tid, key, value);
            exp_stop_timer(tid);
        }
    }
    else {
        for(int i = 0; i < n; i++) {
            key = ops[i].tsk.i.key;
            exp_start_timer(tid);
            tree->insertIfAbsent(tid, key, value);
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

    if(ops == NULL) {
        for(int i = 0; i < BATCH_NUM; i++)
            rand_pim_init(&(zipf_dist[i]), PIM_NR, skewness, INT64_MAX);
    }

    for(int i=0; i<tnum; i++) {
        input_wrappers[i].tid = i;
        input_wrappers[i].tree = tree;
        if(ops == NULL) {
            input_wrappers[i].ops = NULL;
            seed_and_print(i);
        }
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







template<class DATA_STRUCTURE_ADAPTER>
struct i64_wrapper {
    int tid;
    DATA_STRUCTURE_ADAPTER *tree;
    int n;
    int64_t* ops;
};

template<class DATA_STRUCTURE_ADAPTER>
void* init_per_thread_i64(void *ptr) {

    i64_wrapper<DATA_STRUCTURE_ADAPTER> *input_wrapper = (i64_wrapper<DATA_STRUCTURE_ADAPTER>*) ptr;

    int tid = input_wrapper->tid;
    auto tree = input_wrapper->tree;
    int n = input_wrapper->n;
    int64_t* ops = input_wrapper->ops;

    tree->initThread(tid);

    int64_t key;
    void *value = NULL;

    if(ops == NULL) {
        for(int i = 0; i < n; i++) {
            key = rand_dist(&uni_dist, tid);
            tree->insert(tid, key, value);
        }
    }
    else {
        for(int i = 0; i < n; i++) {
            key = ops[i];
            // value = ops[i].tsk.i.value;
            tree->insert(tid, key, value);
        }
    }

    tree->deinitThread(tid);

    return NULL;
}

template<class DATA_STRUCTURE_ADAPTER>
bool run_init_threads_i64(const int tnum, DATA_STRUCTURE_ADAPTER *tree, i64_array init_ops) {

    int init_n = init_ops.n;
    int64_t* ops =  init_ops.i64_map;
    int n_per_thread = init_n / tnum;

    pthread_t threads[tnum];
    i64_wrapper<DATA_STRUCTURE_ADAPTER> input_wrappers[tnum];
    
    int result;

    if(ops == NULL) rand_uniform_init(&uni_dist, INT64_MAX);

    for(int i=0; i<tnum; i++) {
        input_wrappers[i].tid = i;
        input_wrappers[i].tree = tree;
        if(ops == NULL) {
            seed_and_print(i);
            input_wrappers[i].ops = NULL;
        }
        else
            input_wrappers[i].ops = &(ops[n_per_thread * i]);
        input_wrappers[i].n = n_per_thread;

        result = pthread_create(&(threads[i]), NULL, init_per_thread_i64<DATA_STRUCTURE_ADAPTER>, &(input_wrappers[i]));
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
void* search_per_thread_i64(void *ptr) {

    i64_wrapper<DATA_STRUCTURE_ADAPTER> *input_wrapper = (i64_wrapper<DATA_STRUCTURE_ADAPTER>*) ptr;

    int tid = input_wrapper->tid;
    auto tree = input_wrapper->tree;
    int n = input_wrapper->n;
    int64_t* ops = input_wrapper->ops;

    int64_t key;

    int papi_event = papi_exp_start_counter(tid);

    tree->initThread(tid);

    if(ops == NULL) {
        int bbb;
        for(int i = 0; i < n; i++) {
            bbb = i * BATCH_NUM / n;
            key = rand_dist(&(zipf_dist[bbb]), tid);
            exp_start_timer(tid);
            tree->find(tid, key);
            exp_stop_timer(tid);
        }
    }
    else {
        for(int i = 0; i < n; i++) {
            key = ops[i];
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
void* insert_per_thread_i64(void *ptr) {

    i64_wrapper<DATA_STRUCTURE_ADAPTER> *input_wrapper = (i64_wrapper<DATA_STRUCTURE_ADAPTER>*) ptr;

    int tid = input_wrapper->tid;
    auto tree = input_wrapper->tree;
    int n = input_wrapper->n;
    int64_t* ops = input_wrapper->ops;

    int64_t key;
    void *value = NULL;

    int papi_event = papi_exp_start_counter(tid);

    tree->initThread(tid);

    if(ops == NULL) {
        int bbb;
        for(int i = 0; i < n; i++) {
            bbb = i * BATCH_NUM / n;
            key = rand_dist(&(zipf_dist[bbb]), tid);
            exp_start_timer(tid);
            tree->insertIfAbsent(tid, key, value);
            exp_stop_timer(tid);
        }
    }
    else {
        for(int i = 0; i < n; i++) {
            key = ops[i];
            exp_start_timer(tid);
            tree->insertIfAbsent(tid, key, value);
            exp_stop_timer(tid);
        }
    }

    tree->deinitThread(tid);

    papi_exp_stop_counter(tid, papi_event);

    return NULL;
}

template<class DATA_STRUCTURE_ADAPTER>
bool run_test_threads_i64(const int tnum, DATA_STRUCTURE_ADAPTER *tree, i64_array test_ops, operation_t op_type) {

    int init_n = test_ops.n;
    int64_t* ops =  test_ops.i64_map;
    int n_per_thread = init_n / tnum;

    pthread_t threads[tnum];
    i64_wrapper<DATA_STRUCTURE_ADAPTER> input_wrappers[tnum];
    
    int result;

    if(ops == NULL) {
        for(int i = 0; i < BATCH_NUM; i++)
            rand_pim_init(&(zipf_dist[i]), PIM_NR, skewness, INT64_MAX);
    }

    for(int i=0; i<tnum; i++) {
        input_wrappers[i].tid = i;
        input_wrappers[i].tree = tree;
        if(ops == NULL) {
            input_wrappers[i].ops = NULL;
            seed_and_print(i);
        }
        else
            input_wrappers[i].ops = &(ops[n_per_thread * i]);
        input_wrappers[i].n = n_per_thread;
        
        if(op_type == operation_t::predecessor_t)
            result = pthread_create(&(threads[i]), NULL, search_per_thread_i64<DATA_STRUCTURE_ADAPTER>, &(input_wrappers[i]));
        else if(op_type == operation_t::insert_t)
            result = pthread_create(&(threads[i]), NULL, insert_per_thread_i64<DATA_STRUCTURE_ADAPTER>, &(input_wrappers[i]));
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

double rw_ratio;

template<class DATA_STRUCTURE_ADAPTER>
struct ycsb_wrapper {
    int tid;
    DATA_STRUCTURE_ADAPTER *tree;
    int n;
};

template<class DATA_STRUCTURE_ADAPTER>
void* ycsb_per_thread_i64(void *ptr) {

    ycsb_wrapper<DATA_STRUCTURE_ADAPTER> *input_wrapper = (ycsb_wrapper<DATA_STRUCTURE_ADAPTER>*) ptr;

    int tid = input_wrapper->tid;
    auto tree = input_wrapper->tree;
    int n = input_wrapper->n;

    int64_t key;
    void *value = NULL;
    float rw_flag;

    int papi_event = papi_exp_start_counter(tid);

    tree->initThread(tid);

    int bbb;
    for(int i = 0; i < n; i++) {
        bbb = i * BATCH_NUM / n;
        key = rand_dist(&(zipf_dist[bbb]), tid);
        rw_flag = rand_float(tid);
        exp_start_timer(tid);
        if(rw_flag < rw_ratio)
            tree->find(tid, key);
        else
            tree->insertIfAbsent(tid, key, value);
        exp_stop_timer(tid);
    }

    tree->deinitThread(tid);

    papi_exp_stop_counter(tid, papi_event);

    return NULL;
}

template<class DATA_STRUCTURE_ADAPTER>
bool run_ycsb_threads_i64(const int tnum, DATA_STRUCTURE_ADAPTER *tree, int init_n) {

    int n_per_thread = init_n / tnum;

    pthread_t threads[tnum];
    ycsb_wrapper<DATA_STRUCTURE_ADAPTER> input_wrappers[tnum];

    int result;

    for(int i = 0; i < BATCH_NUM; i++)
        rand_pim_init(&(zipf_dist[i]), PIM_NR, skewness, INT64_MAX);

    for(int i=0; i<tnum; i++) {
        input_wrappers[i].tid = i;
        input_wrappers[i].tree = tree;
        seed_and_print(i);
        input_wrappers[i].n = n_per_thread;

        result = pthread_create(&(threads[i]), NULL, ycsb_per_thread_i64<DATA_STRUCTURE_ADAPTER>, &(input_wrappers[i]));

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

    const int KEY_NEG_INFTY = std::numeric_limits<int>::min();
    const int unused1 = 0;
    void * const unused2 = NULL;
    Random64 * const unused3 = NULL;

    auto tree = new ds_adapter<int64_t, void*>(threadNum, KEY_NEG_INFTY, unused1, unused2, unused3);

    seed_and_print(0);

    // ops_array init_ops = {.n = 400, .operation_map = NULL};
    // ops_array search_ops = {.n = 400, .operation_map = NULL};
    // ops_array insert_ops = {.n = 40, .operation_map = NULL};
    // ops_array init_ops, search_ops, insert_ops;
    i64_array init_ops, search_ops, insert_ops;
    int dataset_size, init_size, test_size;
    
    if(argc == 1) {
        init_ops = read_i64_file(string("/usr0/home/yiweiz3/wiki_data/wiki_1200M_keys.i64binary"));
        cout<<"Read file finished"<<endl;
        cout<<init_ops.n<<" "<<init_ops.i64_map<<endl;
        dataset_size = init_ops.n;
        init_size = dataset_size  * 5 / 6;
        test_size = dataset_size / 6;
        init_ops.n = init_size;
        search_ops.n = test_size;
        insert_ops.n = test_size;
        search_ops.i64_map = &(init_ops.i64_map[init_size]);
        insert_ops.i64_map = search_ops.i64_map;
        run_init_threads_i64(threadNum, tree, init_ops);
    }
    else if(argc == 3) {
        init_ops.i64_map = NULL;
        init_ops.n = atoi(argv[1]);
        skewness = atof(argv[2]);
        run_init_threads_i64(threadNum, tree, init_ops);
        cout<<"Init finished"<<endl;
    }
    else if(argc == 4) {
        init_ops.i64_map = NULL;
        init_ops.n = atoi(argv[1]);
        skewness = atof(argv[2]);
        rw_ratio = atof(argv[3]);
        run_init_threads_i64(threadNum, tree, init_ops);
        cout<<"Init finished"<<endl;
        test_size = atoi(argv[1]) / 5;
        papi_exp_init_lib();
        run_ycsb_threads_i64(threadNum, tree, test_size);
        delete tree;
        return 0;
    }
    else return 1;

    papi_exp_init_lib();

    if(argc != 1) {
        search_ops.i64_map = NULL;
        search_ops.n = atoi(argv[1]) / 5;
    }
    cout<<search_ops.n<<" "<<search_ops.i64_map<<endl;

    run_test_threads_i64(threadNum, tree, search_ops, operation_t::predecessor_t);
    cout<<"Search test finished."<<endl;

    if(argc != 1) {
        insert_ops.i64_map = NULL;
        insert_ops.n = atoi(argv[1]) / 5;
    }
    cout<<insert_ops.n<<" "<<insert_ops.i64_map<<endl;

    run_test_threads_i64(threadNum, tree, insert_ops, operation_t::insert_t);
    cout<<"Insert test finished."<<endl;

    if(argc == 1)
    if ( munmap( (void*)(init_ops.i64_map), dataset_size * sizeof(int64_t) ) == -1) {
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

// int main(int argc, char** argv) {

//     const int NUM_THREADS = 1;
//     const int KEY_NEG_INFTY = std::numeric_limits<int>::min();
//     const int unused1 = 0;
//     void * const unused2 = NULL;
//     Random64 * const unused3 = NULL;

//     auto tree = new ds_adapter<int, void *>(NUM_THREADS, KEY_NEG_INFTY, unused1, unused2, unused3);

//     const int threadID = 0;

//     tree->initThread(threadID);

//     void * oldVal = tree->insertIfAbsent(threadID, 7, (void *) 1020);
//     assert(oldVal == tree->getNoValue());

//     bool result = tree->contains(threadID, 7);
//     assert(result);

//     result = tree->contains(threadID, 8);
//     assert(!result);

//     void * val = tree->find(threadID, 7);
//     assert(val == (void *) 1020);

//     val = tree->erase(threadID, 7);
//     assert(val == (void *) 1020);

//     result = tree->contains(threadID, 7);
//     assert(!result);

//     tree->deinitThread(threadID);

//     delete tree;

//     std::cout<<"Passed quick tests."<<std::endl;

//     return 0;
// }
