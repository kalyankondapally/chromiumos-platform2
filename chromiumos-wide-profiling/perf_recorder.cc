// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromiumos-wide-profiling/perf_recorder.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <vector>

#include "chromiumos-wide-profiling/compat/proto.h"
#include "chromiumos-wide-profiling/compat/string.h"
#include "chromiumos-wide-profiling/perf_option_parser.h"
#include "chromiumos-wide-profiling/perf_serializer.h"
#include "chromiumos-wide-profiling/perf_stat_parser.h"
#include "chromiumos-wide-profiling/run_command.h"
#include "chromiumos-wide-profiling/scoped_temp_path.h"
#include "chromiumos-wide-profiling/utils.h"

namespace quipper {

namespace {

// Supported perf subcommands.
const char kPerfRecordCommand[] = "record";
const char kPerfStatCommand[] = "stat";

string IntToString(const int i) {
  stringstream ss;
  ss << i;
  return ss.str();
}

// Reads a perf data file and converts it to a PerfDataProto, which is stored as
// a serialized string in |output_string|. Returns true on success.
bool ParsePerfDataFileToString(const string& filename, string* output_string) {
  // Now convert it into a protobuf.
  PerfSerializer perf_serializer;
  PerfParser::Options options;
  // Make sure to remap address for security reasons.
  options.do_remap = true;
  // Discard unused perf events to reduce the protobuf size.
  options.discard_unused_events = true;

  perf_serializer.set_options(options);

  PerfDataProto perf_data;
  return perf_serializer.SerializeFromFile(filename, &perf_data) &&
         perf_data.SerializeToString(output_string);
}

// Reads a perf data file and converts it to a PerfStatProto, which is stored as
// a serialized string in |output_string|. Returns true on success.
bool ParsePerfStatFileToString(const string& filename,
                               const std::vector<string>& command_line_args,
                               string* output_string) {
  PerfStatProto perf_stat;
  if (!ParsePerfStatFileToProto(filename, &perf_stat)) {
    LOG(ERROR) << "Failed to parse PerfStatProto from " << filename;
    return false;
  }

  // Fill in the command line field of the protobuf.
  string command_line;
  for (size_t i = 0; i < command_line_args.size(); ++i) {
    const string& arg = command_line_args[i];
    // Strip the output file argument from the command line.
    if (arg == "-o") {
      ++i;
      continue;
    }
    command_line.append(arg + " ");
  }
  command_line.resize(command_line.size() - 1);
  perf_stat.mutable_command_line()->assign(command_line);

  return perf_stat.SerializeToString(output_string);
}

}  // namespace

PerfRecorder::PerfRecorder() : PerfRecorder({"/usr/bin/perf"}) {}

PerfRecorder::PerfRecorder(const std::vector<string>& perf_binary_command)
    : perf_binary_command_(perf_binary_command) {
}

bool PerfRecorder::RunCommandAndGetSerializedOutput(
    const std::vector<string>& perf_args,
    const int time,
    string* output_string) {
  if (!ValidatePerfCommandLine(perf_args)) {
    LOG(ERROR) << "Perf arguments are not safe to run!";
    return false;
  }

  // ValidatePerfCommandLine should have checked perf_args[0] == "perf", and
  // that perf_args[1] is a supported sub-command (e.g. "record" or "stat").

  const string& perf_type = perf_args[1];

  if (perf_type != kPerfRecordCommand && perf_type != kPerfStatCommand) {
    LOG(ERROR) << "Unsupported perf subcommand: " << perf_type;
    return false;
  }

  ScopedTempFile output_file;

  // Assemble the full command line:
  // - Replace "perf" in |perf_args[0]| with |perf_binary_command_| to
  //   guarantee we're running a binary we believe we can trust.
  // - Add our own paramters.

  std::vector<string> full_perf_args(perf_binary_command_);
  full_perf_args.insert(full_perf_args.end(),
                        perf_args.begin() + 1,  // skip "perf"
                        perf_args.end());
  full_perf_args.insert(full_perf_args.end(), {"-o", output_file.path()});

  // The perf stat output parser requires raw data from verbose output.
  if (perf_type == kPerfStatCommand)
    full_perf_args.emplace_back("-v");

  // Append the sleep command to run perf for |time| seconds.
  full_perf_args.insert(full_perf_args.end(),
                        {"--", "sleep", IntToString(time)});

  // The perf command writes the output to a file, so ignore stdout.
  if (!RunCommand(full_perf_args, nullptr)) {
    LOG(ERROR) << "Failed to run perf command.";
    return false;
  }

  if (perf_type == kPerfRecordCommand)
    return ParsePerfDataFileToString(output_file.path(), output_string);

  // Otherwise, parse as perf stat output.
  return ParsePerfStatFileToString(output_file.path(),
                                   full_perf_args,
                                   output_string);
}

}  // namespace quipper
