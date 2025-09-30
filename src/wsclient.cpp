#include "wsclient.hpp"
#include <algorithm>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <iostream>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

WebSocketClient::WebSocketClient(const std::string &symbol)
    : symbol_(symbol), host_("stream.binance.com"), port_("9443"),
      running_(false) {
  std::transform(symbol_.begin(), symbol_.end(), symbol_.begin(), ::tolower);
}

WebSocketClient::~WebSocketClient() { disconnect(); }

void WebSocketClient::setMessageCallback(MessageCallback callback) {
  callback_ = callback;
}

void WebSocketClient::connect() {
  if (running_)
    return;
  running_ = true;
  thread_ = std::make_unique<std::thread>(&WebSocketClient::runImpl, this);
}

void WebSocketClient::disconnect() {
  running_ = false;
  if (thread_ && thread_->joinable()) {
    thread_->join();
  }
}

void WebSocketClient::runImpl() {
  try {
    net::io_context ioc;
    ssl::context ctx(ssl::context::tlsv12_client);
    ctx.set_default_verify_paths();
    // Skip cert verification pls.
    ctx.set_verify_mode(ssl::verify_none);

    tcp::resolver resolver(ioc);
    websocket::stream<beast::ssl_stream<tcp::socket>> ws(ioc, ctx);

    std::cout << "Resolving." << std::endl;
    auto const results = resolver.resolve(host_, port_);

    std::cout << "Connecting." << std::endl;
    net::connect(beast::get_lowest_layer(ws), results);

    std::cout << "SSL handshaking." << std::endl;
    ws.next_layer().handshake(ssl::stream_base::client);

    // Set suggested timeout settings for the websocket.
    ws.set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::client));

    std::string path = "/ws/" + symbol_ + "@depth";
    std::cout << "WS handshake: " << path << std::endl;
    ws.handshake(host_, path);

    std::cout << "Connected! Waiting for messages." << std::endl;

    beast::flat_buffer buffer;
    int msg_count = 0;

    while (running_) {
      ws.read(buffer);

      msg_count++;
      if (msg_count <= 5 || msg_count % 100 == 0) {
        std::cout << "Msg #" << msg_count << std::endl;
      }

      std::string message = beast::buffers_to_string(buffer.data());
      buffer.consume(buffer.size());

      if (callback_) {
        try {
          DepthUpdate update = parseUpdate(message);
          callback_(update);
        } catch (const std::exception &e) {
          if (msg_count == 1) {
            std::cerr << "Parse error: " << e.what() << std::endl;
            std::cerr << "First message: " << message.substr(0, 200)
                      << std::endl;
          }
        }
      }
    }

    ws.close(websocket::close_code::normal);

  } catch (std::exception const &e) {
    std::cerr << "WS exception: " << e.what() << std::endl;
  }
}

DepthUpdate WebSocketClient::parseUpdate(const std::string &message) {
  DepthUpdate update;

  auto extractValue = [&](const std::string &key) -> std::string {
    std::string search = "\"" + key + "\":";
    size_t pos = message.find(search);
    if (pos == std::string::npos)
      return "";

    pos += search.length();
    size_t end = message.find_first_of(",}", pos);
    return message.substr(pos, end - pos);
  };

  auto extractArray = [&](const std::string &key) -> std::vector<PriceLevel> {
    std::vector<PriceLevel> levels;
    std::string search = "\"" + key + "\":[";
    size_t pos = message.find(search);
    if (pos == std::string::npos)
      return levels;

    pos += search.length();

    int bracket_count = 1;
    size_t end = pos;
    while (end < message.length() && bracket_count > 0) {
      if (message[end] == '[')
        bracket_count++;
      else if (message[end] == ']')
        bracket_count--;
      end++;
    }
    end--;

    std::string array_str = message.substr(pos, end - pos);

    size_t i = 0;
    while (i < array_str.length()) {
      if (array_str[i] == '[') {
        i++;
        size_t q1 = array_str.find('"', i);
        if (q1 == std::string::npos)
          break;
        size_t q2 = array_str.find('"', q1 + 1);
        if (q2 == std::string::npos)
          break;
        std::string price = array_str.substr(q1 + 1, q2 - q1 - 1);
        size_t q3 = array_str.find('"', q2 + 1);
        if (q3 == std::string::npos)
          break;
        size_t q4 = array_str.find('"', q3 + 1);
        if (q4 == std::string::npos)
          break;
        std::string qty = array_str.substr(q3 + 1, q4 - q3 - 1);
        levels.push_back({price, qty});
        i = array_str.find(']', q4) + 1;
      } else {
        i++;
      }
    }

    return levels;
  };

  update.firstUpdateId = std::stoull(extractValue("U"));
  update.finalUpdateId = std::stoull(extractValue("u"));
  update.bids = extractArray("b");
  update.asks = extractArray("a");

  return update;
}
