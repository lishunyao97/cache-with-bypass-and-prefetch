#include "cache.h"
#include "def.h"
#include <iostream>
#include <set>
#include <iterator>
#include <math.h>
using namespace std;

uint64_t getbit(uint64_t addr, int s, int e) {
	return (addr >> s)&((1 << (e - s + 1)) - 1);
}
void Cache::HandleRequest(uint64_t addr, int bytes, int read, char *content, int &hit, int &time) {
	hit = 0;
	time = 0;
	uint64_t tag_value = 0;
	uint64_t set_value = 0;
	uint64_t block_value = 0;
	int write_back_dirty = 0;
	uint64_t tag_dirty = 0;
	int replace_line = -1;
	stats_.access_counter++;
    PartitionAlgorithm(addr, tag_value, set_value, block_value);
    /* READ */
	if (read == 1) {
		/* READ MISS */
		if (ReplaceDecision(tag_value, set_value, block_value, read)) {
			hit = 0;
			stats_.miss_counter++;
		  	/* BYPASS */
			if(config_.bypass == 1 && SetIsFull(set_value) && CapacityMiss(tag_value, set_value)){
				set[set_value].bypass_tag.insert(tag_value);
                stats_.access_counter--;
                stats_.miss_counter--;
		  	  	// fetch from memory
				int lower_hit, lower_time;
				mem_->HandleRequest(addr, bytes, 1, content, lower_hit, lower_time);
				time += latency_.bus_latency + lower_time;
				stats_.access_time += latency_.bus_latency;
				return;
			}
            /* PREFETCH */
            if (config_.prefetch == 1) {
                int lower_hit, lower_time;
                PrefetchAlgorithm(addr, bytes, 1, content, lower_hit, lower_time);
                
            }
            /* NO PREFETCH */
            else {
                // Fetch from lower layer
                int lower_hit, lower_time;
                lower_->HandleRequest(addr, bytes, 1, content, lower_hit, lower_time);
                time += latency_.bus_latency + latency_.hit_latency + lower_time;
                stats_.access_time += latency_.bus_latency + latency_.hit_latency;
            }
			ReplaceAlgorithm(tag_value, set_value, block_value, write_back_dirty, tag_dirty, replace_line);
			/* Dirty Replacement */
			if (config_.write_through == 0 && write_back_dirty == 1) {
				stats_.dirty_replace++;
				int b = config_.b;
				int s = config_.s;
				uint64_t addr_dirty = (tag_dirty << (b + s)) + (set_value << b);
				set[set_value].line[replace_line].dirty = 0;
				int lower_hit, lower_time;
				lower_->HandleRequest(addr_dirty, bytes, 0, content, lower_hit, lower_time);
				time += latency_.bus_latency + latency_.hit_latency + lower_time;
				stats_.access_time += latency_.bus_latency + latency_.hit_latency;
			}
            return;
		}
		/* READ HIT */
		else {
			hit = 1;
			time += latency_.bus_latency + latency_.hit_latency;
			stats_.access_time += time;
			return;
		}
	}
	/* WRITE */
	int miss = ReplaceDecision(tag_value, set_value, block_value, read);
	/* WRITE HIT */
	if (miss == 0) {
		hit = 1;
		/* WRITE-THROUGH */
		if (config_.write_through == 1) {
			int lower_hit, lower_time;
			lower_->HandleRequest(addr, bytes, read, content, lower_hit, lower_time);
			time += latency_.bus_latency +latency_.hit_latency+ lower_time;
			stats_.access_time += latency_.bus_latency + latency_.hit_latency;

		}
		/* WRITE-BACK */
		else {
			for (int i = 0; i < config_.associativity; i++) {
				if (set[set_value].line[i].valid == 1 && set[set_value].line[i].tag == tag_value) {
					set[set_value].line[i].dirty = 1;
                    stats_.set_dirty++;
                    break;
				}
			}	
			time += latency_.bus_latency + latency_.hit_latency;
			stats_.access_time += latency_.bus_latency + latency_.hit_latency;
		}
		return;
	}
	/* WRITE MISS */
	hit = 0;
	stats_.miss_counter++;
	/* BYPASS */
	if(config_.bypass == 1 && SetIsFull(set_value) && CapacityMiss(tag_value, set_value)){
		set[set_value].bypass_tag.insert(tag_value);
        stats_.access_counter--;
        stats_.miss_counter--;
		// fetch from memory
		int lower_hit, lower_time;
		mem_->HandleRequest(addr, bytes, read, content, lower_hit, lower_time);
		time += latency_.bus_latency + lower_time;
		stats_.access_time += latency_.bus_latency;
		return;
	}
    /* WRITE-ALLOCATE */
	if (config_.write_allocate == 1) {
        /* PREFETCH */
        if (config_.prefetch == 1) {
            int lower_hit, lower_time;
            PrefetchAlgorithm(addr, bytes, 1, content, lower_hit, lower_time);
        }
        /* NO PREFETCH */
        else {
            // Fetch from lower layer
            int lower_hit, lower_time;
            lower_->HandleRequest(addr, bytes, 1, content, lower_hit, lower_time);
            time += latency_.bus_latency + latency_.hit_latency + lower_time;
            stats_.access_time += latency_.bus_latency + latency_.hit_latency;
        }
		ReplaceAlgorithm(tag_value, set_value, block_value, write_back_dirty, tag_dirty, replace_line);
		/* Dirty Replacement */
		if (config_.write_through == 0 && write_back_dirty == 1) {
			stats_.dirty_replace++;
			set[set_value].line[replace_line].dirty = 0;
			int b = config_.b;
			int s = config_.s;
			uint64_t addr_dirty = (tag_dirty<<(b+s))+ (set_value<<b);

			int lower_hit, lower_time;
			lower_->HandleRequest(addr_dirty, bytes, 0, content, lower_hit, lower_time);
			time += latency_.bus_latency + latency_.hit_latency + lower_time;
			stats_.access_time += latency_.bus_latency + latency_.hit_latency;
		}
		set[set_value].line[replace_line].dirty = 1;
		stats_.set_dirty++;
	}
	/* NOT-WRITE-ALLOCATE */
	else {
		// Write to lower layer
		int lower_hit, lower_time;
		lower_->HandleRequest(addr, bytes, 0, content, lower_hit, lower_time);
		time += latency_.bus_latency + latency_.hit_latency;
		stats_.access_time += latency_.bus_latency + latency_.hit_latency;
	}
}

void Cache::PartitionAlgorithm(uint64_t addr, uint64_t &tag_value, uint64_t &set_value, uint64_t &block_value) {
	int b = config_.b;
	int s = config_.s;
	tag_value = getbit(addr, b + s, 63);
	set_value = getbit(addr, b, b + s - 1);
	block_value = getbit(addr, 0, b - 1);
}

// if cache hit, return FALSE
// else if cache miss, return TRUE
int Cache::ReplaceDecision(uint64_t tag_value, uint64_t set_value, uint64_t block_value, int read) {
	for (int i = 0; i < config_.associativity; i++) {
		// cache Hit
		if (set[set_value].line[i].valid == 1 && set[set_value].line[i].tag == tag_value) {
			set[set_value].line[i].frequent_use++;
			set[set_value].recently_use_cnt++;
			set[set_value].line[i].recently_use = set[set_value].recently_use_cnt;
            set[set_value].line[i].CRF_access.insert(set[set_value].recently_use_cnt);
			return FALSE;
		}
	}
	return TRUE;
}

void Cache::ReplaceAlgorithm(uint64_t tag_value, uint64_t set_value, uint64_t block_value,int &write_back_dirty,
	uint64_t &tag_dirty, int &replace_line){
    // check this set full or not
	bool set_is_full = true;
	int empty_index = -1; // empty line in set
	for (int i = 0; i < config_.associativity; i++) {
		if (set[set_value].line[i].valid == 0) {
			set_is_full = false;
			empty_index = i;
			break;
		}
	}
	// set is not full
	if (set_is_full == false) {
		set[set_value].line[empty_index].valid = 1;
		set[set_value].line[empty_index].tag = tag_value;
		set[set_value].line[empty_index].frequent_use = 1;
		set[set_value].recently_use_cnt++;
		set[set_value].line[empty_index].recently_use = set[set_value].recently_use_cnt;
        set[set_value].line[empty_index].CRF_access.clear();
        set[set_value].line[empty_index].CRF_access.insert(set[set_value].recently_use_cnt);
		replace_line = empty_index;
		return;
	}
	// set is full, use specefic replace algorithm
	// LRU
	if(config_.replace_algorithm == 0){
		int replace_index = 0;
		for (int i = 0; i < config_.associativity; i++) {
			if (set[set_value].line[i].recently_use < set[set_value].line[replace_index].recently_use) {
				replace_index = i;
			}
		}
		replace_line = replace_index;
		set[set_value].evict_tag.insert(set[set_value].line[replace_index].tag);
		if (set[set_value].line[replace_index].dirty == 1) {
			write_back_dirty = 1;
			tag_dirty = set[set_value].line[replace_index].tag;
		}
		set[set_value].line[replace_index].valid = 1;
		set[set_value].line[replace_index].tag = tag_value;
		set[set_value].recently_use_cnt++;
		set[set_value].line[replace_index].recently_use = set[set_value].recently_use_cnt;
		return;
	}
	// LFU
	else if(config_.replace_algorithm == 1){
		int replace_index = 0;
		for (int i = 0; i < config_.associativity; i++) {
			if (set[set_value].line[i].frequent_use < set[set_value].line[replace_index].frequent_use) {
				replace_index = i;
			}
		}
		replace_line = replace_index;
		set[set_value].evict_tag.insert(set[set_value].line[replace_index].tag);
		if (set[set_value].line[replace_index].dirty == 1) {
			write_back_dirty = 1;
			tag_dirty = set[set_value].line[replace_index].tag;
		}
		set[set_value].line[replace_index].valid = 1;
		set[set_value].line[replace_index].tag = tag_value;
		set[set_value].line[replace_index].frequent_use = 1;
		return;
	}
    //LRFU
    else if(config_.replace_algorithm == 2){
        set[set_value].recently_use_cnt++;
        for(int i=0;i<config_.associativity;i++){
            set[set_value].line[i].CRF = CalculateCRF(set_value,i,set[set_value].recently_use_cnt);
        }
        int replace_index = 0;
        for (int i = 0; i < config_.associativity; i++) {
            if (set[set_value].line[i].CRF < set[set_value].line[replace_index].CRF) {
                replace_index = i;
            }
        }
        replace_line = replace_index;
        set[set_value].evict_tag.insert(set[set_value].line[replace_index].tag);
        if (set[set_value].line[replace_index].dirty == 1) {
            write_back_dirty = 1;
            tag_dirty = set[set_value].line[replace_index].tag;
        }
        set[set_value].line[replace_index].valid = 1;
        set[set_value].line[replace_index].tag = tag_value;
        set[set_value].line[replace_index].CRF_access.clear();
        set[set_value].line[replace_index].CRF_access.insert(set[set_value].recently_use_cnt);
    }
}
int Cache::CalculateCRF(uint64_t set_value, int line_value, int time_now){
    if(set[set_value].line[line_value].CRF_access.empty()){
        return 0;
    }
    int size = set[set_value].line[line_value].CRF_access.size();
    //set<int>::iterator it;
    int CRF = 0;
    double lamda = 0.5;
    for (set[set_value].line[line_value].it=set[set_value].line[line_value].CRF_access.begin();set[set_value].line[line_value].it!=set[set_value].line[line_value].CRF_access.end();set[set_value].line[line_value].it++){
        CRF+=pow(1/2,lamda*(time_now-*set[set_value].line[line_value].it));
    }
    return CRF;
}

void Cache::PrefetchAlgorithm(uint64_t addr, int bytes, int read, char *content, int &hit, int &time) {
    lower_->HandleRequest(addr, bytes, 1, content, hit, time);
    // latter 3
    for (int i = 0; i < 3; i++) {
        addr = addr + 64;
        uint64_t tag_value = 0;
        uint64_t set_value = 0;
        uint64_t block_value = 0;
        int write_back_dirty = 0;
        uint64_t tag_dirty = 0;
        int replace_line = -1;
        PartitionAlgorithm(addr, tag_value, set_value, block_value);
        ReplaceAlgorithm(tag_value, set_value, block_value, write_back_dirty, tag_dirty, replace_line);
        /* Dirty Replacement */
        if (config_.write_through == 0 && write_back_dirty == 1) {
            stats_.dirty_replace++;
            int b = config_.b;
            int s = config_.s;
            uint64_t addr_dirty = (tag_dirty << (b + s)) + (set_value << b);
            set[set_value].line[replace_line].dirty = 0;
            int lower_hit, lower_time;
            lower_->HandleRequest(addr_dirty, bytes, 0, content, lower_hit, lower_time);
            time += latency_.bus_latency + latency_.hit_latency + lower_time;
            stats_.access_time += latency_.bus_latency + latency_.hit_latency;
        }
    }
}

int Cache::CapacityMiss(uint64_t tag_value, uint64_t set_value){
	// new miss tag in evict tags, conflict miss
	if(set[set_value].evict_tag.find(tag_value) != set[set_value].evict_tag.end())
		return FALSE;
	// else, capacity miss
	return TRUE;
}
int Cache::SetIsFull(uint64_t set_value){
	int set_is_full = TRUE;
	int empty_index = -1;
	for (int i = 0; i < config_.associativity; i++) {
		if (set[set_value].line[i].valid == 0) {
			set_is_full = FALSE;
			empty_index = i;
			break;
		}
	}
	return set_is_full;
}
