#include "config-host.h"
#include "cache.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

void cache_initialize(Cache_t* cache, const char* name, int nset, int bsize, int assoc, 
		                 enum Cache_policy policy, unsigned int hit_lat,
						 unsigned int miss_lat){
	
	int i;
	cache->num_access = 0;
	cache->num_miss = 0;
	cache->num_miss_star = 0;
	cache->cycle_lastmiss = -1;


	cache->config.name = (char*) malloc(strlen(name)+1);
	strcpy(cache->config.name, name);
	cache->config.nset = nset;
	cache->config.bsize = bsize;
	cache->config.assoc = assoc;
	cache->config.policy = policy;
	cache->config.hit_lat = hit_lat;
	cache->config.miss_lat = miss_lat;
	
	cache->len_offset_bit = (int)log2((double)bsize);
	cache->len_idx_bit = (int)log2((double)nset);
	cache->len_tag_bit = 32 - cache->len_offset_bit - cache->len_idx_bit;

	cache->offset_mask = (( 1<< cache->len_offset_bit) - 1);
	cache->addr_mask = ~(( 1 << cache->len_offset_bit) - 1);

	cache->cache_set = (Cache_set_t**) malloc(sizeof(Cache_set_t*)*nset);
	for (i=0 ; i<nset ; i++){
		
		cache->cache_set[i] = (Cache_set_t*) (malloc (sizeof(Cache_set_t)));
		cacheset_initialize(cache, cache->cache_set[i], assoc);
	}
	
//	fprintf(stderr, "Cache %s is initialized...\n", name);
}



void cache_copy(Cache_set_t **target_set, Cache_set_t **source_set, int nset){
	int i;

	target_set = (Cache_set_t**) malloc(sizeof(Cache_set_t*)*nset);
	for (i=0 ; i<nset ; i++){
		//target_set[i] = (Cache_set_t*) (malloc (sizeof(Cache_set_t)));
		//cacheset_initialize(target_set[i], assoc);
	
		Cache_blk_t *target_blk = target_set[i]->first_blk;
		Cache_blk_t *source_blk = source_set[i]->first_blk;

		
		while (target_blk!=NULL){

			target_blk->valid = source_blk->valid;
			target_blk->dirty = source_blk->dirty;
			target_blk->tag = source_blk->tag;
			
			target_blk = target_blk->next_blk;
			source_blk = source_blk->next_blk;
		}
		
	}
	
	//fprintf(stderr, "Cache %s is initialized...\n", name);




	//memcpy(target_set, source_set, sizeof(Cache_set_t));
}


int cache_access(uint64_t cur_cycle, Cache_t* cache, enum CacheAccessType type, md_addr_t addr){
	
	//find the set by using idx. Do masking...
//return 0;
	int i;
	unsigned int mask =  (1 << (cache->len_idx_bit))-1;
	unsigned int idx = ((addr >> cache->len_offset_bit) & mask);
	unsigned int tag =  (addr >> (cache->len_offset_bit + cache->len_idx_bit));

	cache->num_access++;
	

	Cache_set_t *set = cache->cache_set[idx];

	Cache_blk_t *target_blk = set->first_blk;
	
	while (target_blk!=NULL){
		if (target_blk->valid && target_blk->tag == tag){
			//cout << "HIT!!" << endl;
			if (cache->config.policy == LRU)
				cacheset_moveBlkAtFirst(set, target_blk);

			return cache->config.hit_lat;
		}
		target_blk = target_blk->next_blk;
	}
	switch (cache->config.policy){
		case LRU:
		case FIFO:
			target_blk = set->last_blk;
			break;
		case Random:
			srand(time(NULL));
			int victimIdx = rand() % cache->config.assoc;
			target_blk = set->first_blk;
			for (i=0 ; i<victimIdx ; i++){
				target_blk = target_blk->next_blk;
			}
			 
			break;
	}
//	cout << "victim tag: " << hex << target_blk->tag << endl;
	target_blk->valid = true;
	target_blk->tag = tag;
	//cout << "MISS!!" << endl;
	//


	// I don't want to maintain the window in the simulator.
	// I just want to check the distance between the current instruction and the last miss instruction.
	
//	printf("%u\n", (unsigned) cur_cycle);
	if (cache->cycle_lastmiss==-1 || (cur_cycle - cache->cycle_lastmiss) > cache->config.miss_lat ){ 
		cache->num_miss_star++;
		cache->cycle_lastmiss = cur_cycle;
	}
	else {
//		printf("OVERLAP!!!");
//		cache->cycle_lastmiss = cur_cycle;
	}
	cache->num_miss++;
	//cache->cycle_lastmiss = cur_cycle;
	cacheset_moveBlkAtFirst(set, target_blk);

	return cache->config.miss_lat;
	
}



//FIXME
int mcache_readTraceFile(Cache_t* cache1, Cache_t* cache2, const char* filename){


	FILE *f;
	f=fopen(filename,"r");
	char buf[1024];
	while (!feof(f)){
		//char *ret = 
		if (fgets(buf, 1024, f) == NULL) printf("mcache_readTraceFile error\n");
		unsigned long addr =strtoul(buf, NULL, 16);
		int lat = cache_access(0, cache1, Read, addr);
		if (lat > cache1->config.hit_lat){
			lat += cache_access(0, cache2, Read, addr);
		}
	}

	fclose(f);
	return 0;

}

void cache_print(Cache_t* cache){

		fprintf(stdout, "--------------\n");	
		fprintf(stdout, "Cache %s\n", cache->config.name);
		fprintf(stdout, "--------------\n");	
		fprintf(stdout, "num_access: %" PRIu64 "\n",cache->num_access);
		fprintf(stdout, "num_miss: %" PRIu64 "\n", cache->num_miss);
		fprintf(stdout, "num_miss_star: %" PRIu64 "\n", cache->num_miss_star);

}

void cache_close(Cache_t* cache){

	if (cache->cache_set!=NULL){
		int i;
      	for (i=0 ; i<cache->config.nset ; i++){
			Cache_blk_t *blk = cache->cache_set[i]->first_blk;
			
			while (blk!=NULL){
				Cache_blk_t *nextblk = blk->next_blk;
				if (blk->data)
					free(blk->data);
				free(blk);
				blk = nextblk;
			}
			free(cache->cache_set[i]);
	    }
		free(cache->cache_set);
	}
	free(cache);
	
}

void cacheblock_initialize(Cache_t* cache, Cache_blk_t* block){
	block->valid = false;
	block->dirty = false;
	block->tag = 0;
	block->data = (uint8_t *) malloc(cache->config.bsize);
	block->prev_blk = NULL;
	block->next_blk = NULL;
	
}


void cacheset_initialize(Cache_t* cache, Cache_set_t *set, int assoc){
	int i;	
	for (i=0 ; i<assoc ; i++){
		Cache_blk_t *blk = (Cache_blk_t*) malloc(sizeof(Cache_blk_t));
		//printf("%X ", blk);
		cacheblock_initialize(cache, blk);
		if (i==0){
			set->first_blk = blk;
			set->last_blk = blk;
		}
		else {
			blk->prev_blk = set->last_blk;
			blk->next_blk = NULL;
			set->last_blk->next_blk = blk; 
			set->last_blk = blk;
		}
	}
	//cache_printSetTags(set);

}


void cacheset_moveBlkAtFirst(Cache_set_t* set, Cache_blk_t* blk){

	if (set->first_blk == blk){
		return;
	}
	
	//remove
	if (blk->prev_blk != NULL){
		blk->prev_blk->next_blk = blk->next_blk;
	}
	else {
		set->first_blk = blk->next_blk;
	}
	if (blk->next_blk != NULL){
		blk->next_blk->prev_blk = blk->prev_blk;
	}
	else {
		set->last_blk = blk->prev_blk;
	}

	//add
	blk->prev_blk = NULL;
	blk->next_blk = set->first_blk;

	set->first_blk->prev_blk = blk;
	set->first_blk = blk;

}


void cacheset_printSetTags(Cache_set_t* set){

	Cache_blk_t *target_blk = set->first_blk;
	while (target_blk!=NULL){
		printf("%X ", target_blk->tag);
		target_blk = target_blk->next_blk;
	}
	printf("\n");
}

int cache_access_data(uint64_t cur_cycle, Cache_t* cache, enum CacheAccessType type, md_addr_t addr, uint8_t *data, int size){
	
	int i;
	unsigned int mask =  (1 << (cache->len_idx_bit))-1;
	unsigned int idx = ((addr >> cache->len_offset_bit) & mask);
	unsigned int tag =  (addr >> (cache->len_offset_bit + cache->len_idx_bit));

	cache->num_access++;

	Cache_set_t *set = cache->cache_set[idx];
	Cache_blk_t *target_blk = set->first_blk;

	while (target_blk!=NULL){
		if (target_blk->valid && target_blk->tag == tag){
			if (cache->config.policy == LRU)
				cacheset_moveBlkAtFirst(set, target_blk);

			if (type == Write){
				memcpy(&(target_blk->data[addr & cache->offset_mask]), data, size);
				target_blk->dirty = true;
			}else if (type == Read){                                                                                  
				memcpy(data, &(target_blk->data[addr & cache->offset_mask]), size);
			}

			return 0;	//hit
		}
		target_blk = target_blk->next_blk;
	}
	switch (cache->config.policy){
		case LRU:
		case FIFO:
			target_blk = set->last_blk;
			break;
		case Random:
			srand(time(NULL));
			int victimIdx = rand() % cache->config.assoc;
			target_blk = set->first_blk;
			for (i=0 ; i<victimIdx ; i++){
				target_blk = target_blk->next_blk;
			}
			 
			break;
	}
	target_blk->valid = true;
	target_blk->tag = tag;

	// I don't want to maintain the window in the simulator.
	// I just want to check the distance between the current instruction and the last miss instruction.
	
//	printf("%u\n", (unsigned) cur_cycle);
	if (cache->cycle_lastmiss==-1 || (cur_cycle - cache->cycle_lastmiss) > cache->config.miss_lat ){ 
		cache->num_miss_star++;
		cache->cycle_lastmiss = cur_cycle;
	}
	else {
//		printf("OVERLAP!!!");
//		cache->cycle_lastmiss = cur_cycle;
	}
	cache->num_miss++;
	//cache->cycle_lastmiss = cur_cycle;
	cacheset_moveBlkAtFirst(set, target_blk);

	return 1;	//miss
	
}

//precondition: the target blk is reserved.
int cache_update_data(uint64_t cur_cycle, Cache_t* cache,  md_addr_t addr, uint8_t *data){
	
	unsigned int mask =  (1 << (cache->len_idx_bit))-1;
	unsigned int idx = ((addr >> cache->len_offset_bit) & mask);
	unsigned int tag =  (addr >> (cache->len_offset_bit + cache->len_idx_bit));

	cache->num_access++;
	Cache_set_t *set = cache->cache_set[idx];
	Cache_blk_t *target_blk = set->first_blk;
	
	while (target_blk!=NULL){
		if (target_blk->valid && target_blk->tag == tag){
			//cout << "HIT!!" << endl;
			memcpy(target_blk->data, data, cache->config.bsize);
			return 1;
		}
		target_blk = target_blk->next_blk;
	}
	return 0;
}


void cache_invalidate_data(Cache_t* cache){

	if (cache->cache_set!=NULL){
		int i;
      	for (i=0 ; i<cache->config.nset ; i++){
			Cache_blk_t *blk = cache->cache_set[i]->first_blk;
			
			while (blk!=NULL){
				Cache_blk_t *nextblk = blk->next_blk;
				blk->valid = false;
				blk->dirty = false;
				blk = nextblk;
			}
	    }
	}

}
