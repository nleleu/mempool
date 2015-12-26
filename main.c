#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define CHUNK_256_SIZE 256
#define CHUNK_512_SIZE 512
#define CHUNK_1024_SIZE 1024
#define CHUNK_2048_SIZE 2048

#define CHUNK_256_NUMBER 100
#define CHUNK_512_NUMBER 100
#define CHUNK_1024_NUMBER 100
#define CHUNK_2048_NUMBER 100

typedef enum
{
    CHUNK_256,
    CHUNK_512,
    CHUNK_1024,
    CHUNK_2048,
    CHUNK_MAX,
}mempool_chunk_type_t;

typedef struct chunk_info
{
    uint32_t chunk_number;
    uint32_t chunk_size;
}chunk_info_t;

static chunk_info_t chunk_info[CHUNK_MAX] = {
[CHUNK_256] = {CHUNK_256_NUMBER, CHUNK_256_SIZE},
[CHUNK_512] = {CHUNK_512_NUMBER, CHUNK_512_SIZE},
[CHUNK_1024] = {CHUNK_1024_NUMBER, CHUNK_1024_SIZE},
[CHUNK_2048] = {CHUNK_2048_NUMBER, CHUNK_2048_SIZE},
};

typedef enum
{
    AVAILABLE,
    USED
}mempool_chunk_status_t;



typedef struct mempool_stat
{
    uint32_t alloc_nb;
    uint32_t free_nb;
    uint32_t alloc_size;
    uint32_t free_size;
    float avg_load;
}mempool_stat_t;

typedef struct mempool_chunk
{
    uint8_t* data;
    size_t data_size;
    size_t max_size;
    mempool_stat_t stats;
    mempool_chunk_status_t status;
    mempool_chunk_type_t type;
}mempool_chunk_t;

typedef struct mempool_chunk_array
{
    mempool_chunk_t* array;
    mempool_stat_t stats;
}mempool_chunk_array_t;


typedef struct mempool
{
    mempool_chunk_array_t chunks[CHUNK_MAX];

    mempool_chunk_t** available_chunks[CHUNK_MAX];

    uint32_t available_chunks_it[CHUNK_MAX];
    mempool_stat_t stats;
}mempool_t;


void mempool_init(mempool_t* mempool)
{
    memset(mempool, 0, sizeof(mempool_t));
    uint32_t i;
    uint32_t chunk_type;
    for (chunk_type = CHUNK_256; chunk_type < CHUNK_MAX; chunk_type++)
    {
        uint32_t chunk_number = chunk_info[chunk_type].chunk_number;
        uint32_t chunk_size = chunk_info[chunk_type].chunk_size;
        mempool->chunks[chunk_type].array = (mempool_chunk_t*)malloc(chunk_number * sizeof(mempool_chunk_t));
        memset(mempool->chunks[chunk_type].array, 0, chunk_number * sizeof(mempool_chunk_t));
        mempool->available_chunks[chunk_type] = (mempool_chunk_t**)malloc(chunk_number * sizeof(mempool_chunk_t*));
        for (i = 0; i < chunk_number; i++)
        {
            mempool_chunk_t* chunk = mempool->chunks[chunk_type].array + i;
            chunk->data = (uint8_t*)malloc(chunk_number * chunk_size * sizeof(uint8_t));
            chunk->max_size = chunk_size;
            chunk->type = chunk_type;
            mempool->available_chunks[chunk_type][i] = chunk;
        }

    }

}

void print_stats(mempool_stat_t* stats)
{
    printf("ALLOC NB    : %d\n", stats->alloc_nb);
    printf("FREE NB     : %d\n", stats->free_nb);
    printf("ALLOC SIZE  : %d\n", stats->alloc_size);
    printf("FREE SIZE   : %d\n", stats->free_size);
    printf("AVG LOAD    : %f\n", stats->avg_load);
}

float compute_avg_load(size_t size, size_t max_size, float load, uint32_t nb_load)
{
    return (load * nb_load + size / (float)max_size) / ((float)nb_load + 1);
}

mempool_chunk_t* mempool_get(mempool_t* mempool, size_t size)
{
    mempool_chunk_t* res = NULL;
    mempool_stat_t* chunk_type_stats = NULL;

    uint32_t chunk_type;
    for (chunk_type = CHUNK_256; chunk_type < CHUNK_MAX; chunk_type++)
    {
        uint32_t chunk_number = chunk_info[chunk_type].chunk_number;
        uint32_t chunk_size = chunk_info[chunk_type].chunk_size;
        if (size < chunk_size)
        {
            res = mempool->available_chunks[chunk_type][mempool->available_chunks_it[chunk_type]++];
            chunk_type_stats = &mempool->chunks[chunk_type].stats;
            printf("New chunk %d\n", chunk_size);
            break;
        }
    }

    if (res && res->status == AVAILABLE)
    {
        res->data_size = size;
        res->status = USED;

        chunk_type_stats->avg_load = compute_avg_load(res->data_size, res->max_size,
                                                      chunk_type_stats->avg_load, chunk_type_stats->alloc_nb);
        chunk_type_stats->alloc_nb++;
        chunk_type_stats->alloc_size += size;
        print_stats(chunk_type_stats);

        res->stats.avg_load = compute_avg_load(res->data_size, res->max_size,
                                                             res->stats.avg_load, res->stats.alloc_nb);
        res->stats.alloc_nb++;
        res->stats.alloc_size += size;

        print_stats(&res->stats);

        mempool->stats.avg_load = compute_avg_load(res->data_size, res->max_size,
                                                   mempool->stats.avg_load, mempool->stats.alloc_nb);
        mempool->stats.alloc_nb++;
        mempool->stats.alloc_size += size;

        print_stats(&mempool->stats);
    }
    else
    {
        fprintf(stderr, "Invalid chunk\n");
    }
    return res;
}

void mempool_free(mempool_t* mempool, mempool_chunk_t* chunk)
{
    mempool_stat_t* chunk_type_stats = &mempool->chunks[chunk->type].stats;
    chunk_type_stats->free_nb++;
    chunk_type_stats->free_size += chunk->data_size;

    chunk->status = AVAILABLE;
    chunk->stats.free_nb++;
    chunk->stats.free_size += chunk->data_size;

    mempool->stats.free_nb++;
    mempool->stats.free_size += chunk->data_size;

    chunk->data_size = 0;

    uint32_t pos = --mempool->available_chunks_it[chunk->type];
    mempool->available_chunks[chunk->type][pos] = chunk;

}

int main()
{
    mempool_t test;
    mempool_init(&test);
    mempool_chunk_t* chunk = mempool_get(&test, 56);
    mempool_free(&test, chunk);
    chunk = mempool_get(&test, 1024);
    mempool_free(&test, chunk);
    print_stats(&test.stats);

    return 0;
}
