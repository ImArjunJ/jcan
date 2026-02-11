#pragma once

#include <nfd.h>

#include <atomic>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace jcan {

// Lightweight async wrapper around NFD dialogs.
// NFD calls block until the user closes the picker; running them on a
// background thread keeps the main render loop alive.
//
// Usage:
//   async_dialog dlg;
//   dlg.open_file({{"DBC Files", "dbc"}});   // launches picker
//   // ... each frame ...
//   if (auto r = dlg.poll()) {
//     if (*r) use_path(**r);                  // user picked a file
//     else    /* user cancelled */;
//   }

struct nfd_filter {
  const char* name;
  const char* spec;  // e.g. "csv,asc"
};

class async_dialog {
 public:
  async_dialog() = default;
  ~async_dialog() { wait(); }

  // Non-copyable, movable
  async_dialog(const async_dialog&) = delete;
  async_dialog& operator=(const async_dialog&) = delete;
  async_dialog(async_dialog&&) = default;
  async_dialog& operator=(async_dialog&&) = default;

  /// True while a dialog is open (background thread running).
  bool busy() const { return busy_.load(std::memory_order_acquire); }

  /// Launch an Open-File dialog on a background thread.
  void open_file(std::vector<nfd_filter> filters,
                 const char* default_path = nullptr) {
    wait();
    launch([filters = std::move(filters), default_path]() -> std::optional<std::string> {
      std::vector<nfdu8filteritem_t> nfd_filters;
      nfd_filters.reserve(filters.size());
      for (auto& f : filters) nfd_filters.push_back({f.name, f.spec});

      nfdu8char_t* out = nullptr;
      nfdresult_t res = NFD_OpenDialogU8(
          &out, nfd_filters.data(),
          static_cast<nfdfiltersize_t>(nfd_filters.size()), default_path);
      if (res == NFD_OKAY && out) {
        std::string path(out);
        NFD_FreePathU8(out);
        return path;
      }
      return std::nullopt;
    });
  }

  /// Launch a Save-File dialog on a background thread.
  void save_file(std::vector<nfd_filter> filters,
                 const char* default_name = nullptr,
                 const char* default_path = nullptr) {
    wait();
    launch([filters = std::move(filters), default_name,
            default_path]() -> std::optional<std::string> {
      std::vector<nfdu8filteritem_t> nfd_filters;
      nfd_filters.reserve(filters.size());
      for (auto& f : filters) nfd_filters.push_back({f.name, f.spec});

      nfdu8char_t* out = nullptr;
      nfdresult_t res = NFD_SaveDialogU8(
          &out, nfd_filters.data(),
          static_cast<nfdfiltersize_t>(nfd_filters.size()), default_path,
          default_name);
      if (res == NFD_OKAY && out) {
        std::string path(out);
        NFD_FreePathU8(out);
        return path;
      }
      return std::nullopt;
    });
  }

  /// Poll for a completed result.  Returns std::nullopt while the dialog
  /// is still open.  Returns optional<string> once done: the string if the
  /// user picked a file, or empty optional if they cancelled.
  std::optional<std::optional<std::string>> poll() {
    if (!future_.valid()) return std::nullopt;
    if (future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
      return std::nullopt;
    busy_.store(false, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
    return future_.get();
  }

 private:
  void wait() {
    if (thread_.joinable()) thread_.join();
    busy_.store(false, std::memory_order_release);
  }

  void launch(std::function<std::optional<std::string>()> fn) {
    busy_.store(true, std::memory_order_release);
    std::promise<std::optional<std::string>> promise;
    future_ = promise.get_future();
    thread_ = std::thread([fn = std::move(fn),
                           p = std::move(promise)]() mutable {
      p.set_value(fn());
    });
  }

  std::thread thread_;
  std::future<std::optional<std::string>> future_;
  std::atomic<bool> busy_{false};
};

}  // namespace jcan
