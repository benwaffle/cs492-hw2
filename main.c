#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <time.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
    bool valid:1;
    struct timespec last_access;
} pt_entry;

pt_entry *pt;

typedef struct {
    int pid;
    int start_pt;
    int end_pt;
} process;

process *processes;


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

    for (int i = 0; i < n_processes; ++i) {
        printf("pid = %d has pages %d..%d\n",
                processes[i].pid,
                processes[i].start_pt,
                processes[i].end_pt-1);
    }

    fclose(plist);

    // allocate initial memory
    for (int i = 0; i < n_processes; ++i) {
        printf("alloc for proc %d\n", i);
        int pages_per_proc = 512 / pagesize / n_processes;
        int start_pt = processes[i].start_pt;
        int end_pt = MIN(processes[i].end_pt, start_pt + pages_per_proc);
        for (int j = start_pt; j < end_pt; ++j) {
            pt[j].valid = true;
            printf("\tloading page %d\n", j);
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
        printf("p%d needs mem[%d], page = %d (should be in %d..%d)\n", pid, memloc, global_page, start_pt, end_pt);
        assert(start_pt <= global_page && global_page < end_pt);

        if (!pt[global_page].valid) {
            swap_count++;

            //assert(pt[global_page].start_pt <= page_swapped && page_swapped < pt[global_page].end_pt);
        }
    }

    fclose(ptrace);

    printf("Swap count: %d", swap_count);
}
