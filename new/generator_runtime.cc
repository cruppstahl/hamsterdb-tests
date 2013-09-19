﻿/**
 * Copyright (C) 2005-2013 Christoph Rupp (chris@crupp.de).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 *
 * See files COPYING.* for License information.
 */

#include <cassert>

#include "configuration.h"
#include "datasource_numeric.h"
#include "datasource_binary.h"
#include "generator_runtime.h"

#define kZipfianLimit       (1024 * 1024 * 5)

RuntimeGenerator::RuntimeGenerator(Configuration *conf, bool show_progress,
                Database *db)
  : Generator(conf), m_db(db), m_state(0), m_opcount(0),
    m_datasource(0), m_u01(m_rng), m_elapsed_seconds(0.0), m_txn(0),
    m_cursor(0), m_progress(0), m_success(true)
{
  if (conf->seed)
    m_rng.seed(conf->seed);

  memset(&m_metrics, 0, sizeof(m_metrics));
  m_metrics.insert_latency_min = 9999999.99;
  m_metrics.erase_latency_min = 9999999.99;
  m_metrics.find_latency_min = 9999999.99;
  m_metrics.txn_commit_latency_min = 9999999.99;

  if (show_progress) {
    if (!conf->no_progress && !conf->quiet && !conf->verbose)
      m_progress = new boost::progress_display(std::max(
                              std::max(conf->limit_bytes, conf->limit_ops),
                              conf->limit_seconds));
  }

  if (!m_conf->tee_file.empty())
    m_tee.open(m_conf->tee_file.c_str(), std::ios::out);

  switch (conf->key_type) {
    case Configuration::kKeyUint8:
      switch (conf->distribution) {
        case Configuration::kDistributionRandom:
          m_datasource = new NumericRandomDatasource<uint8_t>(conf->seed);
          break;
        case Configuration::kDistributionAscending:
          m_datasource = new NumericAscendingDatasource<uint8_t>();
          break;
        case Configuration::kDistributionDescending:
          m_datasource = new NumericDescendingDatasource<uint8_t>();
          break;
        case Configuration::kDistributionZipfian:
          m_datasource = new NumericZipfianDatasource<uint8_t>(
                          conf->limit_ops ? conf->limit_ops : kZipfianLimit,
                          conf->seed);
          break;
      }
      break;
    case Configuration::kKeyUint16:
      switch (conf->distribution) {
        case Configuration::kDistributionRandom:
          m_datasource = new NumericRandomDatasource<uint16_t>(conf->seed);
          break;
        case Configuration::kDistributionAscending:
          m_datasource = new NumericAscendingDatasource<uint16_t>();
          break;
        case Configuration::kDistributionDescending:
          m_datasource = new NumericDescendingDatasource<uint16_t>();
          break;
        case Configuration::kDistributionZipfian:
          m_datasource = new NumericZipfianDatasource<uint8_t>(
                          conf->limit_ops ? conf->limit_ops : kZipfianLimit,
                          conf->seed);
          break;
      }
      break;
    case Configuration::kKeyUint32:
      switch (conf->distribution) {
        case Configuration::kDistributionRandom:
          m_datasource = new NumericRandomDatasource<uint32_t>(conf->seed);
          break;
        case Configuration::kDistributionAscending:
          m_datasource = new NumericAscendingDatasource<uint32_t>();
          break;
        case Configuration::kDistributionDescending:
          m_datasource = new NumericDescendingDatasource<uint32_t>();
          break;
        case Configuration::kDistributionZipfian:
          m_datasource = new NumericZipfianDatasource<uint8_t>(
                          conf->limit_ops ? conf->limit_ops : kZipfianLimit,
                          conf->seed);
          break;
      }
      break;
    case Configuration::kKeyUint64:
      switch (conf->distribution) {
        case Configuration::kDistributionRandom:
          m_datasource = new NumericRandomDatasource<uint64_t>(conf->seed);
          break;
        case Configuration::kDistributionAscending:
          m_datasource = new NumericAscendingDatasource<uint64_t>();
          break;
        case Configuration::kDistributionDescending:
          m_datasource = new NumericDescendingDatasource<uint64_t>();
          break;
        case Configuration::kDistributionZipfian:
          m_datasource = new NumericZipfianDatasource<uint8_t>(
                          conf->limit_ops ? conf->limit_ops : kZipfianLimit,
                          conf->seed);
          break;
      }
      break;
    case Configuration::kKeyBinary:
      switch (conf->distribution) {
        case Configuration::kDistributionRandom:
          m_datasource = new BinaryRandomDatasource(conf->key_size,
                          conf->key_is_fixed_size, conf->seed);
          break;
        case Configuration::kDistributionAscending:
          m_datasource = new BinaryAscendingDatasource(conf->key_size,
                          conf->key_is_fixed_size);
          break;
        case Configuration::kDistributionDescending:
          m_datasource = new BinaryDescendingDatasource(conf->key_size,
                          conf->key_is_fixed_size);
          break;
        case Configuration::kDistributionZipfian:
          m_datasource = new BinaryZipfianDatasource(
                          conf->limit_ops ? conf->limit_ops : kZipfianLimit,
                          conf->key_size, conf->key_is_fixed_size, conf->seed);
          break;
      }
      break;
  }
  assert(m_datasource != 0);
}

bool
RuntimeGenerator::execute()
{
  if (m_state == kStateStopped)
    return (false);

  int cmd = get_next_command();
  switch (cmd) {
    case Generator::kCommandCreate:
      create();
      break;
    case Generator::kCommandOpen:
      open();
      break;
    case Generator::kCommandClose:
      close();
      break;
    case Generator::kCommandInsert:
      insert();
      break;
    case Generator::kCommandErase:
      erase();
      break;
    case Generator::kCommandFind:
      find();
      break;
    case Generator::kCommandBeginTransaction:
      txn_begin();
      break;
    case Generator::kCommandAbortTransaction:
      txn_abort();
      break;
    case Generator::kCommandCommitTransaction:
      txn_commit();
      break;
    default:
      assert(!"shouldn't be here");
  }

  m_opcount++;

  if (m_progress && m_conf->limit_ops)
    (*m_progress) += 1;

  return (true);
}

void
RuntimeGenerator::create()
{
  tee("CREATE");
  m_last_status = m_db->create_db();
  
  if (m_conf->use_cursors)
    m_cursor = m_db->cursor_create(m_txn);

  if (m_last_status != 0)
    m_success = false;

  m_metrics.other_ops++;
}

void
RuntimeGenerator::open()
{
  tee("OPEN");
  m_last_status = m_db->open_db();

  if (m_conf->use_cursors)
    m_cursor = m_db->cursor_create(m_txn);

  if (m_last_status != 0)
    m_success = false;

  m_metrics.other_ops++;
}
 
void
RuntimeGenerator::close()
 {
  tee("CLOSE");
  if (m_cursor) {
    m_db->cursor_close(m_cursor);
    m_cursor = 0;
  }

  m_last_status = m_db->close_db();

  if (m_last_status != 0)
    m_success = false;

  m_metrics.other_ops++;
  m_metrics.elapsed_wallclock_seconds = m_start.seconds();
}

void
RuntimeGenerator::insert()
{
  ham_key_t key = generate_key();
  ham_record_t rec = generate_record();

  tee("INSERT", &key, &rec);

  Timer<boost::chrono::high_resolution_clock> t;

  if (m_cursor)
    m_last_status = m_db->cursor_insert(m_cursor, &key, &rec);
  else
    m_last_status = m_db->insert(m_txn, &key, &rec);

  double elapsed = t.seconds();
  if (m_metrics.insert_latency_min > elapsed)
    m_metrics.insert_latency_min = elapsed;
  if (m_metrics.insert_latency_max < elapsed)
    m_metrics.insert_latency_max = elapsed;
  m_metrics.insert_latency_total += elapsed;

  if (m_last_status != 0 && m_last_status != HAM_DUPLICATE_KEY)
    m_success = false;

  if (m_last_status == 0) {
    m_metrics.insert_bytes += key.size + rec.size;
    if (m_progress && m_conf->limit_bytes)
      (*m_progress) += key.size + rec.size;
  }

  m_metrics.insert_ops++;
}

void
RuntimeGenerator::erase()
{
  ham_key_t key = generate_key();

  tee("ERASE", &key);

  Timer<boost::chrono::high_resolution_clock> t;

  if (m_cursor)
    m_last_status = m_db->cursor_erase(m_cursor, &key);
  else
    m_last_status = m_db->erase(m_txn, &key);

  double elapsed = t.seconds();
  if (m_metrics.erase_latency_min > elapsed)
    m_metrics.erase_latency_min = elapsed;
  if (m_metrics.erase_latency_max < elapsed)
    m_metrics.erase_latency_max = elapsed;
  m_metrics.erase_latency_total += elapsed;

  if (m_last_status != 0 && m_last_status != HAM_KEY_NOT_FOUND)
    m_success = false;

  m_metrics.erase_ops++;
}

void
RuntimeGenerator::find()
{
  ham_key_t key = generate_key();
  ham_record_t rec = {0};

  tee("FIND", &key);

  Timer<boost::chrono::high_resolution_clock> t;

  if (m_cursor)
    m_last_status = m_db->find(m_cursor, &key, &rec);
  else
    m_last_status = m_db->find(m_txn, &key, &rec);

  double elapsed = t.seconds();
  if (m_metrics.find_latency_min > elapsed)
    m_metrics.find_latency_min = elapsed;
  if (m_metrics.find_latency_max < elapsed)
    m_metrics.find_latency_max = elapsed;
  m_metrics.find_latency_total += elapsed;

  if (m_last_status != 0 && m_last_status != HAM_KEY_NOT_FOUND)
    m_success = false;

  m_metrics.find_bytes += rec.size;
  m_metrics.find_ops++;
}

void
RuntimeGenerator::txn_begin()
{
  tee("TXN_BEGIN");
  assert(m_txn == 0);

  if (m_cursor) {
    m_db->cursor_close(m_cursor);
    m_cursor = 0;
  }

  m_txn = m_db->txn_begin();

  if (m_conf->use_cursors)
    m_cursor = m_db->cursor_create(m_txn);

  m_metrics.other_ops++;
}

void
RuntimeGenerator::txn_abort()
{
  tee("TXN_ABORT");
  assert(m_txn != 0);

  if (m_cursor) {
    m_db->cursor_close(m_cursor);
    m_cursor = 0;
  }

  m_last_status = m_db->txn_abort(m_txn);
  m_txn = 0;

  if (m_last_status != 0)
    m_success = false;

  m_metrics.other_ops++;
}

void
RuntimeGenerator::txn_commit()
{
  tee("TXN_COMMIT");
  assert(m_txn != 0);

  if (m_cursor) {
    m_db->cursor_close(m_cursor);
    m_cursor = 0;
  }

  Timer<boost::chrono::high_resolution_clock> t;

  m_last_status = m_db->txn_commit(m_txn);
  m_txn = 0;

  double elapsed = t.seconds();
  if (m_metrics.txn_commit_latency_min > elapsed)
    m_metrics.txn_commit_latency_min = elapsed;
  if (m_metrics.txn_commit_latency_max < elapsed)
    m_metrics.txn_commit_latency_max = elapsed;
  m_metrics.txn_commit_latency_total += elapsed;

  if (m_last_status != 0)
    m_success = false;

  m_metrics.txn_commit_ops++;
}

ham_key_t
RuntimeGenerator::generate_key()
{
  ham_key_t key = {0};
  m_datasource->get_next(m_key_data);

  // append terminating 0 byte
  m_key_data.resize(m_key_data.size() + 1);
  m_key_data[m_key_data.size() - 1] = 0;

  key.data = &m_key_data[0];
  key.size = m_key_data.size() - 1;
  return (key);
}

ham_record_t
RuntimeGenerator::generate_record()
{
  ham_record_t rec = {0};
  m_record_data.resize(m_conf->rec_size);
  // make the record unique (more or less)
  size_t size = std::min((int)sizeof(m_opcount), m_conf->rec_size);
  memcpy(&m_record_data[0], &m_opcount, size);
  for (int i = size; i < m_conf->rec_size; i++)
    m_record_data[i] = (uint8_t)i;

  rec.data = &m_record_data[0];
  rec.size = m_record_data.size();
  return (rec);
}

int
RuntimeGenerator::get_next_command()
{
  // limit reached - last command? then 'close'
  if (limit_reached()) {
    if (m_state == kStateRunning) {
      if (m_txn)
        return (Generator::kCommandCommitTransaction);
      m_state = kStateStopped;
      return (Generator::kCommandClose);
    }
  }

  // first command? then either 'create' or 'reopen', depending on flags
  if (m_opcount == 0) {
    if (m_conf->open)
      return (Generator::kCommandOpen);
    else
      return (Generator::kCommandCreate);
  }

  // begin/abort/commit transactions!
  if (m_conf->transactions_nth) {
    if (!m_txn)
      return (Generator::kCommandBeginTransaction);
    // add +2 because txn_begin/txn_commit are also counted in m_opcount
    if (m_opcount % (m_conf->transactions_nth + 2) == 0)
      return (Generator::kCommandCommitTransaction);
  }

  // perform "real" work
  if (m_conf->erase_pct || m_conf->find_pct) {
    double d = m_u01();
    if (d * 100 < m_conf->erase_pct)
      return (Generator::kCommandErase);
    if (d * 100 >= m_conf->erase_pct
        && d * 100 < (m_conf->erase_pct + m_conf->find_pct))
      return (Generator::kCommandFind);
  }
  return (Generator::kCommandInsert);
}

bool
RuntimeGenerator::limit_reached()
{
  // reached IOPS limit?
  if (m_conf->limit_ops) {
    if (m_opcount == m_conf->limit_ops)
      return (true);
  }

  // reached time limit?
  if (m_conf->limit_seconds) {
    double new_elapsed = m_start.seconds();
    if (m_progress && new_elapsed - m_elapsed_seconds >= 1.) {
      (*m_progress) += (unsigned)(new_elapsed - m_elapsed_seconds);
      m_elapsed_seconds = new_elapsed;
    }
    if (new_elapsed > m_conf->limit_seconds) {
      m_elapsed_seconds = new_elapsed;
      return (true);
    }
  }

  // check inserted bytes
  if (m_conf->limit_bytes) {
    if (m_metrics.insert_bytes >= m_conf->limit_bytes)
      return (true);
  }

  return (false);
}

void
RuntimeGenerator::tee(const char *foo, const ham_key_t *key,
                    const ham_record_t *record)
{
  if (!m_conf->tee_file.empty() || m_conf->verbose) {
    std::stringstream ss;
    ss << foo;
    if (key) {
      switch (m_conf->key_type) {
        case Configuration::kKeyBinary:
          ss << " (" << (const char *)key->data;
          break;
        case Configuration::kKeyUint8:
          ss << " (" << (int)*(const char *)key->data;
          break;
        case Configuration::kKeyUint16:
          ss << " (" << *(uint16_t *)key->data;
          break;
        case Configuration::kKeyUint32:
          ss << " (" << *(uint32_t *)key->data;
          break;
        case Configuration::kKeyUint64:
          ss << " (" << *(uint64_t *)key->data;
          break;
        default:
          assert(!"shouldn't be here");
      }
    }
    if (record)
      ss << ", " << (uint64_t)record->size;
    if (key || record)
      ss << ")";

    if (!m_conf->tee_file.empty())
      m_tee << ss.str() << std::endl;
    else
      std::cout << ss.str() << std::endl;
  }
}