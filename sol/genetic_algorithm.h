#ifndef GENETIC_ALGORITHM_H
#define GENETIC_ALGORITHM_H

#include "sack_object.h"
#include "individual.h"
#include <pthread.h>

// typedef int pthread_barrierattr_t;
// typedef struct
// {
//     pthread_mutex_t mutex;
//     pthread_cond_t cond;
//     int count;
//     int tripCount;
// } pthread_barrier_t;

typedef struct _thread_arg {
    int id;
    int threads;
    int object_count, generations_count, sack_capacity;
    const sack_object *objects;
    individual *current_generation, *next_generation;
    pthread_barrier_t* barrier;
} thread_arg;

// int pthread_barrier_wait(pthread_barrier_t *barrier);
// int pthread_barrier_destroy(pthread_barrier_t *barrier);
// int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count);

// reads input from a given file
int read_input(sack_object **objects, int *object_count, int *sack_capacity, int *generations_count, int *numberOfThreads, int argc, char *argv[]);

// displays all the objects that can be placed in the sack
void print_objects(const sack_object *objects, int object_count);

// displays all or a part of the individuals in a generation
void print_generation(const individual *generation, int limit);

// displays the individual with the best fitness in a generation
void print_best_fitness(const individual *generation);

// computes the fitness function for each individual in a generation
void compute_fitness_function(const sack_object *objects, individual *generation, int start, int stop, int sack_capacity);

// compares two individuals by fitness and then number of objects in the sack (to be used with qsort)
int cmpfunc(const void *a, const void *b);

// performs a variant of bit string mutation
void mutate_bit_string_1(const individual *ind, int generation_index);

// performs a different variant of bit string mutation
void mutate_bit_string_2(const individual *ind, int generation_index);

// performs one-point crossover
void crossover(individual *parent1, individual *child1, int generation_index);

// copies one individual
void copy_individual(const individual *from, const individual *to);

// deallocates a generation
void free_generation(individual *generation);

// runs the genetic algorithm
void run_genetic_algorithm(const sack_object *objects, int object_count, int generations_count, int sack_capacity, int numberOfThreads);

// thread function to run genetic algorithm
void *computeSolution(void *arg);

individual *merge(individual *source, int start, int mid, int end);

#endif