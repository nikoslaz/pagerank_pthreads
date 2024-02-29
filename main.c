#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> 
 
#define N 367667
#define DAMPING_FACTOR 0.85
#define INITIAL_RANK 1.0
#define MAX_ITERATIONS 50
#define NUM_THREADS 2


pthread_t threads[NUM_THREADS]; 
pthread_mutex_t mutex;
pthread_barrier_t barrier;

struct Graph
{
    struct Node* head[N];
    int numNodes;
    int from;
    int to;
};
 
struct Node
{
    long dest;
    double pagerank;
    struct Node* next;
};
 
struct Edge {
    long src, dest;
};

struct Graph* createGraph(struct Edge edges[], int n)
{
    struct Graph* graph = (struct Graph*)malloc(sizeof(struct Graph));
 
    for (int i = 0; i < N; i++) {
        graph->head[i] = NULL;
        graph->from = 0;
        graph->to = 0;
        graph->numNodes = 0;
    }
 
    for (int i = 0; i < n; i++)
    {

        long src = edges[i].src;
        long dest = edges[i].dest;
 
        struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
        graph->numNodes++;
        newNode->dest = dest;
        newNode->pagerank = 1.0;
        newNode->next = graph->head[src]; 

        graph->head[src] = newNode;
    }
 
    return graph;
}
 
void printGraph(struct Graph* graph)
{
    for (int i = 0; i < N; i++)
    {
        struct Node* ptr = graph->head[i];
        while (ptr != NULL)
        {
            printf("(%d —> %ld)\t", i, ptr->dest);
            ptr = ptr->next;
        }
 
        printf("\n");
    }
}

void *PageRank(void* arg){
    double* pageRank = (double*)malloc(N * sizeof(double));
    struct Graph* graph = (struct Graph*) arg;
    int n = graph->numNodes;

    if(pageRank == NULL){
        printf("Error: Memory allocation failed\n");
        return NULL;
    }

    for(int i = 0; i < n; i++){
        pageRank[i] = INITIAL_RANK;
    }

    pthread_mutex_lock(&mutex);
    for(int iter = graph->from; iter < graph->to; iter++){
        double* newPageRank = (double*)malloc(N * sizeof(double));
        if(newPageRank == NULL){
            printf("Error: Memory allocation failed\n");
            return NULL;
        }

        for(int i = 0; i < n; i++){
            double sum = 0;

            struct Node* current = graph->head[i];
            while(current != NULL){
                long j = current->dest;

                int out_degree = 0;
                struct Node* out = graph->head[j];

                while(out != NULL){
                    out_degree++;
                    sum += out->pagerank;
                    out = out->next;
                }

                current = current->next;
            }

            newPageRank[i] = (1 - DAMPING_FACTOR) + DAMPING_FACTOR * sum;
        }

        pageRank = newPageRank;
        free(newPageRank);
    }
    pthread_mutex_unlock(&mutex);

    return (void*) pageRank;
}


int main(){
    double* pagerank;
    int numEdges = 0;
    int capacity = 100;
    char *line = malloc(100 * sizeof(char));
    FILE *file = fopen("Email-Enron.txt", "r");
    if(file == NULL){
        printf("Error: File not found\n");
        return 1;
    }
    
    struct Edge* edges = (struct Edge*)malloc(700000 * sizeof(struct Edge)); 
    if(edges == NULL){
        printf("Error: Memory allocation failed\n");
        return 1;
    }

    pthread_mutex_init(&mutex, NULL);
    pthread_barrier_init(&barrier, NULL, NUM_THREADS);

    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == '#'){
            continue;
        }

        if  (sscanf(line, "%ld %ld", &edges[numEdges].src, &edges[numEdges].dest) == 2){
            numEdges++;
        }
    }

    struct Graph* graph = createGraph(edges, numEdges);
    int iterationsPerThread = MAX_ITERATIONS / NUM_THREADS; 

    for (int i = 0; i < NUM_THREADS; i++) {
        graph->from = i * iterationsPerThread;
        graph->to = (i == NUM_THREADS - 1) ? MAX_ITERATIONS : (i + 1) * iterationsPerThread; 
        pthread_create(&threads[i], NULL, &PageRank, (void*)graph);
    }

    for(int i = 0; i < NUM_THREADS; i++){
        pthread_join(threads[i], (void**)&pagerank);
    }

    FILE* output_file = fopen("pagerank.csv", "w");
    if (output_file == NULL) {
        fprintf(stderr, "Error opening output file\n");
        exit(1);
    }
    fprintf(output_file, "node,pagerank\n");
    for (int i = 0; i < graph->numNodes; i++) {
        fprintf(output_file, "%d,%f\n", i, pagerank[i]);
    }
    fclose(output_file);

    pthread_mutex_destroy(&mutex);
    pthread_barrier_destroy(&barrier);
    free(line);
    free(edges);
    fclose(file);

    return 0;
}