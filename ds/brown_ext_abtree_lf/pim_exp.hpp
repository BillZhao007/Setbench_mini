#pragma once

#include <sys/mman.h>
#include <sys/stat.h>
#include <string>
#include <cstdlib>
#include <fcntl.h>

using namespace std;

enum operation_t {
    empty_t,
    get_t,
    update_t,
    predecessor_t,
    scan_t,
    insert_t,
    remove_t
};
const int OPERATION_NR_ITEMS = 7;

int op_count[OPERATION_NR_ITEMS];
struct get_operation {
    int64_t key;
};

struct update_operation {
    int64_t key;
    int64_t value;
};

struct predecessor_operation {
    int64_t key;
};

int scan_start = 0;
struct scan_operation {
    int64_t lkey;
    int64_t rkey;
};

struct insert_operation {
    int64_t key;
    int64_t value;
};

struct remove_operation {
    int64_t key;
};

struct operation {
    union {
        get_operation g;
        update_operation u;
        predecessor_operation p;
        scan_operation s;
        insert_operation i;
        remove_operation r;
    } tsk;
    operation_t type;
};

struct ops_array {
    int n;
    operation* operation_map;
};

ops_array read_op_file(string name) {
    const char* filepath = name.c_str();

    int fd = open(filepath, O_RDONLY, (mode_t)0600);

    if (fd == -1) {
        perror("Error opening file for writing");
        exit(EXIT_FAILURE);
    }

    struct stat fileInfo;

    if (fstat(fd, &fileInfo) == -1) {
        perror("Error getting the file size");
        exit(EXIT_FAILURE);
    }

    if (fileInfo.st_size == 0) {
        fprintf(stderr, "Error: File is empty, nothing to do\n");
        exit(EXIT_FAILURE);
    }

    printf("File size is %ji\n", (intmax_t)fileInfo.st_size);

    void* map = mmap(0, fileInfo.st_size, PROT_READ, MAP_SHARED, fd, 0);

    if (map == MAP_FAILED) {
        close(fd);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }

    cout << fileInfo.st_size << ' ' << sizeof(operation) << endl;

    assert(fileInfo.st_size % sizeof(operation) == 0);

    ops_array operation_map;
    operation_map.n = fileInfo.st_size / sizeof(operation);
    operation_map.operation_map = (operation*)map;

    return operation_map;
}

struct i64_array {
    int n;
    int64_t* i64_map;
};

i64_array read_i64_file(string name) {
    const char* filepath = name.c_str();

    int fd = open(filepath, O_RDONLY, (mode_t)0600);

    if (fd == -1) {
        perror("Error opening file for writing");
        exit(EXIT_FAILURE);
    }

    struct stat fileInfo;

    if (fstat(fd, &fileInfo) == -1) {
        perror("Error getting the file size");
        exit(EXIT_FAILURE);
    }

    if (fileInfo.st_size == 0) {
        fprintf(stderr, "Error: File is empty, nothing to do\n");
        exit(EXIT_FAILURE);
    }

    printf("File size is %ji\n", (intmax_t)fileInfo.st_size);

    void* map = mmap(0, fileInfo.st_size, PROT_READ, MAP_SHARED, fd, 0);

    if (map == MAP_FAILED) {
        close(fd);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }

    cout << fileInfo.st_size << ' ' << sizeof(int64_t) << endl;

    assert(fileInfo.st_size % sizeof(int64_t) == 0);

    i64_array operation_map;
    operation_map.n = fileInfo.st_size / sizeof(int64_t);
    operation_map.i64_map = (int64_t*)map;

    return operation_map;
}