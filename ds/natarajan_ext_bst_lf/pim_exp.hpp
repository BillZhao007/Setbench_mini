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

inline void write_ops_to_file(string file_name, operation* ops, int n) {
    printf("Will write to '%s'\n", file_name.c_str());

    /* Open a file for writing.
     *  - Creating the file if it doesn't exist.
     *  - Truncating it to 0 size if it already exists. (not really needed)
     *
     * Note: "O_WRONLY" mode is not sufficient when mmaping.
     */

    const char* filepath = file_name.c_str();

    int try_to_unlink = unlink(filepath);

    if (try_to_unlink == 0) {
        cout << "Remove: " << filepath << endl;
    }

    int fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);

    if (fd == -1) {
        perror("Error opening file for writing");
        exit(EXIT_FAILURE);
    }

    // Stretch the file size to the size of the (mmapped) array of char

    size_t filesize = sizeof(operation) * n;

    if (lseek(fd, filesize - sizeof(operation), SEEK_SET) == -1) {
        close(fd);
        perror("Error calling lseek() to 'stretch' the file");
        exit(EXIT_FAILURE);
    }

    operation empty_op;
    empty_op.type = operation_t::empty_t;

    if (write(fd, &empty_op, sizeof(operation)) == -1) {
        close(fd);
        perror("Error writing last byte of the file");
        exit(EXIT_FAILURE);
    }

    // Now the file is ready to be mmapped.
    void* map = mmap(0, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }

    operation* fops = (operation*)map;

    for(size_t i = 0; i < n; i++) {
        fops[i] = ops[i];
    }

    cout << "task generation finished" << endl;

    // Write it now to disk
    if (msync(map, filesize, MS_SYNC) == -1) {
        perror("Could not sync the file to disk");
    }

    // Don't forget to free the mmapped memory
    if (munmap(map, filesize) == -1) {
        close(fd);
        perror("Error un-mmapping the file");
        exit(EXIT_FAILURE);
    }

    // Un-mmaping doesn't close the file, so we still need to do that.
    close(fd);
}