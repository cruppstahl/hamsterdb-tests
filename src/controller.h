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


#ifndef CONTROLLER_HPP__
#define CONTROLLER_HPP__

#include <boost/thread/condition.hpp>
#include <iostream>

#include "parser.h"
#include "configuration.h"

class Thread;

class Controller
{
  public:
    Controller(Configuration *config, Parser &parser) 
      : m_lineno(0), m_config(config), m_parser(parser) {
    }

    void wakeup() {
      m_controller_cond.notify_one();
    }

    void run(std::vector<Thread *> &threads);

    bool reached_eof() {
      return m_lineno >= m_parser.get_max_lines();
    }

    bool reached_line(std::vector<Thread *> &threads, unsigned line);

    unsigned next();

    bool has_failure(std::vector<Thread *> &threads);

  private:
    void compare_status(std::vector<Thread *> &threads);
    void compare_records(std::vector<Thread *> &threads);
    void compare_fullcheck(std::vector<Thread *> &threads);

    boost::condition m_controller_cond;
    boost::mutex m_mutex;
    unsigned m_lineno;
    Configuration *m_config;
    Parser &m_parser;
};

#endif /* CONTROLLER_HPP__ */