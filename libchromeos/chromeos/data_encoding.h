// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_DATA_ENCODING_H_
#define LIBCHROMEOS_CHROMEOS_DATA_ENCODING_H_

#include <string>
#include <utility>
#include <vector>

#include <chromeos/chromeos_export.h>
#include <chromeos/secure_blob.h>

namespace chromeos {
namespace data_encoding {

using WebParamList = std::vector<std::pair<std::string, std::string>>;

// Encode/escape string to be used in the query portion of a URL.
// If |encodeSpaceAsPlus| is set to true, spaces are encoded as '+' instead
// of "%20"
CHROMEOS_EXPORT std::string UrlEncode(const char* data, bool encodeSpaceAsPlus);

inline std::string UrlEncode(const char* data) {
  return UrlEncode(data, true);
}

// Decodes/unescapes a URL. Replaces all %XX sequences with actual characters.
// Also replaces '+' with spaces.
CHROMEOS_EXPORT std::string UrlDecode(const char* data);

// Converts a list of key-value pairs into a string compatible with
// 'application/x-www-form-urlencoded' content encoding.
CHROMEOS_EXPORT std::string WebParamsEncode(const WebParamList& params,
                                            bool encodeSpaceAsPlus);

inline std::string WebParamsEncode(const WebParamList& params) {
  return WebParamsEncode(params, true);
}

// Parses a string of '&'-delimited key-value pairs (separated by '=') and
// encoded in a way compatible with 'application/x-www-form-urlencoded'
// content encoding.
CHROMEOS_EXPORT WebParamList WebParamsDecode(const std::string& data);

// Encodes binary data using base64-encoding.
CHROMEOS_EXPORT std::string Base64Encode(const void* data, size_t size);

// Encodes binary data using base64-encoding and wraps lines at 64 character
// boundary using LF as required by PEM (RFC 1421) specification.
CHROMEOS_EXPORT std::string Base64EncodeWrapLines(const void* data,
                                                  size_t size);

// Decodes the input string from Base64.
CHROMEOS_EXPORT bool Base64Decode(const std::string& input,
                                  chromeos::Blob* output);

// Helper wrappers to use std::string and chromeos::Blob as binary data
// containers.
inline std::string Base64Encode(const chromeos::Blob& input) {
  return Base64Encode(input.data(), input.size());
}
inline std::string Base64EncodeWrapLines(const chromeos::Blob& input) {
  return Base64EncodeWrapLines(input.data(), input.size());
}
inline std::string Base64Encode(const std::string& input) {
  return Base64Encode(input.data(), input.size());
}
inline std::string Base64EncodeWrapLines(const std::string& input) {
  return Base64EncodeWrapLines(input.data(), input.size());
}
inline bool Base64Decode(const std::string& input, std::string* output) {
  chromeos::Blob blob;
  if (!Base64Decode(input, &blob))
    return false;
  *output = std::string{blob.begin(), blob.end()};
  return true;
}

}  // namespace data_encoding
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DATA_ENCODING_H_
