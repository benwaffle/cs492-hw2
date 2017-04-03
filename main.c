#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

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

    for (int i=0; i<n_processes; ++i) {
        printf("pid = %d has pages %d..%d\n",
                processes[i].pid,
                processes[i].start_pt,
                processes[i].end_pt-1);
    }

    fclose(plist);

    FILE *ptrace = fopen(argv[2], "r");

    fclose(ptrace);
}
