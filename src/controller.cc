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


#include "controller.h"
#include "thread.h"
#include <boost/thread/xtime.hpp>
#include <boost/progress.hpp>

void 
Controller::run(std::vector<Thread *> &threads) 
{
  boost::progress_display *progress = 0;
  if (m_config->progress)
    progress = new boost::progress_display(m_parser.get_max_lines());
  m_lineno = 1;

  std::vector<Thread *>::iterator it;

  while (!reached_eof()) {
    for (it = threads.begin(); it != threads.end(); it++)
      (*it)->wakeup();

    while (true) {
      boost::mutex::scoped_lock lock(m_mutex);
      if (!reached_line(threads, m_lineno)) {
		boost::xtime t = {0,0};
        t.nsec += 10000;
        xtime_get(&t, 1);
        m_controller_cond.timed_wait(lock, t);
      }
      else
        break;
    }
    if (has_failure(threads)) {
      printf("FAIL\n");
      exit(-1);
    }

    if (strstr(m_parser.get_line(m_lineno - 1).c_str(), "FULLCHECK")) {
      compare_fullcheck(threads);
    }
    else {
      compare_status(threads);
      ham_status_t st = (*threads.begin())->get_status();
      if (st == 0 && strstr(m_parser.get_line(m_lineno - 1).c_str(), "FIND"))
        compare_records(threads);
    }

    ++m_lineno;
    if (progress)
      ++(*progress);
  }

  for (it = threads.begin(); it != threads.end(); it++)
    (*it)->wakeup();

  if (progress)
    delete progress;
}

void 
Controller::compare_status(std::vector<Thread *> &threads)
{
  std::vector<Thread *>::iterator it;
  ham_status_t st1 = (*threads.begin())->get_status();
  for (it = threads.begin() + 1; it != threads.end(); it++) {
    ham_status_t st2 = (*threads.begin())->get_status();
    if (st1 != st2) {
      printf("Failure in line %u: status %d != %d\n", m_lineno, st1, st2);
      exit(-1);
    }
  }
}

void 
Controller::compare_records(std::vector<Thread *> &threads)
{
  std::vector<Thread *>::iterator it;
  const ham_record_t &rec1 = (*threads.begin())->get_record();
  for (it = threads.begin() + 1; it != threads.end(); it++) {
    const ham_record_t &rec2 = (*threads.begin())->get_record();
    if (rec1.size != rec2.size 
        || memcmp(rec1.data, rec2.data, rec1.size)) {
      printf("Failure in line %u: record mismatch\n", m_lineno);
      exit(-1);
    }
  }
}

void 
Controller::compare_fullcheck(std::vector<Thread *> &threads)
{
  std::vector<Thread *>::iterator it;
  for (it = threads.begin(); it != threads.end(); it++)
    (*it)->check_integrity();
  compare_status(threads);

  std::vector<void *> cursors;
  for (it = threads.begin(); it!=threads.end(); it++)
    cursors.push_back((*it)->create_cursor());

  do {
    ham_status_t st, st0;
    ham_key_t key0 = {0};
    ham_record_t rec0 = {0};

    if (m_config->fullcheck_find)
      st0 = threads[0]->get_db()->get_next(cursors[0], &key0, &rec0, 
                HAM_SKIP_DUPLICATES);
    else if (m_config->fullcheck_backwards)
      st0 = threads[0]->get_db()->get_previous(cursors[0], &key0, &rec0, 0);
    else
      st0 = threads[0]->get_db()->get_next(cursors[0], &key0, &rec0, 0);

    for (size_t i = 1; i < cursors.size(); i++) {
      ham_key_t key = {0};
      ham_record_t rec = {0};
      if (m_config->fullcheck_find) {
        key = key0;
        if (st0)
          st = st0;
        else
          st = threads[i]->get_db()->find(&key0, &rec);
      }
      else if (m_config->fullcheck_backwards)
        st = threads[i]->get_db()->get_previous(cursors[i], &key, &rec, 0);
      else
        st = threads[i]->get_db()->get_next(cursors[i], &key, &rec, 0);

      if (m_config->verbose > 1) {
        if (m_config->is_numeric())
          printf("fullcheck %d: %d/%d, keys %d/%d, blob size %d/%d\n",
              threads[i]->get_id(), st0, st, key0.data ? *(int *)key0.data : 0, 
              key.data ? *(int *)key.data : 0, rec0.size, rec.size);
        else
          printf("fullcheck %d: %d/%d, keys %s/%s, blob size %d/%d\n",
              threads[i]->get_id(), st0, st, 
            key0.data ? (char *)key0.data : "(null)", 
            key.data ? (char *)key.data : "(null)",
            rec0.size, rec.size);
      }

      if (st0 != st) {
        printf("line %d: Failure in FULLCHECK: status %d != %d\n",
            (int)m_lineno, st0, st);
        exit(-1);
      }
      if (rec.size != rec0.size || memcmp(rec.data, rec0.data, rec.size)) {
        printf("line %d: Failure in FULLCHECK: record mismatch\n",
            (int)m_lineno);
        exit(-1);
      }
      if (key.size != key0.size || memcmp(key.data, key0.data, key.size)) {
        printf("line %d: Failure in FULLCHECK: key mismatch\n",
            (int)m_lineno);
        exit(-1);
      }
    }

    if (st0 != 0)
      break;
  } while (true);

  for (size_t i = 0; i < cursors.size(); i++)
    threads[i]->close_cursor(cursors[i]);
}

bool 
Controller::reached_line(std::vector<Thread *> &threads, unsigned line) 
{
  std::vector<Thread *>::iterator it;
  for (it = threads.begin(); it != threads.end(); it++) {
    if ((*it)->get_lineno() != line)
      return false;
  }
  return true;
}

bool 
Controller::has_failure(std::vector<Thread *> &threads) 
{
  std::vector<Thread *>::iterator it;
  for (it = threads.begin(); it != threads.end(); it++) {
    if (!(*it)->success())
      return true;
  }
  return false;
}