#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <assert.h>
#include <time.h>

#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>

#define MAX_ZIPF_RANGES 10000

// Sample a number in [0,max) with all numbers having equal probability.
#define DIST_UNIFORM 0

// Sample a number in [0,max) with Zipf probabilities: the k'th most
// common number has probability proportional to 1 / (k**skew).
#define DIST_ZIPF 1

// Same as DIST_ZIPF, but with 0 being the most common number, 1 the
// second most common, and so on.
#define DIST_ZIPF_RANK 2

#define DIST_PIM 3

uint64_t rand_state;

static void rand_seed(uint64_t s) {
	rand_state = s;
}

static uint32_t rand_dword() {
	rand_state = 6364136223846793005 * rand_state + 1;
	return rand_state >> 32;
}

static uint32_t rand_dword_r(uint64_t* state) {
	*state = 6364136223846793005 * (*state) + 1;
	return (*state) >> 32;
}

static uint64_t rand_uint64() {
	return (((uint64_t)rand_dword()) << 32) + rand_dword();
}

static float rand_float() {
	return ((float)rand_dword()) / UINT32_MAX;
}

static void random_bytes(uint8_t* buf, int count) {
	int i;
	for (i = 0;i < count;i++)
		buf[i] = rand_dword() % 256;
}

static long int seed_and_print() {
	struct timeval now;
	long int seed;
	gettimeofday(&now, NULL);
	seed = now.tv_sec * 1000000 + now.tv_usec;
	// printf("Using seed %ld\n", seed);
	rand_seed(seed);
	return seed;
}

typedef struct {
	// The weight of all ranges up to and including this one
	double weight_cumsum;

	uint64_t start;
	uint64_t size;
} zipf_range;

typedef struct {
	zipf_range zipf_ranges[MAX_ZIPF_RANGES];
	uint64_t num_zipf_ranges;
	double total_weight;
	double skew;
	uint64_t max;
	int type;

	uint64_t pim_idx[MAX_ZIPF_RANGES];
	uint64_t pim_idx_max;
} rand_distribution;

rand_distribution zipf_dist_cache;

#define ZIPF_ERROR_RATIO 1.01

double rand_double() {
	return ((double)rand_uint64()) / UINT64_MAX;
}

void rand_uniform_init(rand_distribution* dist, uint64_t max) {
	dist->max = max;
	dist->type = DIST_UNIFORM;
}

void rand_zipf_init(rand_distribution* dist, uint64_t max, double skew) {
	uint64_t i;
	double total_weight = 0.0;
	uint64_t range_start = 0;
	uint64_t range_end;
	uint64_t range_num = 0;

	if (zipf_dist_cache.max == max && zipf_dist_cache.skew == skew) {
		*dist = zipf_dist_cache;
		return;
	}

	// A multiplier M s.t. the ratio between the weights of the k'th element
	// and the (k*M)'th element is at most ZIPF_ERROR_RATIO
	double range_size_multiplier = pow(ZIPF_ERROR_RATIO, 1.0 / skew);

	while (range_start < max) {
		zipf_range* range = &(dist->zipf_ranges[range_num]);
		range->start = range_start;
		range_end = (uint64_t) floor((range->start + 1) * range_size_multiplier);
		range->size = range_end - range->start;
		if (range->start + range->size > max)
			range->size = max - range->start;
		for (i = 0;i < range->size;i++)
			total_weight += 1.0 / pow(i + range->start + 1, skew);

		range->weight_cumsum = total_weight;

		// Compute start point of the next range
		range_start = range->start + range->size;
		range_num++;
	}

	dist->num_zipf_ranges = range_num;
	dist->total_weight = total_weight;
	dist->max = max;
	dist->type = DIST_ZIPF;
	dist->skew = skew;

	zipf_dist_cache = *dist;
}

void rand_zipf_rank_init(rand_distribution* dist, uint64_t max, double skew) {
	rand_zipf_init(dist, max, skew);
	dist->type = DIST_ZIPF_RANK;
}

void rand_pim_init(rand_distribution* dist, uint64_t max, double skew, uint64_t idx_max) {
	rand_zipf_init(dist, max, skew);
	dist->type = DIST_PIM;
	dist->pim_idx_max = idx_max;
	uint64_t j, tmp;
	for(uint64_t i = 0; i < max; i++)
		dist->pim_idx[i] = i;
	for(uint64_t i = 0; i < max; i++) {
		j = rand_uint64() % (max - i);
		tmp = dist->pim_idx[i];
		dist->pim_idx[i] = dist->pim_idx[j];
		dist->pim_idx[j] = tmp;
	}
}

uint64_t mix(uint64_t x) {
	x ^= x >> 33;
	x *= 0xC2B2AE3D27D4EB4FULL;  // Random prime
	x ^= x >> 29;
	x *= 0x165667B19E3779F9ULL;  // Random prime
	x ^= x >> 32;
	return x;
}

uint64_t rand_dist(rand_distribution* dist) {
	uint64_t low, high;
	uint64_t range_num;

	if (dist->type == DIST_UNIFORM)
		return rand_uint64() % dist->max;

	// Generate Zipf-distributed random
	double x = rand_double() * dist->total_weight;

	// Find which range contains x
	low = 0;
	high = dist->num_zipf_ranges;
	while (1) {
		if (high - low <= 1) {
			range_num = low;
			break;
		}
		uint64_t mid = (low + high) / 2 - 1;
		if (x < dist->zipf_ranges[mid].weight_cumsum) {
			high = mid + 1;
		} else {
			low = mid + 1;
		}
	}

	// This range contains x. Choose a random value in the range.
	zipf_range* range = &(dist->zipf_ranges[range_num]);
	uint64_t zipf_rand = (rand_uint64() % range->size) + range->start;

	if (dist->type == DIST_ZIPF) {
		// Permute the output. Otherwise, all common values will be near one another
		assert(dist->max > 1000);  // When <max> is small, collisions change the distribution considerably.
		return mix(zipf_rand) % dist->max;
	} else if(dist->type == DIST_ZIPF_RANK) {
		assert(dist->type == DIST_ZIPF_RANK);
		return zipf_rand;
	}
	else if(dist->type == DIST_PIM) {
		uint64_t rank_idx = mix(zipf_rand) % dist->max;
		uint64_t rank_size = dist->pim_idx_max / dist->max;
		uint64_t pim_rand_res = rank_size * rank_idx + (rand_uint64() % rank_size);
		return pim_rand_res;
	}
}