#pragma once

#include <stdexcept>
#include <chrono>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

#include "helpers.h"
#include <tscore/BufferWriter.h>

struct LocalSocketClient {
  using self_reference = LocalSocketClient &;

  LocalSocketClient(std::string path) : _state{State::DISCONNECTED}, _path{std::move(path)} {}
  LocalSocketClient() : _state{State::DISCONNECTED}, _path{"/tmp/jsonrpc20.sock"} {}

  ~LocalSocketClient() { this->disconnect(); }
  self_reference
  connect()
  {
    if (_state == State::CONNECTED) {
      return *this;
    }
    _sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_sock < 0) {
      Debug("rpc", "error reading stream message :%s", std ::strerror(errno));
      throw std::runtime_error{std::strerror(errno)};
    }
    _server.sun_family = AF_UNIX;
    strcpy(_server.sun_path, _path.c_str());
    if (::connect(_sock, (struct sockaddr *)&_server, sizeof(struct sockaddr_un)) < 0) {
      this->close();
      Debug("rpc", "error reading stream message :%s", std ::strerror(errno));
      throw std::runtime_error{std::strerror(errno)};
    }
    _state = State::CONNECTED;
    return *this;
  }

  bool
  is_connected()
  {
    return _state == State::CONNECTED;
  }

  template <std::size_t N>
  self_reference
  send_in_chunks_with_wait(std::string_view data, std::chrono::milliseconds wait_between_write)
  {
    assert(_state == State::CONNECTED || _state == State::SENT);

    auto chunks = chunk<N>(data);
    for (auto &&part : chunks) {
      if (::write(_sock, part.c_str(), part.size()) < 0) {
        Debug("rpc", "error reading stream message :%s", std ::strerror(errno));
      }
      std::this_thread::sleep_for(wait_between_write);
    }
    _state = State::SENT;
    return *this;
  }

  self_reference
  send(std::string_view data)
  {
    assert(_state == State::CONNECTED || _state == State::SENT);

    if (::write(_sock, data.data(), data.size()) < 0) {
      std::cout << "Error writing on stream socket" << std ::strerror(errno) << '\n';
    }
    _state = State::SENT;

    return *this;
  }

  std::string
  read()
  {
    assert(_state == State::SENT);
    char buf[1024]; // should be enough
    bzero(buf, 1024);
    ssize_t rval{-1};

    if (rval = ::read(_sock, buf, 1024); rval <= 0) {
      // std::cout << "error reading stream message :" << std ::strerror(errno) << ", socket:" << _sock << '\n';
      Debug("rpc", "error reading stream message :%s, socket: %d", std ::strerror(errno), _sock);
      this->disconnect();
      return {};
    }
    _state = State::RECEIVED;

    return {buf, static_cast<std::size_t>(rval)};
  }

  void
  disconnect()
  {
    this->close();
    _state = State::DISCONNECTED;
  }

  void
  close()
  {
    if (_sock > 0) {
      ::close(_sock);
    }
  }

private:
  enum class State { CONNECTED, DISCONNECTED, SENT, RECEIVED };
  State _state;
  std::string _path;

  int _sock{-1};
  struct sockaddr_un _server;
};

struct ScopedLocalSocket : LocalSocketClient {
  // TODO, use another path.
  ScopedLocalSocket() : LocalSocketClient("/tmp/jsonrpc20.sock") {}
  ~ScopedLocalSocket() { LocalSocketClient::disconnect(); }
};
