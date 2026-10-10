// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#pragma once
#include <string>
#include <vector>

namespace modcache {
  using arguments = std::vector<std::string>;
  bool modmap(std::string const &text, arguments &result, bool windows);
  bool dependencies(std::string const &text, std::string const &output,
                    arguments &result, bool windows);
  std::string sha256(std::string const &bytes);
  std::string quote_windows(std::string const &argument);
  int launch(arguments const &original);
#ifdef _WIN32
  std::string utf8(std::wstring const &value);
#endif
}
