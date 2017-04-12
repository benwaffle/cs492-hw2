#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
    bool valid:1;
    struct timespec data;
} pt_entry;

pt_entry *pt;

typedef struct {
    int pid;
    int start_pt;
    int end_pt;
} process;

process *processes;

typedef enum {
    CLOCK,
    FIFO,
    LRU
} REPL_ALG;

int cmp_timespec(struct timespec a, struct timespec b) {
    if (a.tv_sec < b.tv_sec)
        return -1;
    else if (a.tv_sec > b.tv_sec)
        return 1;
    else if (a.tv_nsec < b.tv_nsec)
        return -1;
    else if (a.tv_nsec > b.tv_nsec)
        return 1;
    else
        return 1;
}

void fifo_lru_evict(int pid, int page) {
    int start_pt = processes[pid].start_pt;
    int end_pt = processes[pid].end_pt;
    assert(start_pt <= page && page < end_pt);
    int to_swap = -1;
    // first valid page
    for (int i = start_pt; i < end_pt; ++i)
        if (pt[i].valid) {
            to_swap = i;
            break;
        }
    assert(to_swap != -1);

    // find first page inserted
    for (int i = start_pt; i < end_pt; ++i) {
        if (pt[i].valid && cmp_timespec(pt[to_swap].data, pt[i].data) > 0)
            to_swap = i;
    }

    assert(start_pt <= to_swap && to_swap < end_pt);

    pt[to_swap].valid = false; // remove old page
    pt[page].valid = true; // insert new page
    // set 'in' time for new page
    if (clock_gettime(CLOCK_MONOTONIC, &pt[page].data) == -1) {
        perror("clock_gettime");
        exit(1);
    }
#ifndef NDEBUG
    printf("[%d] replacing page %d for %d\n", pid, to_swap, page);
#endif
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage %s\n"
                        "\t<plist file>\n"
                        "\t<ptrace file>\n"
                        "\t<page size>\n"
                        "\t<replacement algorithm>\n"
                        "\t<prepaging +/->\n",
                    argv[0]);
        return 1;
    }

    int pagesize = atoi(argv[3]);

    bool prepaging = (strncmp(argv[5], "+", 1) == 0);

    REPL_ALG alg;
    if (strncmp(argv[4], "LRU", 3) == 0)
        alg = LRU;
    else if (strncmp(argv[4], "CLOCK", 5) == 0)
        alg = CLOCK;
    else if (strncmp(argv[4], "FIFO", 4) == 0)
        alg = FIFO;
    else {
        fprintf(stderr, "Unknown replacement algorithm %s\n", argv[4]);
        return 1;
    }

    FILE *plist = fopen(argv[1], "r");
    // total number of pages all processes need
    int ptsize = 0, n_processes = 0, ret, pid, memlocs;
    while ((ret = fscanf(plist, "%d %d\n", &pid, &memlocs)) != EOF) {
        // the number of pages this process needs
        int num_pages = (int)ceil((double)memlocs/pagesize);
        ptsize += num_pages;
        n_processes++;
    }

    pt = calloc(ptsize, sizeof(pt_entry));
    processes = calloc(n_processes, sizeof(process));

    rewind(plist);

    // our incrementing counter, because we can't guarantee that PID in the file
    // is 0 to n
    int proc_cnt = 0;
    int page_cnt = 0;
    while ((ret = fscanf(plist, "%d %d\n", &pid, &memlocs)) != EOF) {
        int num_pages = (int)ceil((double)memlocs/pagesize);
        processes[proc_cnt] = (process) {
            .pid = pid,
            .start_pt = page_cnt,
            .end_pt = page_cnt + num_pages
        };
        ++proc_cnt;
        page_cnt += num_pages;
    }

#ifndef NDEBUG
    for (int i = 0; i < n_processes; ++i) {
        printf("pid = %d has pages %d..%d\n",
                processes[i].pid,
                processes[i].start_pt,
                processes[i].end_pt-1);
    }
#endif

    fclose(plist);

    // allocate initial memory
    for (int i = 0; i < n_processes; ++i) {
#ifndef NDEBUG
        printf("alloc for proc %d\n", i);
#endif
        int pages_per_proc = 512 / pagesize / n_processes;
        int start_pt = processes[i].start_pt;
        int end_pt = MIN(processes[i].end_pt, start_pt + pages_per_proc);
        for (int j = start_pt; j < end_pt; ++j) {
            pt[j].valid = true;
            if (alg == FIFO) {
                // for FIFO, set the 'in' time
                if (clock_gettime(CLOCK_MONOTONIC, &pt[j].data) == -1) {
                    perror("clock_gettime");
                    return 1;
                }
            }
            // for LRU, calloc already sets .tv_sec and .tv_nsec to 0
#ifndef NDEBUG
            printf("\tloading page %d\n", j);
#endif
        }
    }

    { // checks
        int bytes_in_mem = 0;
        for (int i = 0; i < ptsize; ++i)
            if (pt[i].valid)
                bytes_in_mem += pagesize;
        assert(bytes_in_mem <= 512);
    }

    FILE *ptrace = fopen(argv[2], "r");

    int swap_count = 0;

    // start "running" programs
    int memloc;
    while((ret = fscanf(ptrace, "%d %d", &pid, &memloc)) != EOF) {
        memloc--; // memory locations in the file start at 1
        int start_pt = processes[pid].start_pt;
        int end_pt = processes[pid].end_pt;
        int global_page = start_pt + (memloc/pagesize);
#ifndef NDEBUG
        printf("p%d needs mem[%d], page = %d (should be in %d..%d)\n", pid, memloc, global_page, start_pt, end_pt);
#endif
        assert(start_pt <= global_page && global_page < end_pt);

        if (!pt[global_page].valid) {
            swap_count++;

            if (alg == FIFO || alg == LRU)
                fifo_lru_evict(pid, global_page);
                if (prepaging) {
                  for (int i = global_page+1; i < end_pt; i++) {
            // load next page
                    if (!pt[i].valid) {
                      fifo_lru_evict(pid, i);
                      break;
                    }
                  }
                }
        } else {
            if (alg == LRU) // for LRU, set the time when used
                if (clock_gettime(CLOCK_MONOTONIC, &pt[global_page].data) == -1) {
                    perror("clock_gettime");
                    return 1;
                }
        }
    }

    fclose(ptrace);

    free(pt);
    free(processes);

    printf("Swap count: %d\n", swap_count);
}
