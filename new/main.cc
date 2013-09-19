/**
 * Copyright (C) 2005-2013 Christoph Rupp (chris@crupp.de).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 *
 * See files COPYING.* for License information.
 */


#include <iostream>
#include <cstdio>
#include <ctime>

#include <boost/filesystem.hpp>
#include <ham/hamsterdb.h>

#include "getopts.h"
#include "configuration.h"
#include "datasource.h"
#include "datasource_numeric.h"
#include "datasource_binary.h"
#include "generator_runtime.h"
#include "hamsterdb.h"
#include "berkeleydb.h"
#include "metrics.h"

#define ARG_HELP                    1
#define ARG_VERBOSE                 2
#define ARG_QUIET                   3
#define ARG_NO_PROGRESS             4
#define ARG_REOPEN                  5
#define ARG_METRICS                 6
#define ARG_KEYSIZE_BTREE           7
#define ARG_INMEMORY                10
#define ARG_OVERWRITE               11
#define ARG_DISABLE_MMAP            12
#define ARG_PAGESIZE                13
#define ARG_KEYSIZE                 14
#define ARG_KEYSIZE_FIXED           15
#define ARG_RECSIZE                 16
#define ARG_CACHE                   17
#define ARG_USE_CURSORS             23
#define ARG_KEY                     24
#define ARG_REC                     25
#define ARG_DUPLICATE               26
#define ARG_FULLCHECK               27
#define ARG_RECOVERY                34
#define ARG_HINTING                 37
#define ARG_DIRECT_ACCESS           39
#define ARG_USE_TRANSACTIONS        41
#define ARG_USE_FSYNC               42
#define ARG_USE_BERKELEYDB          43
#define ARG_USE_HAMSTERDB           47
#define ARG_NUM_THREADS             44
#define ARG_ENABLE_ENCRYPTION       45
#define ARG_USE_REMOTE              46
#define ARG_ERASE_PCT               48
#define ARG_FIND_PCT                49
#define ARG_STOP_TIME               50
#define ARG_STOP_OPS                51
#define ARG_STOP_BYTES              52
#define ARG_TEE                     53
#define ARG_SEED                    54
#define ARG_DISTRIBUTION            55

/*
 * command line parameters
 */
static option_t opts[] = {
  { 
    ARG_HELP,         // symbolic name of this option
    "h",          // short option 
    "help",         // long option 
    "Prints this help screen",   // help string
    0 },          // no flags
  {
    ARG_VERBOSE,
    "v",
    "verbose",
    "Prints verbose information",
    0 },
  {
    ARG_QUIET,
    "q",
    "quiet",
    "Does not print profiling metrics",
    0 },
  {
    ARG_NO_PROGRESS,
    0,
    "no-progress",
    "Disables the progress bar",
    0 },
  {
    ARG_REOPEN,
    "r",
    "reopen",
    "Calls OPEN/FULLCHECK/CLOSE after each close",
    0 },
  {
    ARG_METRICS,
    0,
    "metrics",
    "Prints metrics and statistics ('none', 'default', 'all)",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_TEE,
    0,
    "tee",
    "Copies the generated test data into the specified file",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_SEED,
    0,
    "seed",
    "Sets the seed for the random number generator",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_DISTRIBUTION,
    0,
    "distribution",
    "Sets the distribution of the key values ('random', 'ascending',\n"
            "\t'descending', 'zipfian'",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_INMEMORY,
    0,
    "inmemorydb",
    "Creates in-memory-databases",
    0 },
  {
    ARG_OVERWRITE,
    0,
    "overwrite",
    "Overwrite existing keys",
    0 },
  {
    ARG_DUPLICATE,
    0,
    "duplicate",
    "Enables duplicate keys ('first': inserts them at the beginning;\n"
            "\t'last': inserts at the end (default))",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_USE_CURSORS,
    0,
    "use-cursors",
    "use cursors for insert/erase",
    0 },
  {
    ARG_RECOVERY,
    0,
    "use-recovery",
    "Uses recovery",
    0 },
  {
    ARG_KEY,
    0,
    "key",
    "Describes the key type ('uint16', 'uint32', 'uint64', 'binary' (default))",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_REC,
    0,
    "record",
    "Describes the record type ('fixed' or 'variable' (default))",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_DISABLE_MMAP,
    0,
    "no-mmap",
    "Disables memory mapped I/O",
    0 },
  {
    ARG_FULLCHECK,
    0,
    "fullcheck",
    "Sets 'fullcheck' algorithm ('find' uses ham_db_find,\n"
            "\t'reverse' searches backwards, leave empty for default)",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_PAGESIZE,
    0,
    "pagesize",
    "Sets the pagesize (use 0 for default)",
    0 },
  {
    ARG_KEYSIZE,
    0,
    "keysize",
    "Sets the key size (use 0 for default)",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_KEYSIZE_BTREE,
    0,
    "btree-keysize",
    "Sets the key size of the btree; if < --keysize: extended keys are enabled."
            "\n\tif not specified: will use same size as for --keysize",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_KEYSIZE_FIXED,
    0,
    "keysize-fixed",
    "Forces a fixed key size; default behavior depends on --keytype",
    0 },
  {
    ARG_RECSIZE,
    0,
    "recsize",
    "Sets the record size (default is 1024)",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_CACHE,
    0,
    "cache",
    "Sets the cachesize (use 0 for default) or 'unlimited'",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_HINTING,
    0,
    "hints",
    "sets hinting flags - one of:\n"
    "\tHAM_HINT_APPEND, HAM_HINT_PREPEND",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_DIRECT_ACCESS,
    0,
    "direct-access",
    "sets HAM_DIRECT_ACCESS flag",
    0 },
  {
    ARG_USE_TRANSACTIONS,
    0,
    "use-transactions",
    "use Transactions; arguments are \n"
    "\t'tmp' - create temp. Transactions;\n"
    "\tN - (number) group N statements into a Transaction;\n"
    "\t'all' - group the whole test into a single Transaction",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_USE_FSYNC,
    0,
    "use-fsync",
    "Calls fsync() when flushing to disk",
    0 },
  {
    ARG_USE_BERKELEYDB,
    0,
    "use-berkeleydb",
    "Enables use of berkeleydb ('true', 'false' (default))",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_USE_HAMSTERDB,
    0,
    "use-hamsterdb",
    "Enables use of hamsterdb ('true' (default), 'false')",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_NUM_THREADS,
    0,
    "num-threads",
    "sets the number of threads (default: 1)",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_ERASE_PCT,
    0,
    "erase-pct",
    "Percentage of erase calls (default: 0)",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_FIND_PCT,
    0,
    "find-pct",
    "Percentage of lookup calls (default: 0)",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_STOP_TIME,
    0,
    "stop-seconds",
    "Stops test after specified duration, in seconds",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_STOP_OPS,
    0,
    "stop-ops",
    "Stops test after executing specified number of operations",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_STOP_BYTES,
    0,
    "stop-bytes",
    "Stops test after inserting specified number of bytes (default: 100 mb)",
    GETOPTS_NEED_ARGUMENT },
  {
    ARG_USE_REMOTE,
    0,
    "use-remote",
    "Runs test in remote client/server scenario",
    0 },
  { 0, 0, 0, 0, 0 }
};

static void
parse_config(int argc, char **argv, Configuration *c)
{
  unsigned opt;
  char *param;
	
  getopts_init(argc, argv, "test");

  /*
   * parse command line parameters
   */
  while ((opt = getopts(&opts[0], &param))) {
    if (opt == ARG_HELP) {
      getopts_usage(&opts[0]);
      exit(0);
    }
    else if (opt == ARG_QUIET) {
      c->quiet = true;
    }
    else if (opt == ARG_VERBOSE) {
      c->verbose++;
    }
    else if (opt == ARG_INMEMORY) {
      c->inmemory = true;
    }
    else if (opt == ARG_DISTRIBUTION) {
      if (param && !strcmp(param, "random"))
        c->distribution = Configuration::kDistributionRandom;
      else if (param && !strcmp(param, "ascending"))
        c->distribution = Configuration::kDistributionAscending;
      else if (param && !strcmp(param, "descending"))
        c->distribution = Configuration::kDistributionDescending;
      else if (param && !strcmp(param, "zipfian"))
        c->distribution = Configuration::kDistributionZipfian;
      else {
        printf("[FAIL] invalid parameter for --distribution\n");
        exit(-1);
      }
    }
    else if (opt == ARG_OVERWRITE) {
      if (c->duplicate) {
        printf("[FAIL] invalid combination: overwrite && duplicate\n");
        exit(-1);
      }
      c->overwrite = true;
    }
    else if (opt == ARG_DUPLICATE) {
      if (c->overwrite) {
        printf("[FAIL] invalid combination: overwrite && duplicate\n");
        exit(-1);
      }
      if (param && !strcmp(param, "first"))
        c->duplicate = Configuration::kDuplicateFirst;
      else if ((param && !strcmp(param, "last")) || !param)
        c->duplicate = Configuration::kDuplicateLast;
      else {
        printf("[FAIL] invalid parameter for 'duplicate'\n");
        exit(-1);
      }
    }
    else if (opt == ARG_USE_CURSORS) {
      c->use_cursors = true;
    }
    else if (opt == ARG_RECOVERY) {
      c->use_recovery = true;
    }
    else if (opt == ARG_KEY) {
      if (param && !strcmp(param, "uint8"))
        c->key_type = Configuration::kKeyUint8;
      else if (param && !strcmp(param, "uint16"))
        c->key_type = Configuration::kKeyUint16;
      else if (param && !strcmp(param, "uint32"))
        c->key_type = Configuration::kKeyUint32;
      else if (param && !strcmp(param, "uint64"))
        c->key_type = Configuration::kKeyUint64;
      else if (param && strcmp(param, "binary")) {
        printf("invalid parameter for --key\n");
        exit(-1);
      }
    }
    else if (opt == ARG_REC) {
      if (param && !strcmp(param, "fixed"))
        c->record_type = Configuration::kRecordFixed;
      else if (param && strcmp(param, "variable")) {
        printf("[FAIL] invalid parameter for --record\n");
        exit(-1);
      }
    }
    else if (opt == ARG_NO_PROGRESS) {
      c->no_progress = true;
    }
    else if (opt == ARG_DISABLE_MMAP) {
      c->no_mmap = true;
    }
    else if (opt == ARG_PAGESIZE) {
      c->pagesize = strtoul(param, 0, 0);
    }
    else if (opt == ARG_KEYSIZE) {
      c->key_size = strtoul(param, 0, 0);
    }
    else if (opt == ARG_KEYSIZE_BTREE) {
      c->btree_key_size = strtoul(param, 0, 0);
    }
    else if (opt == ARG_KEYSIZE_FIXED) {
      c->key_is_fixed_size = true;
    }
    else if (opt == ARG_RECSIZE) {
      c->rec_size = strtoul(param, 0, 0);
    }
    else if (opt == ARG_CACHE) {
      if (strstr(param, "unlimited"))
        c->cacheunlimited = true;
      else
        c->cachesize = strtoul(param, 0, 0);
    }
    else if (opt == ARG_HINTING) {
      if (strstr(param, "HAM_HINT_APPEND"))
        c->hints |= HAM_HINT_APPEND;
      if (strstr(param, "HAM_HINT_PREPEND"))
        c->hints |= HAM_HINT_PREPEND;
      if (param && !c->hints) {
        printf("[FAIL] invalid or missing parameter for 'hints'\n");
        exit(-1);
      }
    }
    else if (opt == ARG_DIRECT_ACCESS) {
      c->direct_access = true;
    }
    else if (opt == ARG_USE_FSYNC) {
      c->use_fsync = true;
    }
    else if (opt == ARG_USE_BERKELEYDB) {
      if (!param || !strcmp(param, "true"))
        c->use_berkeleydb = true;
      else if (param && !strcmp(param, "false"))
        c->use_berkeleydb = false;
      else {
        printf("[FAIL] invalid or missing parameter for 'use-berkeleydb'\n");
        exit(-1);
      }
    }
    else if (opt == ARG_USE_HAMSTERDB) {
      if (!param || !strcmp(param, "true"))
        c->use_hamsterdb = true;
      else if (param && !strcmp(param, "false"))
        c->use_hamsterdb = false;
      else {
        printf("[FAIL] invalid or missing parameter for 'use-hamsterdb'\n");
        exit(-1);
      }
    }
    else if (opt == ARG_USE_TRANSACTIONS) {
      c->use_transactions = true;
      if (strcmp("tmp", param) == 0)
        c->transactions_nth = 0;
      else if (strcmp("all", param) == 0)
        c->transactions_nth = 0xffffffff;
      else {
        c->transactions_nth = strtoul(param, 0, 0);
        if (!c->transactions_nth) {
          printf("[FAIL] invalid parameter for 'use-transactions'\n");
          exit(-1);
        }
      }
    }
    else if (opt == ARG_REOPEN) {
      c->reopen = true;
    }
    else if (opt == ARG_METRICS) {
      if (param && !strcmp(param, "none"))
        c->metrics = Configuration::kMetricsNone;
      else if (param && !strcmp(param, "all"))
        c->metrics = Configuration::kMetricsAll;
      else if (param && strcmp(param, "default")) {
        printf("[FAIL] invalid parameter for '--metrics'\n");
        exit(-1);
      }
    }
    else if (opt == ARG_TEE) {
      c->tee_file = param;
    }
    else if (opt == ARG_SEED) {
      c->seed = strtoul(param, 0, 0);
    }
    else if (opt == ARG_FULLCHECK) {
      if (param && !strcmp(param, "find"))
        c->fullcheck = Configuration::kFullcheckFind;
      else if (param && !strcmp(param, "reverse"))
        c->fullcheck = Configuration::kFullcheckReverse;
      else if (param && strcmp(param, "forward")) {
        printf("[FAIL] invalid parameter for --fullcheck\n");
        exit(-1);
      }
    }
    else if (opt == ARG_ERASE_PCT) {
      c->erase_pct = strtoul(param, 0, 0);
      if (!c->erase_pct || c->erase_pct > 100) {
        printf("[FAIL] invalid parameter for 'erase-pct'\n");
        exit(-1);
      }
    }
    else if (opt == ARG_FIND_PCT) {
      c->find_pct = strtoul(param, 0, 0);
      if (!c->find_pct || c->find_pct > 100) {
        printf("[FAIL] invalid parameter for 'find-pct'\n");
        exit(-1);
      }
    }
    else if (opt == ARG_STOP_TIME) {
      c->limit_seconds = strtoul(param, 0, 0);
      if (!c->limit_seconds) {
        printf("[FAIL] invalid parameter for 'stop-seconds'\n");
        exit(-1);
      }
    }
    else if (opt == ARG_STOP_BYTES) {
      c->limit_bytes = strtoul(param, 0, 0);
      if (!c->limit_bytes) {
        printf("[FAIL] invalid parameter for 'stop-bytes'\n");
        exit(-1);
      }
    }
    else if (opt == ARG_STOP_OPS) {
      c->limit_ops = strtoul(param, 0, 0);
      if (!c->limit_ops) {
        printf("[FAIL] invalid parameter for 'stop-ops'\n");
        exit(-1);
      }
    }
    else if (opt == ARG_NUM_THREADS) {
      c->num_threads = strtoul(param, 0, 0);
      if (!c->num_threads) {
        printf("[FAIL] invalid parameter for 'num-threads'\n");
        exit(-1);
      }
    }
    else if (opt == ARG_ENABLE_ENCRYPTION) {
      c->use_encryption = true;
    }
    else if (opt == ARG_USE_REMOTE) {
      c->use_remote = true;
    }
    else if (opt == GETOPTS_PARAMETER) {
      c->filename = param;
    }
    else {
      printf("[FAIL] unknown parameter '%s'\n", param);
      exit(-1);
    }
  }

  if (c->duplicate == Configuration::kDuplicateFirst && !c->use_cursors) {
    printf("[FAIL] '--duplicate=first' needs 'use-cursors'\n");
    exit(-1);
  }

  if (c->btree_key_size == 0)
    c->btree_key_size = c->key_size;

  if (c->verbose && c->metrics == Configuration::kMetricsDefault)
    c->metrics = Configuration::kMetricsAll;
}

static void
print_metrics(Metrics *metrics, Configuration *conf)
{
  printf("\telapsed time (sec)             %f\n",
          metrics->elapsed_wallclock_seconds);
  printf("\ttotal #ops                     %lu\n",
                  metrics->insert_ops + metrics->erase_ops
                  + metrics->find_ops + metrics->txn_commit_ops
                  + metrics->other_ops);
  printf("\tinsert #ops                    %lu (%f/sec)\n",
                  metrics->insert_ops,
                  (double)metrics->insert_ops / metrics->insert_latency_total);
  printf("\tinsert throughput              %f/sec\n",
                  (double)metrics->insert_bytes / metrics->insert_latency_total);
  printf("\tinsert latency (min, avg, max) %f, %f, %f\n",
                  metrics->insert_latency_min,
                  metrics->insert_latency_total / metrics->insert_ops,
                  metrics->insert_latency_max);
  if (metrics->find_ops) {
    printf("\tfind #ops                      %lu (%f/sec)\n",
                  metrics->find_ops,
                  (double)metrics->find_ops / metrics->find_latency_total);
    printf("\tfind throughput                %f/sec\n",
                  (double)metrics->find_bytes / metrics->find_latency_total);
    printf("\tfind latency (min, avg, max)   %f, %f, %f\n",
                  metrics->find_latency_min,
                  metrics->find_latency_total / metrics->find_ops,
                  metrics->find_latency_max);
  }
  if (metrics->erase_ops) {
    printf("\terase #ops                     %lu (%f/sec)\n",
                  metrics->erase_ops,
                  (double)metrics->erase_ops / metrics->erase_latency_total);
    printf("\terase latency (min, avg, max)  %f, %f, %f\n",
                  metrics->erase_latency_min,
                  metrics->erase_latency_total / metrics->erase_ops,
                  metrics->erase_latency_max);
  }
  if (conf->use_hamsterdb)
    printf("\thamsterdb filesize             %lu\n",
                  boost::filesystem::file_size("test-ham.db"));
  if (conf->use_berkeleydb)
    printf("\tberkeleydb filesize            %lu\n",
                  boost::filesystem::file_size("test-berk.db"));

  if (conf->metrics != Configuration::kMetricsAll)
    return;

  printf("\thamsterdb mem_total_allocations       %lu\n",
          metrics->hamster_metrics.mem_total_allocations);
  printf("\thamsterdb mem_current_usage           %lu\n",
          metrics->hamster_metrics.mem_current_usage);
  printf("\thamsterdb mem_peak_usage              %lu\n",
          metrics->hamster_metrics.mem_peak_usage);
  printf("\thamsterdb page_count_fetched          %lu\n",
          metrics->hamster_metrics.page_count_fetched);
  printf("\thamsterdb page_count_flushed          %lu\n",
          metrics->hamster_metrics.page_count_flushed);
  printf("\thamsterdb page_count_type_index       %lu\n",
          metrics->hamster_metrics.page_count_type_index);
  printf("\thamsterdb page_count_type_blob        %lu\n",
          metrics->hamster_metrics.page_count_type_blob);
  printf("\thamsterdb page_count_type_freelist    %lu\n",
          metrics->hamster_metrics.page_count_type_freelist);
  printf("\thamsterdb freelist_hits               %lu\n",
          metrics->hamster_metrics.freelist_hits);
  printf("\thamsterdb freelist_misses             %lu\n",
          metrics->hamster_metrics.freelist_misses);
  printf("\thamsterdb cache_hits                  %lu\n",
          metrics->hamster_metrics.cache_hits);
  printf("\thamsterdb cache_misses                %lu\n",
          metrics->hamster_metrics.cache_misses);
  printf("\thamsterdb blob_total_allocated        %lu\n",
          metrics->hamster_metrics.blob_total_allocated);
  printf("\thamsterdb blob_total_read             %lu\n",
          metrics->hamster_metrics.blob_total_read);
  printf("\thamsterdb blob_direct_read            %lu\n",
          metrics->hamster_metrics.blob_direct_read);
  printf("\thamsterdb blob_direct_written         %lu\n",
          metrics->hamster_metrics.blob_direct_written);
  printf("\thamsterdb blob_direct_allocated       %lu\n",
          metrics->hamster_metrics.blob_direct_allocated);
  printf("\thamsterdb extkey_cache_hits           %lu\n",
          metrics->hamster_metrics.extkey_cache_hits);
  printf("\thamsterdb extkey_cache_misses         %lu\n",
          metrics->hamster_metrics.extkey_cache_misses);
  printf("\thamsterdb btree_smo_split             %lu\n",
          metrics->hamster_metrics.btree_smo_split);
  printf("\thamsterdb btree_smo_merge             %lu\n",
          metrics->hamster_metrics.btree_smo_merge);
  printf("\thamsterdb btree_smo_shift             %lu\n",
          metrics->hamster_metrics.btree_smo_shift);
}

int
main(int argc, char **argv)
{
  Configuration c;
  parse_config(argc, argv, &c);

  // ALWAYS set the seed!
  if (c.seed == 0)
    c.seed = ::time(0);

  // set a limit
  if (!c.limit_bytes && !c.limit_seconds && !c.limit_ops)
    c.limit_bytes = 100 * 1024 * 1024;

  // ALWAYS dump the configuration
  c.print();

  bool ok = true;

  // if berkeleydb is disabled, and hamsterdb runs in only one thread:
  // just execute the test single-threaded
  if (c.use_hamsterdb && !c.use_berkeleydb) {
    Database *db = new HamsterDatabase(0, &c);
    db->create_env();
    RuntimeGenerator generator(&c, true, db);
    while (generator.execute())
      ;
    // have to collect metrics now while the database was not yet closed
    Metrics metrics;
    generator.get_metrics(&metrics);
    if (c.reopen) {
      db->close_env();
      db->open_env();
      generator.open();
      generator.close();
    }
    db->close_env();
    delete db;

    ok = generator.was_successful();

    if (ok) {
      printf("[OK] %s\n", c.filename.c_str());
      if (!c.quiet && c.metrics != Configuration::kMetricsNone)
        print_metrics(&metrics, &c);
    }
    else
      printf("[FAIL] %s\n", c.filename.c_str());
  }
  else if (c.use_berkeleydb && !c.use_hamsterdb) {
    Database *db = new BerkeleyDatabase(0, &c);
    db->create_env();
    RuntimeGenerator generator(&c, true, db);
    while (generator.execute())
      ;
    // have to collect metrics now while the database was not yet closed
    Metrics metrics;
    generator.get_metrics(&metrics);
    if (c.reopen) {
      db->close_env();
      db->open_env();
      generator.open();
      generator.close();
    }
    db->close_env();
    delete db;

    ok = generator.was_successful();

    if (ok) {
      printf("[OK] %s\n", c.filename.c_str());
      if (!c.quiet && c.metrics != Configuration::kMetricsNone)
        print_metrics(&metrics, &c);
    }
    else
      printf("[FAIL] %s\n", c.filename.c_str());
  }

#if 0
  //NumericDescendingDatasource<unsigned short> ds;
  //NumericRandomDatasource<unsigned short> ds;
  //NumericZipfianDatasource<unsigned int> ds(100);
  //BinaryRandomDatasource ds(5, false);
  //BinaryZipfianDatasource ds(100, 5, false);
  //std::vector<unsigned char> t;
  RuntimeGenerator generator(&c);
  while (generator.execute(0)) {
    //t.clear();
    //std::cout.width(2);
    //std::cout.fill('0');
    //ds.get_next(t);
    //for (size_t j = 0; j < t.size(); j++)
      //printf("0x%02x ", t[j]);
    //std::cout << std::endl;
  }

  if (c.reopen) {
    generator.open(0);
    generator.close(0);
  }
#endif

  return (ok ? 0 : 1);
}