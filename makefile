ifdef CLANG
CC = clang++ 
CFLAGS = -mcx16 -O2 -fcilkplus -ldl -std=c++11 -march=native
else ifdef ICC
CC = /opt/intel/bin/icpc
CFLAGS = -O2 -std=c++11 -march=native
else
CC = g++
# works with g++ (GCC) 4.9.0 (compiled for cilkplus)
CFLAGS = -mcx16 -O2 -Wall -fcilkplus -std=c++11 -march=native
endif

time_tests:	time_tests.cpp time_operations.h
	$(CC) $(CFLAGS) time_tests.cpp -o time_tests

tt:	tt.cpp time_operations.h
	$(CC) $(CFLAGS) tt.cpp -o tt

all:	time_tests 

clean:
	rm -f time_tests
