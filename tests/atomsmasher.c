/*
  Sample test application.
*/
#include <assert.h>
#include <memcached.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "server.h"
#include "../lib/common.h"
#include "../src/generator.h"
#include "../src/execute.h"

#ifndef INT64_MAX
#define INT64_MAX LONG_MAX
#endif
#ifndef INT32_MAX
#define INT32_MAX INT_MAX
#endif


#include "test.h"

#define GLOBAL_COUNT 100000
#define TEST_COUNTER 100000
static uint32_t global_count;

static pairs_st *global_pairs;
static char *global_keys[GLOBAL_COUNT];
static size_t global_keys_length[GLOBAL_COUNT];

uint8_t cleanup_pairs(memcached_st *memc)
{
  pairs_free(global_pairs);

  return 0;
}

uint8_t generate_pairs(memcached_st *memc)
{
  unsigned long long x;
  global_pairs= pairs_generate(GLOBAL_COUNT, 400);
  global_count= GLOBAL_COUNT;

  for (x= 0; x < global_count; x++)
  {
    global_keys[x]= global_pairs[x].key; 
    global_keys_length[x]=  global_pairs[x].key_length;
  }

  return 0;
}

uint8_t drizzle(memcached_st *memc)
{
  unsigned int x;
  memcached_return rc;

  {
    char *return_value;
    size_t return_value_length;
    uint32_t flags;

    for (x= 0; x < TEST_COUNTER; x++)
    {
      uint32_t test_bit;
      uint8_t which;

      test_bit= random() % GLOBAL_COUNT;
      which= random() % 2;

      if (which == 0)
      {
        return_value= memcached_get(memc, global_keys[test_bit], global_keys_length[test_bit],
                                    &return_value_length, &flags, &rc);
        if (rc == MEMCACHED_SUCCESS && return_value)
          free(return_value);
        else
          WATCHPOINT_ERROR(rc);
      } 
      else
      {
        rc= memcached_set(memc, global_pairs[test_bit].key, 
                          global_pairs[test_bit].key_length,
                          global_pairs[test_bit].value, 
                          global_pairs[test_bit].value_length,
                          0, 0);
        if (rc != MEMCACHED_SUCCESS && rc != MEMCACHED_BUFFERED)
        {
          WATCHPOINT_ERROR(rc);
          WATCHPOINT_ASSERT(0);
        }
      }
    }
  }

  return 0;
}

memcached_return pre_nonblock(memcached_st *memc)
{
  memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_NO_BLOCK, NULL);

  return MEMCACHED_SUCCESS;
}

memcached_return pre_md5(memcached_st *memc)
{
  memcached_hash value= MEMCACHED_HASH_MD5;
  memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_HASH, &value);

  return MEMCACHED_SUCCESS;
}

memcached_return pre_hsieh(memcached_st *memc)
{
  memcached_hash value= MEMCACHED_HASH_HSIEH;
  memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_HASH, &value);

  return MEMCACHED_SUCCESS;
}

memcached_return enable_consistent(memcached_st *memc)
{
  memcached_server_distribution value= MEMCACHED_DISTRIBUTION_CONSISTENT;
  memcached_hash hash;
  memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_DISTRIBUTION, &value);
  pre_hsieh(memc);

  value= (memcached_server_distribution)memcached_behavior_get(memc, MEMCACHED_BEHAVIOR_DISTRIBUTION);
  assert(value == MEMCACHED_DISTRIBUTION_CONSISTENT);

  hash= (memcached_hash)memcached_behavior_get(memc, MEMCACHED_BEHAVIOR_HASH);
  assert(hash == MEMCACHED_HASH_HSIEH);


  return MEMCACHED_SUCCESS;
}

test_st generate_tests[] ={
  {"generate_pairs", 1, generate_pairs },
  {"cleanup", 1, cleanup_pairs },
  {0, 0, 0}
};


collection_st collection[] ={
  {"generate", 0, 0, generate_tests},
  {"generate_hsieh", pre_hsieh, 0, generate_tests},
  {"generate_hsieh_consistent", enable_consistent, 0, generate_tests},
  {"generate_md5", pre_md5, 0, generate_tests},
  {"generate_nonblock", pre_nonblock, 0, generate_tests},
  {0, 0, 0, 0}
};

#define SERVERS_TO_CREATE 5

void *world_create(void)
{
  server_startup_st *construct;

  construct= (server_startup_st *)malloc(sizeof(server_startup_st));
  memset(construct, 0, sizeof(server_startup_st));
  construct->count= SERVERS_TO_CREATE;
  construct->udp= 0;
  server_startup(construct);

  return construct;
}

void world_destroy(void *p)
{
  server_startup_st *construct= (server_startup_st *)p;
  memcached_server_st *servers= (memcached_server_st *)construct->servers;
  memcached_server_list_free(servers);

  server_shutdown(construct);
  free(construct);
}

void get_world(world_st *world)
{
  world->collections= collection;
  world->create= world_create;
  world->destroy= world_destroy;
}
