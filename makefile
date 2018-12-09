OMPCFLAGS = -mcx16 -O2 -DOPENMP -fopenmp -Wall -std=c++11 -march=native

ifdef CLANG
CC = clang++ 
CFLAGS = -mcx16 -O2 -DCILK -fcilkplus -ldl -std=c++11 -march=native
else ifdef ICC
CC = /opt/intel/bin/icpc
CFLAGS = -O2 -std=c++11 -DCILK -march=native
else ifdef CILK
CC = g++
CFLAGS = -mcx16 -O2 -DCILK -Wall -fcilkplus -std=c++11 -march=native
else ifdef OPENMP
CC = g++
CFLAGS = $(OMPCFLAGS)
else ifdef HOMEGROWN
CC = g++
CFLAGS = -fopenmp -mcx16 -O2 -Wall -std=c++11 -march=native -DHOMEGROWN
else ifdef SERIAL
CC = g++
CFLAGS = -mcx16 -O2 -Wall -std=c++11 -march=native
else # default is cilk
CC = g++
CFLAGS = -mcx16 -O2 -DCILK -Wall -fcilkplus -std=c++11 -march=native
endif

AllFiles = allocator.h alloc.h bag.h binary_search.h block_allocator.h collect_reduce.h concurrent_stack.h counting_sort.h get_time.h hash_table.h histogram.h integer_sort.h list_allocator.h memory_size.h merge.h merge_sort.h monoid.h parallel.h parse_command_line.h par_string.h quicksort.h random.h random_shuffle.h reducer.h sample_sort.h seq.h sequence_ops.h sparse_mat_vec_mult.h time_operations.h transpose.h utilities.h

time_tests:	$(AllFiles) time_tests.cpp time_operations.h
	$(CC) $(CFLAGS) time_tests.cpp -o time_tests

test:	test.cpp scheduler.h
	$(CC) $(OMPCFLAGS) test.cpp -o test

test2:	test2.cpp
	$(CC) $(CFLAGS) test2.cpp -o test2

all:	time_tests 

clean:
	rm -f time_tests test
