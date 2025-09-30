#include "httpclient.hpp"
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <iostream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

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

    std::cout << "Parsing response body (length: " << res.body().length()
              << ")..." << std::endl;

    // Parse JSON.
    std::string body = res.body();

    std::cout << "Extracting lastUpdateId." << std::endl;

    size_t pos = body.find("\"lastUpdateId\":");
    if (pos != std::string::npos) {
      pos += 15;
      size_t end = body.find_first_of(",}", pos);
      response.lastUpdateId = std::stoull(body.substr(pos, end - pos));
      std::cout << "lastUpdateId: " << response.lastUpdateId << std::endl;
    } else {
      std::cerr << "Could not find lastUpdateId in response" << std::endl;
      return response;
    }

    // Extract bids.
    std::cout << "Extracting bids." << std::endl;
    pos = body.find("\"bids\":[");
    if (pos != std::string::npos) {
      pos += 8;
      size_t end = body.find("]", pos);
      std::string bids_str = body.substr(pos, end - pos);
      size_t i = 0;
      int count = 0;
      while (i < bids_str.length() && count < 5000) {
        if (bids_str[i] == '[') {
          i++;
          size_t quote1 = bids_str.find('"', i);
          if (quote1 == std::string::npos)
            break;
          size_t quote2 = bids_str.find('"', quote1 + 1);
          if (quote2 == std::string::npos)
            break;
          std::string price = bids_str.substr(quote1 + 1, quote2 - quote1 - 1);
          size_t quote3 = bids_str.find('"', quote2 + 1);
          if (quote3 == std::string::npos)
            break;
          size_t quote4 = bids_str.find('"', quote3 + 1);
          if (quote4 == std::string::npos)
            break;
          std::string qty = bids_str.substr(quote3 + 1, quote4 - quote3 - 1);
          response.bids.push_back({price, qty});
          i = bids_str.find(']', quote4) + 1;
          count++;
        } else {
          i++;
        }
      }
      std::cout << "Parsed " << response.bids.size() << " bids." << std::endl;
    } else {
      std::cerr << "Could not find bids in response." << std::endl;
      return response;
    }

    // Extract asks.
    std::cout << "Extracting asks." << std::endl;
    pos = body.find("\"asks\":[");
    if (pos != std::string::npos) {
      pos += 8;
      int bracket_count = 1;
      size_t end = pos;
      while (end < body.length() && bracket_count > 0) {
        if (body[end] == '[')
          bracket_count++;
        else if (body[end] == ']')
          bracket_count--;
        end++;
      }
      end--;
      std::string asks_str = body.substr(pos, end - pos);
      size_t i = 0;
      int count = 0;
      while (i < asks_str.length() && count < 5000) {
        if (asks_str[i] == '[') {
          i++;
          size_t quote1 = asks_str.find('"', i);
          if (quote1 == std::string::npos)
            break;
          size_t quote2 = asks_str.find('"', quote1 + 1);
          if (quote2 == std::string::npos)
            break;
          std::string price = asks_str.substr(quote1 + 1, quote2 - quote1 - 1);
          size_t quote3 = asks_str.find('"', quote2 + 1);
          if (quote3 == std::string::npos)
            break;
          size_t quote4 = asks_str.find('"', quote3 + 1);
          if (quote4 == std::string::npos)
            break;
          std::string qty = asks_str.substr(quote3 + 1, quote4 - quote3 - 1);
          response.asks.push_back({price, qty});
          i = asks_str.find(']', quote4) + 1;
          count++;
        } else {
          i++;
        }
      }
      std::cout << "Parsed " << response.asks.size() << " asks." << std::endl;
    }

    response.success = true;
    std::cout << "Snapshot received: " << response.bids.size() << " bids, "
              << response.asks.size() << " asks, "
              << "lastUpdateId=" << response.lastUpdateId << std::endl;

  } catch (std::exception const &e) {
    std::cerr << "HTTP Error: " << e.what() << std::endl;
  }

  return response;
}
