#include "cachelab.h"
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define NUMBITS 64 // number of address bits

typedef struct cache_line {
    long long tag;
    int accessTime;
    bool valid;
} cache_line;

// E-way associative cache with S sets
static int S, E;

// s index bits, b bits block offset in 64-bit address
static int s, b;
static bool vFlag = 0; // verbose flag

// Keep track the performance of the cache
static int hits, misses, evictions;

void printUsageInfo(char *argv[]) {
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h\t      Print this help message.\n");
    printf("  -v\t      Optional verbose flag.\n");
    printf("  -s <num>    Number of set index bits.\n");
    printf("  -E <num>    Number of lines per set.\n");
    printf("  -b <num>    Number of block offset bits.\n");
    printf("  -t <file> Trace file.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  linux> %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux> %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
}

cache_line *initCache() {
    // Allocate 1-D array to be used as 2-D array
    cache_line *cache = (cache_line *) malloc(S * E * sizeof(cache_line));

    for (int i = 0; i < S; i++) {
        for (int j = 0; j < E; j++) {
            cache_line line;
            line.valid = 0;
            cache[i * E + j] = line;
        }
    }
    return cache;
}

void accessCache(cache_line *cache, long long address, int *time) {
    // Extract value of index bits
    int index = (address >> b) & ((1 << s) - 1);
    int setIndex = index * E;
    
    // Extract value of tag bits;
    long long tag = address >> (b + s);

    int loadTime = *time;
    int lru = loadTime; // Time of least recently used cache line

    int emptySlot = -1; 
    int lruPos = -1;

    for (int i = 0; i < E; i++) {
        if (cache[setIndex + i].valid && cache[setIndex + i].tag == tag) {
            cache[setIndex + i].accessTime = loadTime;
            hits++;
            if (vFlag) printf("hit ");
            return;
        } else if (!cache[setIndex + i].valid) {
            emptySlot = i;
        } else if (cache[setIndex + i].accessTime < lru) {
            lru = cache[setIndex + i].accessTime;
            lruPos = i;
        }
    }
    
    // Cold miss
    if (emptySlot != -1) {
        cache[setIndex + emptySlot].valid = 1;
        cache[setIndex + emptySlot].tag = tag;
        cache[setIndex + emptySlot].accessTime = loadTime;
        misses++;
        if (vFlag) printf("miss ");
        return;
    } else {
        // Miss and eviction
        cache[setIndex + lruPos].tag = tag;
        cache[setIndex + lruPos].accessTime = loadTime;
        misses++;
        evictions++;
        if (vFlag) printf("miss eviction ");
        return;
    }
}

int main(int argc, char *argv[]) {

    int opt;
    char *traceFile;
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
            case 'h':
                printUsageInfo(argv);
                exit(0);
            case 'v':
                vFlag = 1;
                break;
            case 's':
                s = atoi(optarg);
                S = 1 << s;
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                traceFile = optarg;
                break;
            default:
                printUsageInfo(argv);
                exit(1);
        }
    }

    hits = misses = evictions = 0;
    cache_line *cache = initCache();

    FILE *filePtr;
    filePtr = fopen(traceFile, "r");

    char identifier;
    long long address;
    int size, time;
    time = 0;

    while (fscanf(filePtr, " %c %llx,%d", &identifier, &address, &size) > 0) {

        if (identifier == 'I') continue;
        printf(" %c %llx,%d ", identifier, address, size);

        if (identifier == 'L') {
            accessCache(cache, address, &time);
        } else if (identifier == 'S') {
            accessCache(cache, address, &time);
        } else if (identifier == 'M') {
            accessCache(cache, address, &time);
            accessCache(cache, address, &time);
        }
        time++;
        printf("\n");
    }

    free(cache);
    fclose(filePtr);

    printSummary(hits, misses, evictions);
    return 0;
}