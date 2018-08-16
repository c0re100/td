//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2018
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "td/utils/port/detail/NativeFd.h"
#include "td/utils/logging.h"

#include "td/utils/Status.h"
#include "td/utils/format.h"

#if TD_PORT_POSIX
#include <unistd.h>
#endif

namespace td {
NativeFd::NativeFd(NativeFd::Raw raw) : fd_(raw) {
  VLOG(fd) << *this << " create";
}
NativeFd::NativeFd(NativeFd::Raw raw, bool nolog) : fd_(raw) {
}
#if TD_PORT_WINDOWS
NativeFd::NativeFd(SOCKET raw) : fd_(reinterpret_cast<HANDLE>(raw)), is_socket_(true) {
  VLOG(fd) << *this << " create";
}
#endif
NativeFd::~NativeFd() {
  close();
}
NativeFd::operator bool() const {
  return fd_.get() != empty_raw();
}
constexpr NativeFd::Raw NativeFd::empty_raw() {
#if TD_PORT_POSIX
  return -1;
#elif TD_PORT_WINDOWS
  return INVALID_HANDLE_VALUE;
#endif
}
NativeFd::Raw NativeFd::raw() const {
  return fd_.get();
}
NativeFd::Raw NativeFd::fd() const {
  return raw();
}
#if TD_PORT_WINDOWS
NativeFd::Raw NativeFd::io_handle() const {
  return raw();
}
SOCKET NativeFd::socket() const {
  CHECK(is_socket_);
  return reinterpret_cast<SOCKET>(fd_.get());
}
#elif TD_PORT_POSIX
NativeFd::Raw NativeFd::socket() const {
  return raw();
}
#endif
void NativeFd::close() {
  if (!*this) {
    return;
  }
  VLOG(fd) << *this << " close";
#if TD_PORT_WINDOWS
  if (!CloseHandle(io_handle())) {
#elif TD_PORT_POSIX
  if (::close(fd()) < 0) {
#endif
    auto error = OS_ERROR("Close fd");
    LOG(ERROR) << error;
  }
  fd_ = {};
}
NativeFd::Raw NativeFd::release() {
  VLOG(fd) << *this << " release";
  auto res = fd_.get();
  fd_ = {};
  return res;
}

StringBuilder &operator<<(StringBuilder &sb, const NativeFd &fd) {
  sb << tag("fd", fd.raw());
  return sb;
}
}  // namespace td