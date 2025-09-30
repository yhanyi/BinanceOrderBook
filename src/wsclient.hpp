#pragma once

#include "orderbook.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

class WebSocketClient {
public:
  // Callback function for when a message / depth update is received.
  using MessageCallback = std::function<void(const DepthUpdate &)>;

  WebSocketClient(const std::string &symbol);
  ~WebSocketClient();

  void connect();
  void disconnect();
  void setMessageCallback(MessageCallback callback);

private:
  std::string symbol_;
  std::string host_;
  std::string port_;
  MessageCallback callback_;
  std::atomic<bool> running_;
  std::unique_ptr<std::thread> thread_;

  void runImpl();
  // Processes a raw message from websocket into DepthUpdate.
  DepthUpdate parseUpdate(const std::string &message);
};
