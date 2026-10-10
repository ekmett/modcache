// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "cache.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace modcache {
  namespace {
    bool starts(std::string const &s, std::string const &prefix) {
      return s.compare(0, prefix.size(), prefix) == 0;
    }
    bool ends(std::string const &s, std::string const &suffix) {
      return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
    bool words(std::string const &s, arguments &out, bool windows) {
      std::string word;
      for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size() && (!windows || s[i + 1] == ' ')) {
          word += s[++i];
        } else if (c == ' ' || c == '\t') {
          if (!word.empty()) { out.push_back(word); word.clear(); }
        } else {
          if (c == '\0' || c == '\n' || c == '\r' || c == '$' || c == '#' || c == '"' || c == '\'') return false;
          word += c;
        }
      }
      if (!word.empty()) out.push_back(word);
      return true;
    }
    std::uint32_t rotate(std::uint32_t n, unsigned shift) {
      return (n >> shift) | (n << (32 - shift));
    }
    struct digest {
      std::array<std::uint32_t, 8> state{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                                        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
      std::array<unsigned char, 64> block{};
      std::uint64_t size = 0;
      std::size_t used = 0;
      void compress() {
        static std::uint32_t const k[] = {
          0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
          0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
          0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
          0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
          0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
          0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
          0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
          0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        std::uint32_t w[64];
        for (unsigned i = 0; i < 16; ++i)
          w[i] = (std::uint32_t(block[4*i]) << 24) | (std::uint32_t(block[4*i+1]) << 16)
               | (std::uint32_t(block[4*i+2]) << 8) | block[4*i+3];
        for (unsigned i = 16; i < 64; ++i) {
          auto a = w[i-15], b = w[i-2];
          w[i] = w[i-16] + (rotate(a,7) ^ rotate(a,18) ^ (a >> 3)) + w[i-7]
               + (rotate(b,17) ^ rotate(b,19) ^ (b >> 10));
        }
        auto a=state[0], b=state[1], c=state[2], d=state[3];
        auto e=state[4], f=state[5], g=state[6], h=state[7];
        for (unsigned i=0; i<64; ++i) {
          auto t = h + (rotate(e,6)^rotate(e,11)^rotate(e,25)) + ((e&f)^(~e&g)) + k[i] + w[i];
          auto u = (rotate(a,2)^rotate(a,13)^rotate(a,22)) + ((a&b)^(a&c)^(b&c));
          h=g; g=f; f=e; e=d+t; d=c; c=b; b=a; a=t+u;
        }
        std::uint32_t const next[] = {a,b,c,d,e,f,g,h};
        for (unsigned i=0; i<8; ++i) state[i] += next[i];
      }
      void update(char const *data, std::size_t length) {
        size += length;
        while (length) {
          auto n = std::min(length, block.size() - used);
          std::memcpy(block.data() + used, data, n);
          used += n; data += n; length -= n;
          if (used == block.size()) { compress(); used = 0; }
        }
      }
      std::string finish() {
        auto bits = size * 8;
        char pad = char(0x80);
        update(&pad, 1);
        pad = 0;
        while (used != 56) update(&pad, 1);
        for (int i=7; i>=0; --i) { char b = char(bits >> (i*8)); update(&b,1); }
        static char const hex[] = "0123456789abcdef";
        std::string result;
        for (auto word : state)
          for (int i=7; i>=0; --i) result += hex[(word >> (i*4)) & 15];
        return result;
      }
    };
#ifdef _WIN32
    std::wstring wide(std::string const &s) {
      int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), nullptr, 0);
      if (!n && !s.empty()) throw std::runtime_error("invalid UTF-8 argument");
      std::wstring out(n, L'\0');
      if (n) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), &out[0], n);
      return out;
    }
#endif
    using file = std::unique_ptr<FILE, decltype(&std::fclose)>;
    file open(std::string const &path, char const *mode) {
#ifdef _WIN32
      return file(_wfopen(wide(path).c_str(), wide(mode).c_str()), &std::fclose);
#else
      return file(std::fopen(path.c_str(), mode), &std::fclose);
#endif
    }
    bool read(std::string const &path, std::string &out, std::size_t limit) {
      auto stream = open(path, "rb");
      if (!stream) return false;
      char buffer[4096];
      while (auto n = std::fread(buffer, 1, sizeof buffer, stream.get())) {
        if (out.size() + n > limit) return false;
        out.append(buffer,n);
      }
      return !std::ferror(stream.get());
    }
    std::string absolute(std::string const &path) {
#ifdef _WIN32
      auto w = wide(path);
      DWORD size = GetFullPathNameW(w.c_str(), 0, nullptr, nullptr);
      if (!size) return {};
      std::wstring result(size, L'\0');
      DWORD n = GetFullPathNameW(w.c_str(), size, &result[0], nullptr);
      if (!n || n >= size) return {};
      result.resize(n);
      return utf8(result);
#else
      std::unique_ptr<char, decltype(&std::free)> p(realpath(path.c_str(), nullptr), &std::free);
      return p ? p.get() : std::string();
#endif
    }
    std::string environment(char const *name) {
#ifdef _WIN32
      auto key = wide(name);
      DWORD n = GetEnvironmentVariableW(key.c_str(), nullptr, 0);
      if (!n) return {};
      std::wstring value(n, L'\0');
      DWORD length = GetEnvironmentVariableW(key.c_str(), &value[0], n);
      if (length >= n) throw std::runtime_error("environment changed while reading");
      value.resize(length);
      return utf8(value);
#else
      char const *s = std::getenv(name);
      return s ? s : "";
#endif
    }
    bool set_environment(std::string const &value) {
#ifdef _WIN32
      return SetEnvironmentVariableW(L"SCCACHE_EXTRAFILES", wide(value).c_str()) != 0;
#else
      return setenv("SCCACHE_EXTRAFILES", value.c_str(), 1) == 0;
#endif
    }
    int execute(arguments const &args) {
#ifdef _WIN32
      std::string command;
      for (auto const &arg : args) {
        if (!command.empty()) command += ' ';
        command += quote_windows(arg);
      }
      auto text = wide(command);
      STARTUPINFOW startup{};
      startup.cb = sizeof startup;
      PROCESS_INFORMATION process{};
      text.push_back(L'\0');
      if (!CreateProcessW(nullptr, &text[0], nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process))
        throw std::runtime_error("cannot launch " + args[0] + " (Windows error " + std::to_string(GetLastError()) + ")");
      CloseHandle(process.hThread);
      WaitForSingleObject(process.hProcess, INFINITE);
      DWORD status = 1;
      bool ok = GetExitCodeProcess(process.hProcess, &status) != 0;
      CloseHandle(process.hProcess);
      if (!ok) throw std::runtime_error("cannot obtain compiler exit status");
      return int(status);
#else
      std::vector<char *> argv;
      for (auto const &s : args) argv.push_back(const_cast<char *>(s.c_str()));
      argv.push_back(nullptr);
      execvp(argv[0], argv.data());
      return errno;
#endif
    }
    bool manifest(arguments const &inputs, std::string const &output, std::string &path) {
      std::string data = "modcache-v1\n";
      for (auto const &input : inputs) {
        auto full = absolute(input);
        auto stream = open(input, "rb");
        if (full.empty() || !stream) return false;
        digest hash;
        char buffer[16384];
        while (auto n = std::fread(buffer,1,sizeof buffer,stream.get())) hash.update(buffer,n);
        if (std::ferror(stream.get())) return false;
        data += std::to_string(full.size()) + ":" + full + hash.finish() + "\n";
      }
      path = output + ".modcache";
      // CMake owns one compilation per object. Atomic replacement keeps a
      // reader from observing a partial manifest after interruption.
#ifdef _WIN32
      auto temporary = path + "." + std::to_string(GetCurrentProcessId()) + ".tmp";
#else
      auto temporary = path + "." + std::to_string(getpid()) + ".tmp";
#endif
      auto stream = open(temporary, "wb");
      if (!stream) return false;
      bool ok = std::fwrite(data.data(),1,data.size(),stream.get()) == data.size();
      ok = std::fclose(stream.release()) == 0 && ok;
#ifdef _WIN32
      ok = ok && MoveFileExW(wide(temporary).c_str(),wide(path).c_str(),MOVEFILE_REPLACE_EXISTING);
      if (!ok) DeleteFileW(wide(temporary).c_str());
#else
      ok = ok && std::rename(temporary.c_str(),path.c_str()) == 0;
      if (!ok) std::remove(temporary.c_str());
#endif
      path = absolute(path);
      return ok && !path.empty();
    }
  }

#ifdef _WIN32
  std::string utf8(std::wstring const &value) {
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), int(value.size()), nullptr,0,nullptr,nullptr);
    if (!n && !value.empty()) throw std::runtime_error("invalid Unicode argument");
    std::string result(n,'\0');
    if (n) WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),&result[0],n,nullptr,nullptr);
    return result;
  }
#endif
  std::string sha256(std::string const &bytes) {
    digest hash;
    hash.update(bytes.data(),bytes.size());
    return hash.finish();
  }
  std::string quote_windows(std::string const &arg) {
    std::string result = "\"";
    std::size_t slashes = 0;
    for (char c : arg) {
      if (c == '\\') { ++slashes; continue; }
      result.append(c == '"' ? 2*slashes+1 : slashes, '\\');
      slashes = 0;
      result += c;
    }
    result.append(2*slashes,'\\');
    return result + '"';
  }
  bool modmap(std::string const &text, arguments &result, bool windows) {
    arguments parsed;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream,line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (!windows && line == "-x c++-module") { parsed.push_back("-x"); parsed.push_back("c++-module"); continue; }
      // Older CMake emits unquoted module values. Accept those only when
      // they are single response-file words; the same value checks follow.
      auto equal = line.find('=');
      if (equal != std::string::npos && equal+1 < line.size() && line[equal+1] != '"') {
        if (line.find_first_of(" \t\"'") != std::string::npos) return false;
        line.insert(equal+1,1,'"');
        line += '"';
      }
      std::smatch match;
      static std::regex const module("-fmodule-file=\"([A-Za-z_][A-Za-z_0-9.:]*)=([^\"\r\n]+)\"");
      static std::regex const output("-fmodule-output=\"([^\"\r\n]+)\"");
      std::string arg;
      if (std::regex_match(line,match,module)) arg = "-fmodule-file=" + match[1].str() + "=" + match[2].str();
      else if (!windows && std::regex_match(line,match,output)) arg = "-fmodule-output=" + match[1].str();
      else return false;
      // POSIX response quoting interprets backslashes. Windows directory
      // separators are literal, but a trailing backslash would escape a quote.
      if ((!windows && arg.find('\\') != std::string::npos) || arg.back() == '\\' || arg.find('\0') != std::string::npos) return false;
      parsed.push_back(arg);
    }
    result.insert(result.end(),parsed.begin(),parsed.end());
    return true;
  }
  bool dependencies(std::string const &text, std::string const &output, arguments &result, bool windows) {
    std::string joined;
    for (std::size_t i=0; i<text.size(); ++i) {
      if (text[i] == '\\' && i+1<text.size() && text[i+1] == '\n') ++i;
      else if (text[i] == '\\' && i+2<text.size() && text[i+1] == '\r' && text[i+2] == '\n') i += 2;
      else joined += text[i];
    }
    while (!joined.empty() && (joined.back() == '\n' || joined.back() == '\r')) joined.pop_back();
    auto separator = joined.find(": ");
    if (separator == std::string::npos) return false;
    arguments targets, inputs;
    if (!words(joined.substr(0,separator),targets,windows) || targets.size()!=1 || targets[0]!=output+".ddi") return false;
    if (!words(joined.substr(separator+2),inputs,windows) || inputs.empty()) return false;
    result.insert(result.end(),inputs.begin(),inputs.end());
    return true;
  }

  int launch(arguments const &original) {
#ifdef _WIN32
    bool const windows = true;
    char const path_separator = ';';
#else
    bool const windows = false;
    char const path_separator = ':';
#endif
    auto bypass = [&]() {
      int status = execute(original);
#ifndef _WIN32
      throw std::runtime_error("cannot launch " + original[0] + ": " + std::strerror(status));
#endif
      return status;
    };
    auto basename = original[0].substr(original[0].find_last_of("/\\") + 1);
    std::regex const compiler(windows ? "clang-cl(-[0-9]+)?(\\.exe)?" : "clang(\\+\\+)?(-[0-9]+)?", std::regex::icase);
    if (!std::regex_match(basename,compiler)) return bypass();
    arguments args{original[0]}, inputs;
    for (std::size_t i=1; i<original.size(); ++i) {
      auto const &arg = original[i];
      if (starts(arg,"@")) {
        std::string text;
        if (!ends(arg,".modmap") || !read(arg.substr(1),text,64*1024) || !modmap(text,args,windows)) return bypass();
      } else args.push_back(arg);
    }
    bool modules = false;
    std::string output;
    arguments sources;
    for (std::size_t i=1; i<args.size(); ++i) {
      auto arg = args[i];
      bool forwarded = false;
      for (;;) {
        std::size_t prefix = starts(arg,"/clang:") || starts(arg,"-clang:") ? 7 : starts(arg,"-Xclang=") ? 8 : 0;
        if (!prefix) break;
        arg.erase(0,prefix);
        forwarded = true;
      }
      if (forwarded && (starts(arg,"-fmodule") || starts(arg,"-emit-module") || starts(arg,"-include-pch"))) return bypass();
      if (arg == "-o" && i+1<args.size()) output = args[i+1];
      if (windows && starts(arg,"/Fo")) output = arg.substr(3);
      bool source = ends(arg,".cc") || ends(arg,".cpp") || ends(arg,".cxx") || ends(arg,".c++") || ends(arg,".ccm") || ends(arg,".cppm") || ends(arg,".ixx") || ends(arg,".cxxm") || ends(arg,".mpp") || ends(arg,".mxx");
      if (source) sources.push_back(arg);
      bool provider = arg == "c++-module" || starts(arg,"-xc++-module") || arg == "--precompile" || starts(arg,"-fmodule-output=") || ends(arg,".ccm") || ends(arg,".cppm") || ends(arg,".ixx") || ends(arg,".cxxm") || ends(arg,".mpp") || ends(arg,".mxx");
      if (windows && provider) return bypass();
      if (arg == "c++-header" || starts(arg,"-xc++-header") || arg == "-emit-pch") return bypass();
      modules = modules || provider;
      if (starts(arg,"-fmodule-file=")) {
        auto value = arg.substr(14);
        auto equal = value.find('=');
        inputs.push_back(equal == std::string::npos ? value : value.substr(equal+1));
        modules = true;
      } else if (starts(arg,"-fmodule-output=")) {
        modules = true;
      } else if (starts(arg,"-fmodule") || starts(arg,"-fprebuilt-module") || starts(arg,"-fimplicit-module") || starts(arg,"-fno-implicit-module") || starts(arg,"-emit-module")) {
        return bypass();
      }
      if (!starts(arg,"-fmodule-file=") && !starts(arg,"-fmodule-output=") &&
          (ends(arg,".pcm") || ends(arg,".bmi") || ends(arg,".ifc") || ends(arg,".pch"))) {
        // The explicit PCH handler below already accounted for its argument.
        if (i == 0 || (args[i-1] != "-include-pch" &&
            !(i>2 && args[i-1] == "-Xclang" && args[i-2] == "-include-pch"))) return bypass();
      }
      if (arg == "-include-pch") {
        if (windows) return bypass();
        std::size_t index = i+1;
        if (i>1 && args[i-1] == "-Xclang") {
          if (index>=args.size() || args[index++] != "-Xclang") return bypass();
        }
        if (index>=args.size() || args[index].empty() || args[index][0]=='-' || args[index][0]=='@') return bypass();
        inputs.push_back(args[index]);
      } else if (arg.find("include-pch") != std::string::npos || arg.find("include-pth") != std::string::npos ||
                 starts(arg,"/Fp") || starts(arg,"/Yu") || starts(arg,"/Yc") || starts(arg,"-Fp") || starts(arg,"-Yu") || starts(arg,"-Yc") ||
                 starts(arg,"/ifc") || starts(arg,"/reference") || starts(arg,"/headerUnit") || starts(arg,"/interface") ||
                 starts(arg,"/stdIfcDir") || starts(arg,"/exportHeader") || starts(arg,"/internalPartition") || starts(arg,"/experimental:module")) return bypass();
    }
    if (modules) {
      std::string scan;
      arguments raw;
      if (output.empty() || !read(output+".ddi.d",scan,1024*1024) || !dependencies(scan,output,raw,windows)) return bypass();
      bool owns_source = false;
      for (auto const &source : sources) {
        auto full = absolute(source);
        for (auto const &input : raw) if (!full.empty() && full==absolute(input)) owns_source = true;
      }
      if (!owns_source) return bypass();
      inputs.insert(inputs.end(),raw.begin(),raw.end());
      inputs.push_back(output+".ddi.d");
    }
    if (!inputs.empty()) {
      std::string path;
      if (output.empty() || !manifest(inputs,output,path) || path.find(path_separator)!=std::string::npos) return bypass();
      auto previous = environment("SCCACHE_EXTRAFILES");
      if (!previous.empty()) path = previous + path_separator + path;
      if (windows && path.size()>30000) return bypass();
      if (!set_environment(path)) return bypass();
    }
    if (windows) {
      std::size_t size = 0;
      for (auto const &arg : args) size += quote_windows(arg).size() + 1;
      if (size > 30000) return bypass();
    }
    args.insert(args.begin(),"sccache");
    int status = execute(args);
#ifndef _WIN32
    if (status == E2BIG) return bypass();
    throw std::runtime_error("cannot launch sccache: " + std::string(std::strerror(status)));
#else
    return status;
#endif
  }
}
