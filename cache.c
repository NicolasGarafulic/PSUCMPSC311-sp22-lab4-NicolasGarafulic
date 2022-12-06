#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
  //checks if the currently is a cache
  if (cache != NULL){
    return -1;
  }
  if(num_entries<2){
    return -1;
  }
  if(num_entries>4096){
    return -1;
  }
  //creates and sets the up the cache
  int i;
    cache = malloc(sizeof(cache_entry_t) * num_entries);
    for(i=0; i<num_entries;i++){
      cache[i].valid=false;
    }
    cache_size = num_entries;
    clock = 0;
    num_queries = 0;
    num_hits = 0;
  
  return 1;
}

int cache_destroy(void) {
  //checks to see if there currently is a cache
  if(cache == NULL){
    return -1;
  }
  //resets the initial values of the cache data
  cache = NULL;
  cache_size = 0;
  clock=0;
  num_queries=0;
  num_hits=0;
  
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  int i,j;
  if(buf == NULL){
    return -1;
  }
  if(cache == NULL){
    return -1;
  }
  if(cache_size==0){
    return -1;
  }
  if(clock==0){
    return -1;
  }

  //updates data
  clock++;
  num_queries++;

  //finds a copies the data from the cache
  for(i=0; i<cache_size; i++){
    if(cache[i].disk_num == disk_num){
      if(cache[i].block_num == block_num){
        for(j=0; j<JBOD_BLOCK_SIZE; j++){
          buf[j] = cache[i].block[j];
        }
        cache[i].access_time = clock;
        num_hits++;
        return 1;
      }
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  int i;
  clock++;

  //updates the cache if the data is in the cache
  for(i=0; i<cache_size; i++){
    if(cache[i].disk_num == disk_num){
      if(cache[i].block_num == block_num){
        int j;
        for(j=0; j<JBOD_BLOCK_SIZE; j++){
          cache[i].block[j] = buf[j];
        }
        cache[i].access_time = clock;
      }
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  int i;
  int least = 0;
  clock++;
  if(cache==NULL){
    return -1;
  }
  if(buf == NULL){
    return -1;
  }
  if(disk_num<0 || disk_num>16){
    return -1;
  }
  if(block_num<0 || block_num>256){
    return -1;
  }
  
  num_queries++;

  //checks to see if the item is already in the cache
  int q;
  for(q=0; q<cache_size; q++){
    if(cache[q].block_num==block_num && cache[q].disk_num==disk_num){
      if(cache[q].valid==true){
        return -1;
      }
    }
  }

  //finds the lru item in the cache
  for(i=0; i<cache_size; i++){

    if(cache[least].access_time > cache[i].access_time){
      least = i;
    }
  }  
  int j;

  //updates cache item data
  cache[least].block_num=block_num;
  cache[least].disk_num=disk_num;
  cache[least].valid = true;

  //inserts the data from the block to the cache
  for(j=0; j<JBOD_BLOCK_SIZE; j++){
    cache[least].block[j] = buf[j];
  }
  cache[least].access_time = clock;
  return 1;
}

bool cache_enabled(void) {
  if(cache_size <= 2){
    return false;
  }
  return true;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
