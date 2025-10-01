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
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;
using json = nlohmann::json;

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

    while (running_) {
      ws.read(buffer);

      std::string message = beast::buffers_to_string(buffer.data());
      buffer.consume(buffer.size());

      if (callback_) {
        try {
          DepthUpdate update = parseUpdate(message);
          callback_(update);
        } catch (const std::exception &e) {
          std::cerr << "Parse error: " << e.what() << std::endl;
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

  // Parse JSON message.
  json data = json::parse(message);

  if (!data.contains("U") || !data.contains("u") || !data.contains("b") ||
      !data.contains("a")) {
    throw std::runtime_error("Missing required fields in depth update.");
  }

  update.firstUpdateId = data["U"].get<uint64_t>();
  update.finalUpdateId = data["u"].get<uint64_t>();

  // Parse bids.
  for (const auto &bid : data["b"]) {
    if (bid.is_array() && bid.size() >= 2) {
      PriceLevel level;
      level.price = bid[0].get<std::string>();
      level.quantity = bid[1].get<std::string>();
      update.bids.push_back(level);
    }
  }

  // Parse asks.
  for (const auto &ask : data["a"]) {
    if (ask.is_array() && ask.size() >= 2) {
      PriceLevel level;
      level.price = ask[0].get<std::string>();
      level.quantity = ask[1].get<std::string>();
      update.asks.push_back(level);
    }
  }

  return update;
}
