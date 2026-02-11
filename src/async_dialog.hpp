#pragma once

#include <nfd.h>

#include <optional>
#include <string>
#include <vector>

namespace jcan {

struct nfd_filter {
  const char* name;
  const char* spec;
};

class async_dialog {
 public:
  async_dialog() = default;

  bool busy() const { return false; }

  void open_file(std::vector<nfd_filter> filters,
                 const char* default_path = nullptr) {
    std::vector<nfdu8filteritem_t> nfd_filters;
    nfd_filters.reserve(filters.size());
    for (auto& f : filters) nfd_filters.push_back({f.name, f.spec});

    nfdu8char_t* out = nullptr;
    nfdresult_t res = NFD_OpenDialogU8(
        &out, nfd_filters.data(),
        static_cast<nfdfiltersize_t>(nfd_filters.size()), default_path);
    if (res == NFD_OKAY && out) {
      result_ = std::string(out);
      NFD_FreePathU8(out);
    } else {
      result_ = std::nullopt;
    }
    has_result_ = true;
  }

  void save_file(std::vector<nfd_filter> filters,
                 const char* default_name = nullptr,
                 const char* default_path = nullptr) {
    std::vector<nfdu8filteritem_t> nfd_filters;
    nfd_filters.reserve(filters.size());
    for (auto& f : filters) nfd_filters.push_back({f.name, f.spec});

    nfdu8char_t* out = nullptr;
    nfdresult_t res =
        NFD_SaveDialogU8(&out, nfd_filters.data(),
                         static_cast<nfdfiltersize_t>(nfd_filters.size()),
                         default_path, default_name);
    if (res == NFD_OKAY && out) {
      result_ = std::string(out);
      NFD_FreePathU8(out);
    } else {
      result_ = std::nullopt;
    }
    has_result_ = true;
  }

  std::optional<std::optional<std::string>> poll() {
    if (!has_result_) return std::nullopt;
    has_result_ = false;
    return result_;
  }

 private:
  std::optional<std::string> result_;
  bool has_result_{false};
};

}  // namespace jcan
