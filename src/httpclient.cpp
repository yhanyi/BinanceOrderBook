#include "httpclient.hpp"
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;
using json = nlohmann::json;

HttpClient::HttpClient() : host_("api.binance.com"), port_("443") {}

HttpClient::SnapshotResponse
HttpClient::getSnapshot(const std::string &symbol) {
  SnapshotResponse response;
  response.success = false;

  try {
    std::cout << "Creating SSL context." << std::endl;
    net::io_context ioc;
    ssl::context ctx(ssl::context::tlsv12_client);
    ctx.set_default_verify_paths();
    std::cout << "SSL context created." << std::endl;

    std::cout << "Resolving DNS for " << host_ << "." << std::endl;
    tcp::resolver resolver(ioc);
    beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

    if (!SSL_set_tlsext_host_name(stream.native_handle(), host_.c_str())) {
      throw beast::system_error(
          beast::error_code(static_cast<int>(::ERR_get_error()),
                            net::error::get_ssl_category()),
          "Failed to set SNI.");
    }

    std::cout << "Connecting to " << host_ << ":" << port_ << "." << std::endl;
    auto const results = resolver.resolve(host_, port_);
    beast::get_lowest_layer(stream).connect(results);
    std::cout << "TCP connected, starting SSL handshake." << std::endl;
    stream.handshake(ssl::stream_base::client);
    std::cout << "SSL handshake complete." << std::endl;

    std::string target = "/api/v3/depth?symbol=" + symbol + "&limit=5000";
    http::request<http::string_body> req{http::verb::get, target, 11};
    req.set(http::field::host, host_);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    std::cout << "Sending HTTP request: GET " << target << std::endl;
    http::write(stream, req);
    std::cout << "Request sent, waiting for response." << std::endl;

    beast::flat_buffer buffer;
    http::response<http::string_body> res;

    beast::error_code ec;
    http::read(stream, buffer, res, ec);

    if (ec) {
      std::cerr << "HTTP read error: " << ec.message() << std::endl;
      return response;
    }

    std::cout << "Response received, status: " << res.result_int() << std::endl;

    stream.shutdown(ec);

    if (res.result() != http::status::ok) {
      std::cerr << "HTTP error: " << res.result_int() << std::endl;
      return response;
    }

    // Parse JSON response.
    json data = json::parse(res.body());

    if (!data.contains("lastUpdateId") || !data.contains("bids") ||
        !data.contains("asks")) {
      std::cerr << "Missing required fields in snapshot response" << std::endl;
      return response;
    }

    response.lastUpdateId = data["lastUpdateId"].get<uint64_t>();

    // Parse bids.
    for (const auto &bid : data["bids"]) {
      if (bid.is_array() && bid.size() >= 2) {
        PriceLevel level;
        level.price = bid[0].get<std::string>();
        level.quantity = bid[1].get<std::string>();
        response.bids.push_back(level);
      }
    }

    // Parse asks.
    for (const auto &ask : data["asks"]) {
      if (ask.is_array() && ask.size() >= 2) {
        PriceLevel level;
        level.price = ask[0].get<std::string>();
        level.quantity = ask[1].get<std::string>();
        response.asks.push_back(level);
      }
    }

    response.success = true;
    std::cout << "Fetched snapshot: " << response.bids.size() << " bids, "
              << response.asks.size()
              << " asks, lastUpdateId=" << response.lastUpdateId << std::endl;

  } catch (const json::exception &e) {
    std::cerr << "JSON parse error: " << e.what() << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "HTTP error: " << e.what() << std::endl;
  }
  return response;
}
