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

#include "misc.h"
#include "configuration.h"
#include "generator_parser.h"

ParserGenerator::ParserGenerator(int id, Configuration *conf, Database *db,
                bool show_progress)
  : Generator(id, conf, db), m_txn(0), m_cursor(0), m_progress(0),
    m_success(true), m_cur_line(0)
{
  memset(&m_metrics, 0, sizeof(m_metrics));
  m_metrics.insert_latency_min = 9999999.99;
  m_metrics.erase_latency_min = 9999999.99;
  m_metrics.find_latency_min = 9999999.99;
  m_metrics.txn_commit_latency_min = 9999999.99;

  read_file();

  if (show_progress) {
    if (!conf->no_progress && !conf->quiet && !conf->verbose)
      m_progress = new boost::progress_display(get_line_count());
  }
}

bool
ParserGenerator::execute()
{
  if (m_cur_line >= m_lines.size())
    return (false);

  const char *flags = 0;
  const char *keydata = 0;
  const char *recdata = 0;
  int cmd = get_next_command(&flags, &keydata, &recdata);

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
      insert(keydata, recdata, flags);
      break;
    case Generator::kCommandErase:
      erase(keydata);
      break;
    case Generator::kCommandFind:
      find(keydata);
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
    case Generator::kCommandFullcheck:
      m_last_status = Generator::kCommandFullcheck;
      break;
    case Generator::kCommandFlush: // TODO
    case Generator::kCommandNop:
      break;
    default:
      assert(!"shouldn't be here");
  }

  m_cur_line++;

  if (m_progress)
    (*m_progress) += 1;

  return (true);
}

void
ParserGenerator::create()
{
  // Environment was already created in main.cc
  // m_db->create_env();
  m_last_status = m_db->create_db(m_id);
  
  if (m_config->use_cursors)
    m_cursor = m_db->cursor_create(m_txn);

  if (m_last_status != 0)
    m_success = false;

  m_metrics.other_ops++;
}

void
ParserGenerator::open()
{
  // Environment was already opened in main.cc
  // m_db->open_env();
  m_last_status = m_db->open_db(m_id);

  if (m_config->use_cursors)
    m_cursor = m_db->cursor_create(m_txn);

  if (m_last_status != 0)
    m_success = false;

  m_metrics.other_ops++;
}
 
void
ParserGenerator::close()
{
  if (m_cursor) {
    m_db->cursor_close(m_cursor);
    m_cursor = 0;
  }

  m_last_status = m_db->close_db();
  if (m_last_status != 0)
    m_success = false;

  m_db->close_env();

  m_metrics.other_ops++;
  m_metrics.elapsed_wallclock_seconds = m_start.seconds();
}

void
ParserGenerator::insert(const char *keydata, const char *recdata,
                const char *flags)
{
  ham_key_t key = {0}; // TODO generate_key();
  ham_record_t rec = {0}; // TODO generate_record();

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
    if (m_progress && m_config->limit_bytes)
      (*m_progress) += key.size + rec.size;
  }

  m_metrics.insert_ops++;
}

void
ParserGenerator::erase(const char *keydata)
{
  ham_key_t key = {0}; // TODO generate_key();

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
ParserGenerator::find(const char *keydata)
{
  ham_key_t key = {0}; // TODO generate_key();
  ham_record_t m_record = {0};
  memset(&m_record, 0, sizeof(m_record));

  Timer<boost::chrono::high_resolution_clock> t;

  if (m_cursor)
    m_last_status = m_db->cursor_find(m_cursor, &key, &m_record);
  else
    m_last_status = m_db->find(m_txn, &key, &m_record);

  double elapsed = t.seconds();
  if (m_metrics.find_latency_min > elapsed)
    m_metrics.find_latency_min = elapsed;
  if (m_metrics.find_latency_max < elapsed)
    m_metrics.find_latency_max = elapsed;
  m_metrics.find_latency_total += elapsed;

  if (m_last_status != 0 && m_last_status != HAM_KEY_NOT_FOUND)
    m_success = false;

  m_metrics.find_bytes += m_record.size;
  m_metrics.find_ops++;
}

void
ParserGenerator::txn_begin()
{
  assert(m_txn == 0);

  if (m_cursor) {
    m_db->cursor_close(m_cursor);
    m_cursor = 0;
  }

  m_txn = m_db->txn_begin();

  if (m_config->use_cursors)
    m_cursor = m_db->cursor_create(m_txn);

  m_metrics.other_ops++;
}

void
ParserGenerator::txn_abort()
{
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
ParserGenerator::txn_commit()
{
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

int
ParserGenerator::get_next_command(const char **pflags, const char **pkeydata,
            const char **precdata)
{
  // create a local copy because the string will be modified
  std::string tmp = m_lines[m_cur_line];
  std::vector<std::string> tokens = tokenize(m_lines[m_cur_line]);
  if (tokens.empty())
    return (kCommandNop);
  VERBOSE(("%d: line %u: reading token '%s' .......................\n", 
        m_db->get_id(), m_cur_line, tokens[0].c_str()));
  if (tokens[0] == "BREAK") {
    printf("[info] break at %s:%u\n", __FILE__, __LINE__);
    return (kCommandNop);
  }
  if (tokens[0] == "--") {
    return (kCommandNop);
  }
  if (tokens[0] == "CREATE") {
    return (kCommandCreate);
  }
  if (tokens[0] == "OPEN") {
    return (kCommandOpen);
  }
  if (tokens[0] == "INSERT") {
    if (tokens.size() == 3) {
      *pflags = tokens[1].c_str();
      *pkeydata = "";
      *precdata = tokens[2].c_str();
    }
    else if (tokens.size() == 4) {
      *pflags  = tokens[1].c_str();
      *pkeydata = tokens[2].c_str();
      *precdata = tokens[3].c_str();
    }
    else {
      ERROR(("line %d (INSERT): parser error\n", m_cur_line + 1));
      exit(-1);
    }
    if (!*precdata)
      *precdata = "";
    //TODO return (engine->insert(m_cur_line, keytok, data));
    return (kCommandInsert);
  }
  if (tokens[0] == "ERASE") {
    if (tokens.size() < 3) {
      ERROR(("line %d (ERASE): parser error\n", m_cur_line + 1));
      exit(-1);
    }
    *pflags = tokens[1].c_str();
    *pkeydata = tokens[2].c_str();
    // TODO return (engine->erase(keytok));
    return (kCommandErase);
  }
  if (tokens[0] == "FIND") {
    if (tokens.size() != 3) {
      ERROR(("line %d (FIND): parser error\n", m_cur_line + 1));
      exit(-1);
    }
    *pflags = tokens[1].c_str();
    *pkeydata = tokens[2].c_str();
    // TODO return (engine->find(keytok));
    return (kCommandFind);
  }
  if (tokens[0] == "FULLCHECK") {
    return (kCommandFullcheck);
  }
  if (tokens[0] == "BEGIN_TXN") {
    return (kCommandBeginTransaction);
  }
  if (tokens[0] == "CLOSE_TXN") {
    return (kCommandCommitTransaction);
  }
  if (tokens[0] == "CLOSE") {
    return (kCommandClose);
  }
  if (tokens[0] == "FLUSH") {
    return (kCommandFlush);
  }

  ERROR(("line %d: invalid token '%s'\n", m_cur_line, tokens[0].c_str()));
  ::exit(-1);
  return (0);
}

void
ParserGenerator::read_file()
{
  FILE *f = stdin;
  if (!m_config->filename.empty())
    f = fopen(m_config->filename.c_str(), "rt");

  if (!f) {
    ERROR(("failed to open %s\n", m_config->filename.c_str()));
    exit(-1);
  }

  char line[1024 * 16];

  while (!feof(f)) {
    char *p = fgets(line, sizeof(line), f);
    if (!p)
      break;

    m_lines.push_back(p);
  }

  if (f != stdin)
    fclose(f);
}

std::vector<std::string> 
ParserGenerator::tokenize(const std::string &str)
{
  std::vector<std::string> tokens;
  std::string delimiters = " \t\n\r()\",";
  // Skip delimiters at beginning.
  std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
  // Find first "non-delimiter".
  std::string::size_type pos = str.find_first_of(delimiters, lastPos);

  while (std::string::npos != pos || std::string::npos != lastPos) {
    // Found a token, add it to the vector.
    tokens.push_back(str.substr(lastPos, pos - lastPos));
    // Skip delimiters.  Note the "not_of"
    lastPos = str.find_first_not_of(delimiters, pos);
    // Find next "non-delimiter"
    pos = str.find_first_of(delimiters, lastPos);
  }
  return tokens;
}

