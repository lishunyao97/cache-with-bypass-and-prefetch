#include <stdio.h>
#include "cache.h"
#include "memory.h"
#include <fstream>
#include <iostream>
#include <string>
using namespace std;
int main(int argc, char * argv[]) {
	int repeat = 1;
	// cache parameters
	int cache_size = 32 * 1024;
	int associativity = 8;
	int block_size = 64;
	int write_through = 0;
	int write_allocate = 1;
	// 0 for LRU, 1 for LFU
	int replace_algorithm = 1;
	int bypass = 0;
	int prefetch = 0;
	Memory m;
	Cache l1(32 * 1024, 8, 64, 0, 1, 0, 0, 0);
	Cache l2(256 * 1024, 8, 64, 0, 1, 0, 0, 0);
	l1.SetLower(&l2);
	l2.SetLower(&m);
	l1.SetMem(&m);
	l2.SetMem(&m);
	StorageStats s;
	s.access_time = 0;
	s.access_counter = 0;
	s.miss_counter = 0;
	s.dirty_replace = 0;
	s.set_dirty = 0;
	m.SetStats(s);
	l1.SetStats(s);
	l2.SetStats(s);

	StorageLatency ml;
	ml.bus_latency = 0;
	ml.hit_latency = 100;
	m.SetLatency(ml);

	StorageLatency l1l;
	l1l.bus_latency = 0;
	l1l.hit_latency = 3;
	l1.SetLatency(l1l);

	StorageLatency l2l;
	l2l.bus_latency = 6;
	l2l.hit_latency = 4;
	l2.SetLatency(l2l);

	char *filename = "2.trace";
	/*
	if (argc == 1) {
		printf("Please input file name. Format: ./cache <filename>\n");
		return 0;
	}
	if (argc == 2) {
		filename = argv[1];
	}*/
	//cout<<"--------------------"<<argv[1]<<"---------------------"<<endl;
	cout << "------------------" << filename << "-------------------" << endl;
	for(int i = 0; i < repeat; i++) {
		int hit, time;
		char content[64];
		ifstream infile;
		infile.open(filename);
		if (!infile) {
			cout << "file cannot open" << endl;
			return -1;
		}
		else {
			string c;
			char hex_num[20];
			//uint64_t num;
			while (!infile.eof()) {
				//infile >> c >> num;
				//cout<<c<<" "<<num<<endl;
				infile >> c >> hex_num;
				if(infile.fail()) break;   
				long long unsigned num = 0;
				sscanf(hex_num, "%llx", &num);
				int read = 0;
				if (c == "r") read = 1;
				else if (c == "w") read = 0; 
				l1.HandleRequest(num, 0, read, content, hit, time);
			}
		}
		infile.close();

	}

	l1.GetStats(s);
	printf("Total L1 access time: %d CPU cycle\n", s.access_time);
	printf("Total L1 access cnt: %d\n", s.access_counter);
	//int time1 = s.access_time;
	int access1 = s.access_counter;
	int miss1 = s.miss_counter;
	int dirty1 = s.dirty_replace;
	int set_dirty1 = s.set_dirty;
	l2.GetStats(s);
	printf("Total L2 access time: %d CPU cycle\n", s.access_time);
	printf("Total L2 access cnt: %d\n", s.access_counter);
	//int time2 = s.access_time;
	int access2 = s.access_counter;
	int miss2 = s.miss_counter;
	int dirty2 = s.dirty_replace;
	int set_dirty2 = s.set_dirty;
	m.GetStats(s);
	printf("Total Memory access time: %d CPU cycle\n", s.access_time);
	printf("Total Memory access cnt: %d\n", s.access_counter);
	//int time3 = s.access_time;
	int access3 = s.access_counter;
	int miss3 = s.miss_counter;
	double L1_miss_rate = (double)miss1 / (double)access1;
	double L2_miss_rate = (double)miss2 / (double)access2;

	printf("L1 miss rate: %lf%%\n", L1_miss_rate * 100);
	printf("L2 miss rate: %lf%%\n", L2_miss_rate * 100);
	//printf("Total access time: %dns\n", time1+time2+time3);
	double L2_AMAT = 6 + 4 + L2_miss_rate * 100;
	double L1_AMAT = 3 + L1_miss_rate * L2_AMAT;
	printf("AMAT: %lf\n", L1_AMAT);
	printf("L1 dirty replace: %d\n",dirty1);
	printf("L2 dirty replace: %d\n",dirty2);
	printf("L1 set dirty: %d\n",set_dirty1);
	printf("L2 set dirty: %d\n",set_dirty2);
	return 0;
}
