
bitset_cmp: bitset_cmp.cpp
	g++ -O3 -DBITSET_SIZE=512 bitset_cmp.cpp -lrt -o bitset_cmp

map_size_test: map_size_test.cpp
	g++ -O3 map_size_test.cpp -lrt -o map_size_test

shared_lock_test: shared_lock_test.cpp
	g++ -O3 -I../third_party/include shared_lock_test.cpp -lboost_system \
	-lrt -pthread -Wl,-rpath,../third_party/lib -L../third_party/lib \
	-lglog -o shared_lock_test
clean:
	rm -rf bitset_cmp map_size_test shared_lock_test