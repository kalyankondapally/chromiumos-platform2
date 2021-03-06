// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROSLOG_LOG_PARSER_AUDIT_H_
#define CROSLOG_LOG_PARSER_AUDIT_H_

#include "croslog/log_parser.h"

#include <string>

namespace croslog {

class LogParserAudit : public LogParser {
 public:
  LogParserAudit();

 private:
  MaybeLogEntry ParseInternal(std::string&& entire_line);

  DISALLOW_COPY_AND_ASSIGN(LogParserAudit);
};

}  // namespace croslog

#endif  // CROSLOG_LOG_PARSER_AUDIT_H_
