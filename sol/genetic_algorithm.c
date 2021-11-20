#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "genetic_algorithm.h"

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

int read_input(sack_object **objects, int *object_count, int *sack_capacity, int *generations_count, int *numberOfThreads, int argc, char *argv[])
{
	FILE *fp;

	if (argc < 4) {
		fprintf(stderr, "Usage:\n\t./tema1_par in_file generations_count\n");
		return 0;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		return 0;
	}

	if (fscanf(fp, "%d %d", object_count, sack_capacity) < 2) {
		fclose(fp);
		return 0;
	}

	if (*object_count % 10) {
		fclose(fp);
		return 0;
	}

	sack_object *tmp_objects = (sack_object *) calloc(*object_count, sizeof(sack_object));

	for (int i = 0; i < *object_count; ++i) {
		if (fscanf(fp, "%d %d", &tmp_objects[i].profit, &tmp_objects[i].weight) < 2) {
			free(objects);
			fclose(fp);
			return 0;
		}
	}

	fclose(fp);

	*generations_count = (int) strtol(argv[2], NULL, 10);
	*numberOfThreads = (int) atoi(argv[3]);
	
	if (*generations_count == 0 || *numberOfThreads == 0) {
		free(tmp_objects);

		return 0;
	}

	*objects = tmp_objects;

	return 1;
}

void print_objects(const sack_object *objects, int object_count)
{
	for (int i = 0; i < object_count; ++i) {
		printf("%d %d\n", objects[i].weight, objects[i].profit);
	}
}

void print_generation(const individual *generation, int limit)
{
	for (int i = 0; i < limit; ++i) {
		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			printf("%d ", generation[i].chromosomes[j]);
		}

		printf("\n%d - %d\n", i, generation[i].fitness);
	}
}

void print_best_fitness(const individual *generation)
{
	printf("%d\n", generation[0].fitness);
}

void compute_fitness_function(const sack_object *objects, individual *generation, int start, int stop, int sack_capacity)
{
	int weight;
	int profit;

	for (int i = start; i < stop; ++i) {
		weight = 0;
		profit = 0;

		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			if (generation[i].chromosomes[j]) {
				weight += objects[j].weight;
				profit += objects[j].profit;
			}
		}

		generation[i].fitness = (weight <= sack_capacity) ? profit : 0;
	}
}

int cmpfunc(const void *a, const void *b)
{
	int i;
	individual *first = (individual *) a;
	individual *second = (individual *) b;

	int res = second->fitness - first->fitness; // decreasing by fitness
	if (res == 0) {
		int first_count = 0, second_count = 0;

		for (i = 0; i < first->chromosome_length && i < second->chromosome_length; ++i) {
			first_count += first->chromosomes[i];
			second_count += second->chromosomes[i];
		}

		res = first_count - second_count; // increasing by number of objects in the sack
		if (res == 0) {
			return second->index - first->index;
		}
	}

	return res;
}

void mutate_bit_string_1(const individual *ind, int generation_index)
{
	int i, mutation_size;
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	if (ind->index % 2 == 0) {
		// for even-indexed individuals, mutate the first 40% chromosomes by a given step
		mutation_size = ind->chromosome_length * 4 / 10;
		for (i = 0; i < mutation_size; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	} else {
		// for even-indexed individuals, mutate the last 80% chromosomes by a given step
		mutation_size = ind->chromosome_length * 8 / 10;
		for (i = ind->chromosome_length - mutation_size; i < ind->chromosome_length; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	}
}

void mutate_bit_string_2(const individual *ind, int generation_index)
{
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	// mutate all chromosomes by a given step
	for (int i = 0; i < ind->chromosome_length; i += step) {
		ind->chromosomes[i] = 1 - ind->chromosomes[i];
	}
}

void crossover(individual *parent1, individual *child1, int generation_index)
{
	individual *parent2 = parent1 + 1;
	individual *child2 = child1 + 1;
	int count = 1 + generation_index % parent1->chromosome_length;

	memcpy(child1->chromosomes, parent1->chromosomes, count * sizeof(int));
	memcpy(child1->chromosomes + count, parent2->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));

	memcpy(child2->chromosomes, parent2->chromosomes, count * sizeof(int));
	memcpy(child2->chromosomes + count, parent1->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));
}

void copy_individual(const individual *from, const individual *to)
{
	memcpy(to->chromosomes, from->chromosomes, from->chromosome_length * sizeof(int));
}

void free_generation(individual *generation)
{
	int i;

	for (i = 0; i < generation->chromosome_length; ++i) {
		free(generation[i].chromosomes);
		generation[i].chromosomes = NULL;
		generation[i].fitness = 0;
	}
}

void run_genetic_algorithm(const sack_object *objects, int object_count, int generations_count, int sack_capacity, int numberOfThreads)
{
	// same instances for each thread
	individual *current_generation = (individual*) calloc(object_count, sizeof(individual));
	individual *next_generation = (individual*) calloc(object_count, sizeof(individual));
	pthread_t threads[numberOfThreads];
	thread_arg arguments[numberOfThreads];
	void *status;
	int r;
	pthread_barrier_t barrier; // barrier for each thread ( the same instance )

	r = pthread_barrier_init(&barrier, NULL, numberOfThreads);

	if (r) {
		fprintf(stderr, "pthread_barrier_init: %s\n", strerror(r));
		exit(1);
	}

	// initialize each thread arguments
	for (int i = 0; i < numberOfThreads; ++i) {
		arguments[i].id = i;
		arguments[i].threads = numberOfThreads;
		arguments[i].object_count = object_count;
		arguments[i].generations_count = generations_count;
		arguments[i].sack_capacity = sack_capacity;
		arguments[i].objects = objects;
		arguments[i].current_generation = current_generation;
		arguments[i].next_generation = next_generation;
		arguments[i].barrier = &barrier;

		// open thread
		r = pthread_create(&threads[i], NULL, computeSolution, (void *) &arguments[i]);
 
        if (r) {
            printf("Eroare la crearea thread-ului %d\n", i);
            exit(-1);
        }
	}

	// join thread
	// function took from the lab files
	for (int i = 0; i < numberOfThreads; i++) {
        r = pthread_join(threads[i], &status);
 
        if (r) {
            printf("Eroare la asteptarea thread-ului %d\n", i);
            exit(-1);
        }
    }

	r = pthread_barrier_destroy(&barrier);

	if (r < 0) {
		printf("Eroare la dealocarea barierei\n");
		exit(-1);
	}

	// free resources for old generation
	free_generation(current_generation);
	free_generation(next_generation);

	// free resources
	free(current_generation);
	free(next_generation);
}

void *computeSolution(void *arg) {
	thread_arg thread = *(thread_arg *) arg;
	int id = thread.id;
	int N = thread.object_count; // number of objects from list
	int P = thread.threads; // number of threads
	int start = id * (double) N / P;
	int stop = min((id + 1) * (double) N / P, N);
	int cursor, count;
	individual *tmp = NULL;

	// make first generation from each thread for each object
	for (int i = start; i < stop; ++i) {
		thread.current_generation[i].fitness = 0;
		thread.current_generation[i].chromosomes = (int*) calloc(N, sizeof(int));
		thread.current_generation[i].chromosomes[i] = 1;
		thread.current_generation[i].index = i;
		thread.current_generation[i].chromosome_length = N;

		thread.next_generation[i].fitness = 0;
		thread.next_generation[i].chromosomes = (int*) calloc(N, sizeof(int));
		thread.next_generation[i].index = i;
		thread.next_generation[i].chromosome_length = N;
	}

	pthread_barrier_wait(thread.barrier);

	// go generations_count times to compute final generation
	for (int k = 0; k < thread.generations_count; ++k) {
		cursor = 0;
		int startGeneration, stopGeneration; // variables used for computing 30%, 20% for the best generation

		// compute fitness per each thread
		compute_fitness_function(thread.objects, thread.current_generation, start, stop, thread.sack_capacity); 
		pthread_barrier_wait(thread.barrier);

		// qsort per each chunk on each thread
		qsort(thread.current_generation + start, stop - start, sizeof(individual), cmpfunc);
		pthread_barrier_wait(thread.barrier);

		// onyl 1 thread will merge all P chunks, where P is the nubmer of threads
		if (id == 0) {
			for (int i = 1; i <	P; ++i) {
				int beginning = i * (double) N / P;
				int ending = min((i + 1) * (double) N / P, N);

				memcpy(thread.current_generation, merge(thread.current_generation, 0, beginning, ending), ending * sizeof(individual));
			}
		}

		pthread_barrier_wait(thread.barrier);
		// keep first 30% children (elite children selection)
		count = N * 3 / 10;
		startGeneration = id * (double) count / P;
		stopGeneration = min((id + 1) * (double) count / P, count);

		for (int i = startGeneration; i < stopGeneration; ++i) {
			copy_individual(thread.current_generation + i, thread.next_generation + i);
		}

		cursor = count;
		// mutate first 20% children with the first version of bit string mutation
		count = N * 2 / 10;
		// start and stop for each thread
		startGeneration = id * (double) count / P;
		stopGeneration = min((id + 1) * (double) count / P, count);

		for (int i = startGeneration; i < stopGeneration; ++i) {
			copy_individual(thread.current_generation + i, thread.next_generation + i + cursor);
			mutate_bit_string_1(thread.next_generation + cursor + i, k);
		}

		cursor += count;

		// mutate next 20% children with the second version of bit string mutation
		for (int i = startGeneration; i < stopGeneration; ++i) {
			copy_individual(thread.current_generation + i + count, thread.next_generation + i + cursor);
			mutate_bit_string_2(thread.next_generation + cursor + i, k);
		}

		cursor += count;
		pthread_barrier_wait(thread.barrier);

		// crossover first 30% parents with one-point crossover
		// (if there is an odd number of parents, the last one is kept as such)
		count = N * 3 / 10;

		if (count % 2 == 1) {
			if (id == 0) {
				copy_individual(thread.current_generation + N - 1, thread.next_generation + cursor + count - 1);
			}

			count--;
		}

		pthread_barrier_wait(thread.barrier);

		startGeneration = id * (double) count / P;
		stopGeneration = min((id + 1) * (double) count / P, count);

		// if edges are odd, substract 1
		if (startGeneration % 2) {
			startGeneration--;
		}

		if (stopGeneration % 2) {
			stopGeneration--;
		}

		for (int i = startGeneration; i < stopGeneration; i += 2) {
			crossover(thread.current_generation + i, thread.next_generation + cursor + i, k);
		}

		pthread_barrier_wait(thread.barrier);

		tmp = thread.current_generation;
		thread.current_generation = thread.next_generation;
		thread.next_generation = tmp;

		for (int i = start; i < stop; ++i) {
			thread.current_generation[i].index = i;
		}

		if (k % 5 == 0 && id == 0) {
			print_best_fitness(thread.current_generation);
		}

		pthread_barrier_wait(thread.barrier);
	}

	// compute for the last operation the fitness and sort them
	compute_fitness_function(thread.objects, thread.current_generation, start, stop, thread.sack_capacity);

	pthread_barrier_wait(thread.barrier);

	qsort(thread.current_generation + start, stop - start, sizeof(individual), cmpfunc);
	pthread_barrier_wait(thread.barrier);

	if (id == 0) {
		for (int i = 1; i <	P; ++i) {
			int beginning = i * (double) N / P;
			int ending = min((i + 1) * (double) N / P, N);

			memcpy(thread.current_generation, merge(thread.current_generation, 0, beginning, ending), ending * sizeof(individual));
		}
	}

	pthread_barrier_wait(thread.barrier);

	if (id == 0) {
		print_best_fitness(thread.current_generation);
	}

	return NULL;
}

// same function from the lab3
individual *merge(individual *source, int start, int mid, int end) {
	int iA = start;
	int iB = mid;
	int i;
	individual* tmp = (individual*) malloc(sizeof(individual) * (end - start));

	for (i = start; i < end; i++) {
		if (end == iB || (iA < mid && cmpfunc(&source[iA], &source[iB]) <= 0)) {
			tmp[i] = source[iA];
			iA++;
		} else {
			tmp[i] = source[iB];
			iB++;
		}
	}

	return tmp;
}