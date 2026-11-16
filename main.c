#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define DAMPING_FACTOR 0.85
#define INITIAL_RANK 1.0
#define MAX_ITERATIONS 50
#define DEFAULT_THREADS 2

/*
 * Graph in compressed (CSR) form, stored by *incoming* edges:
 * the nodes that link to node v are in_edges[in_start[v] .. in_start[v + 1] - 1].
 */
struct Graph {
    long numNodes;
    long numEdges;
    long* in_start;
    long* in_edges;
    long* out_degree;
};

struct Edge {
    long src, dest;
};

/*
 * Simple reusable barrier (pthread_barrier_t is not available on macOS).
 */
struct Barrier {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int waiting;
    unsigned long generation;
};

static void barrier_init(struct Barrier* b, int count) {
    pthread_mutex_init(&b->mutex, NULL);
    pthread_cond_init(&b->cond, NULL);
    b->count = count;
    b->waiting = 0;
    b->generation = 0;
}

static void barrier_wait(struct Barrier* b) {
    pthread_mutex_lock(&b->mutex);
    unsigned long gen = b->generation;
    if (++b->waiting == b->count) {
        b->waiting = 0;
        b->generation++;
        pthread_cond_broadcast(&b->cond);
    } else {
        while (gen == b->generation) {
            pthread_cond_wait(&b->cond, &b->mutex);
        }
    }
    pthread_mutex_unlock(&b->mutex);
}

static void barrier_destroy(struct Barrier* b) {
    pthread_mutex_destroy(&b->mutex);
    pthread_cond_destroy(&b->cond);
}

/* Data shared by all threads */
struct Shared {
    struct Graph* graph;
    double* rank[2];          /* rank[iter % 2] is read, rank[(iter + 1) % 2] is written */
    struct Barrier barrier;
};

/* Each thread computes the ranks of the nodes in [from, to) */
struct ThreadArgs {
    struct Shared* shared;
    long from;
    long to;
};

/*
 * Read the edge list ("src dest" per line, lines starting with '#' are comments).
 */
struct Edge* read_edges(const char* filename, long* numEdges, long* numNodes) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        return NULL;
    }

    long capacity = 1024;
    long count = 0;
    long maxNode = -1;
    struct Edge* edges = malloc(capacity * sizeof(struct Edge));
    char line[256];

    while (edges != NULL && fgets(line, sizeof(line), file) != NULL) {
        long src, dest;
        if (line[0] == '#' || sscanf(line, "%ld %ld", &src, &dest) != 2 || src < 0 || dest < 0) {
            continue;
        }
        if (count == capacity) {
            capacity *= 2;
            struct Edge* bigger = realloc(edges, capacity * sizeof(struct Edge));
            if (bigger == NULL) {
                free(edges);
                edges = NULL;
                break;
            }
            edges = bigger;
        }
        edges[count].src = src;
        edges[count].dest = dest;
        count++;
        if (src > maxNode) maxNode = src;
        if (dest > maxNode) maxNode = dest;
    }

    fclose(file);
    *numEdges = count;
    *numNodes = maxNode + 1;
    return edges;
}

struct Graph* createGraph(struct Edge edges[], long numEdges, long numNodes) {
    struct Graph* graph = malloc(sizeof(struct Graph));
    if (graph == NULL) {
        return NULL;
    }
    graph->numNodes = numNodes;
    graph->numEdges = numEdges;
    graph->in_start = calloc(numNodes + 1, sizeof(long));
    graph->in_edges = malloc((numEdges > 0 ? numEdges : 1) * sizeof(long));
    graph->out_degree = calloc(numNodes > 0 ? numNodes : 1, sizeof(long));
    long* next = malloc((numNodes > 0 ? numNodes : 1) * sizeof(long));
    if (!graph->in_start || !graph->in_edges || !graph->out_degree || !next) {
        free(next);
        return NULL;
    }

    /* Count incoming edges per node and outgoing edges per node */
    for (long i = 0; i < numEdges; i++) {
        graph->in_start[edges[i].dest + 1]++;
        graph->out_degree[edges[i].src]++;
    }
    for (long v = 0; v < numNodes; v++) {
        graph->in_start[v + 1] += graph->in_start[v];
        next[v] = graph->in_start[v];
    }
    for (long i = 0; i < numEdges; i++) {
        graph->in_edges[next[edges[i].dest]++] = edges[i].src;
    }

    free(next);
    return graph;
}

void freeGraph(struct Graph* graph) {
    if (graph == NULL) return;
    free(graph->in_start);
    free(graph->in_edges);
    free(graph->out_degree);
    free(graph);
}

/*
 * PR(v) = (1 - d) + d * sum(PR(u) / outdegree(u)) over all u that link to v.
 * All threads do every iteration, each for its own range of nodes,
 * and wait at the barrier before the next iteration starts.
 */
void* PageRank(void* arg) {
    struct ThreadArgs* args = (struct ThreadArgs*)arg;
    struct Shared* shared = args->shared;
    struct Graph* graph = shared->graph;

    for (int iter = 0; iter < MAX_ITERATIONS; iter++) {
        const double* oldRank = shared->rank[iter % 2];
        double* newRank = shared->rank[(iter + 1) % 2];

        for (long v = args->from; v < args->to; v++) {
            double sum = 0;
            for (long k = graph->in_start[v]; k < graph->in_start[v + 1]; k++) {
                long u = graph->in_edges[k];
                sum += oldRank[u] / graph->out_degree[u];
            }
            newRank[v] = (1 - DAMPING_FACTOR) + DAMPING_FACTOR * sum;
        }

        barrier_wait(&shared->barrier);
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    const char* filename = argc > 1 ? argv[1] : "Email-Enron.txt";
    int numThreads = argc > 2 ? atoi(argv[2]) : DEFAULT_THREADS;
    const char* outputName = "pagerank.csv";

    if (argc > 3 || numThreads < 1) {
        printf("Usage: %s [edge_list_file] [threads]\n", argv[0]);
        return 1;
    }

    long numEdges, numNodes;
    struct Edge* edges = read_edges(filename, &numEdges, &numNodes);
    if (edges == NULL) {
        printf("Error: Could not read %s\n", filename);
        return 1;
    }

    struct Graph* graph = createGraph(edges, numEdges, numNodes);
    free(edges);
    if (graph == NULL) {
        printf("Error: Memory allocation failed\n");
        return 1;
    }

    struct Shared shared;
    shared.graph = graph;
    shared.rank[0] = malloc((numNodes > 0 ? numNodes : 1) * sizeof(double));
    shared.rank[1] = malloc((numNodes > 0 ? numNodes : 1) * sizeof(double));
    if (shared.rank[0] == NULL || shared.rank[1] == NULL) {
        printf("Error: Memory allocation failed\n");
        return 1;
    }
    for (long v = 0; v < numNodes; v++) {
        shared.rank[0][v] = INITIAL_RANK;
    }
    if (numThreads > numNodes && numNodes > 0) {
        numThreads = (int)numNodes;
    }
    barrier_init(&shared.barrier, numThreads);

    pthread_t* threads = malloc(numThreads * sizeof(pthread_t));
    struct ThreadArgs* args = malloc(numThreads * sizeof(struct ThreadArgs));
    if (threads == NULL || args == NULL) {
        printf("Error: Memory allocation failed\n");
        return 1;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Split the nodes evenly between the threads */
    long chunk = numNodes / numThreads;
    for (int i = 0; i < numThreads; i++) {
        args[i].shared = &shared;
        args[i].from = i * chunk;
        args[i].to = (i == numThreads - 1) ? numNodes : (i + 1) * chunk;
        pthread_create(&threads[i], NULL, PageRank, &args[i]);
    }
    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("%s: %ld nodes, %ld edges, %d threads, %d iterations: %.3f s\n",
           filename, numNodes, numEdges, numThreads, MAX_ITERATIONS, elapsed);

    /* The final ranks are in the buffer written by the last iteration */
    const double* pagerank = shared.rank[MAX_ITERATIONS % 2];

    FILE* output_file = fopen(outputName, "w");
    if (output_file == NULL) {
        fprintf(stderr, "Error opening output file\n");
        return 1;
    }
    fprintf(output_file, "node,pagerank\n");
    for (long v = 0; v < numNodes; v++) {
        fprintf(output_file, "%ld,%f\n", v, pagerank[v]);
    }
    fclose(output_file);

    barrier_destroy(&shared.barrier);
    free(shared.rank[0]);
    free(shared.rank[1]);
    free(threads);
    free(args);
    freeGraph(graph);

    return 0;
}
