#ifndef CACHE_CACHE_H_
#define CACHE_CACHE_H_

#include <stdint.h>
#include <cmath>
#include <vector>
#include <set>
#include "storage.h"
using namespace std;
class CacheLine {
public:
	//CacheLine(){}
	//~CacheLine(){}
	CacheLine(int block_size_, int t_) {
		valid = 0;
		block_size = block_size_;
		t = t_;
		tag = 0;
		recently_use = 0;
		frequent_use = 0;
		dirty = 0;
		//byte = new int[block_size_];
	}
	~CacheLine() {
		//delete[]byte;
	}

	int valid; // 0|1 for not-valid|valid
	int t; // t tag bits
	uint64_t tag; // tag bits 
	int block_size; // B
	//int *byte; 
	int recently_use;
	int frequent_use;
	int dirty;

};
class CacheSet {
public:
	//CacheSet(){}
	//~CacheSet(){}
	CacheSet(int asso_,int block_size_, int t_) {
		E = asso_;
		//line = new CacheLine[E];
		recently_use_cnt = 0;
		for (int i = 0; i < E; i++) {
			line.push_back(CacheLine(block_size_,t_));
		}
	}
	~CacheSet() {}

	int E; // number of lines in a set
	int recently_use_cnt;
	vector<CacheLine> line;
	set<uint64_t> evict_tag;
	set<uint64_t> bypass_tag;

};
typedef struct CacheConfig_ {
  int size; // C
  int associativity; // E
  int block_size; // B
  int set_num; // S
  int t;
  int s;
  int b;
  int write_through; // 0|1 for back|through
  int write_allocate; // 0|1 for no-alc|alc
  int replace_algorithm; // 0 for LRU, 1 for LFU
  int bypass;
  int prefetch;
} CacheConfig;

class Cache: public Storage {
public:
	Cache(int size_, int asso_, int block_size_, int write_through_, int write_allocate_,
		int replace_algorithm_, int bypass_, int prefetch_) {
	  config_.size = size_; // C=S*E*B
	  config_.associativity = asso_; // E
	  config_.block_size = block_size_; // B
	  config_.set_num = size_ / (asso_*block_size_); // S=C/(E*B)
	  config_.s = log(config_.set_num) / log(2); // s
	  config_.b = log(config_.block_size) / log(2); // b
	  config_.t = 64 - config_.s - config_.b; // t
	  config_.write_through = write_through_;
	  config_.write_allocate = write_allocate_;
	  config_.replace_algorithm = replace_algorithm_;
	  config_.bypass = bypass_;
	  config_.prefetch = prefetch_;
	 // set = new CacheSet[config_.set_num];
	  for (int i = 0; i < config_.set_num; i++) {
	  	set.push_back(CacheSet(config_.associativity, config_.block_size, config_.t));
	  }

	}
	~Cache() {}

  // Sets & Gets
	void SetConfig(CacheConfig cc);
	void GetConfig(CacheConfig cc);
	void SetLower(Storage *ll) { lower_ = ll; }
	void SetMem(Storage *mem) { mem_ = mem; }
  // Main access process
	void HandleRequest(uint64_t addr, int bytes, int read,
		char *content, int &hit, int &time);

private:
  // Partitioning
	void PartitionAlgorithm(uint64_t addr, uint64_t &tag_value, uint64_t &set_value, uint64_t &block_value);
  // Replacement
	int ReplaceDecision(uint64_t tag_value, uint64_t set_value, uint64_t block_value, int read);
	void ReplaceAlgorithm(uint64_t tag_value, uint64_t set_value, uint64_t block_value, int &write_back_dirty, 
		uint64_t &tag_dirty, int &replace_line);
  // Prefetching
	void PrefetchAlgorithm(uint64_t addr, int bytes, int read, char *content, int &hit, int &time);
	int CapacityMiss(uint64_t tag_value, uint64_t set_value);
	int SetIsFull(uint64_t set_value);
	CacheConfig config_;
	Storage *lower_;
	Storage *mem_;
	DISALLOW_COPY_AND_ASSIGN(Cache);
	vector<CacheSet> set;
};

#endif //CACHE_CACHE_H_ 
