#include "httpclient.hpp"
#include "orderbook.hpp"
#include "wsclient.hpp"
#include <chrono>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class OrderBookManager {
public:
  OrderBookManager(const std::string &symbol)
      : symbol_(symbol), orderBook_(), httpClient_(), wsClient_(symbol),
        snapshotReceived_(false) {}

  void start() {
    // Set up WebSocket callback.
    wsClient_.setMessageCallback(
        [this](const DepthUpdate &update) { this->onDepthUpdate(update); });

    // Start WebSocket connection.
    wsClient_.connect();

    // Buffer initial messages for a longer period.
    std::cout << "Buffering WebSocket messages." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Fetch snapshot.
    bool success = false;
    int attempts = 0;
    const int maxAttempts = 3;

    while (!success && attempts < maxAttempts) {
      std::cout << "Fetching depth snapshot (attempt " << (attempts + 1) << ")."
                << std::endl;

      auto snapshot = httpClient_.getSnapshot(symbol_);

      if (!snapshot.success) {
        attempts++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        continue;
      }

      // Check if need to re-fetch.
      std::lock_guard<std::mutex> lock(bufferMutex_);
      if (!updateBuffer_.empty()) {
        uint64_t firstBufferedU = updateBuffer_.front().firstUpdateId;

        if (snapshot.lastUpdateId < firstBufferedU) {
          std::cout << "Refetching snapshot." << std::endl;
          attempts++;
          std::this_thread::sleep_for(std::chrono::seconds(1));
          continue;
        }
      }

      // Apply snapshot.
      orderBook_.setSnapshot(snapshot.bids, snapshot.asks,
                             snapshot.lastUpdateId);
      snapshotReceived_ = true;

      // Process buffered updates.
      processBufferedUpdates(snapshot.lastUpdateId);

      success = true;
    }

    if (!success) {
      std::cerr << "Failed to initialise order book after " << maxAttempts
                << " attempts." << std::endl;
      return;
    }

    std::cout << "Order book initialised." << std::endl;
    orderBook_.display();

    // Loop to periodically redisplay order book.
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      orderBook_.display();
    }
  }

private:
  std::string symbol_;
  OrderBook orderBook_;
  HttpClient httpClient_;
  WebSocketClient wsClient_;

  std::queue<DepthUpdate> updateBuffer_;
  std::mutex bufferMutex_;
  bool snapshotReceived_;

  void onDepthUpdate(const DepthUpdate &update) {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    if (!snapshotReceived_) {
      updateBuffer_.push(update);
      if (updateBuffer_.size() == 1) {
        std::cout << "First buffered update: U=" << update.firstUpdateId
                  << ", u=" << update.finalUpdateId << std::endl;
      }
      if (updateBuffer_.size() % 50 == 0) {
        std::cout << "Buffered " << updateBuffer_.size() << " updates..."
                  << std::endl;
      }
    } else {
      // Apply update after snapshot.
      if (!orderBook_.update(update)) {
        std::cerr << "Failed to apply update U=" << update.firstUpdateId
                  << ", u=" << update.finalUpdateId << std::endl;
      }
    }
  }

  void processBufferedUpdates(uint64_t snapshotLastUpdateId) {
    std::cout << "Processing buffered updates." << std::endl;
    std::cout << "Snapshot lastUpdateId: " << snapshotLastUpdateId << std::endl;
    std::cout << "Buffer count: " << updateBuffer_.size() << std::endl;

    if (updateBuffer_.empty()) {
      std::cout << "No buffered updates, might not be "
                   "receiving messages."
                << std::endl;
      return;
    }

    int discarded = 0;
    int applied = 0;

    while (!updateBuffer_.empty()) {
      DepthUpdate update = updateBuffer_.front();
      updateBuffer_.pop();

      if (discarded == 0 && applied == 0) {
        std::cout << "First buffered update: U=" << update.firstUpdateId
                  << ", u=" << update.finalUpdateId << std::endl;
      }

      // Discard events where u <= lastUpdateId.
      if (update.finalUpdateId <= snapshotLastUpdateId) {
        discarded++;
        continue;
      }

      // First valid event should have lastUpdateId within [U, u].
      if (applied == 0) {
        if (update.firstUpdateId > snapshotLastUpdateId + 1) {
          std::cerr << "Gap between snapshot and first valid update."
                    << std::endl;
          std::cerr << "Snapshot lastUpdateId: " << snapshotLastUpdateId
                    << std::endl;
          std::cerr << "First update U: " << update.firstUpdateId << std::endl;
        }
      }

      if (orderBook_.update(update)) {
        applied++;
      }
    }

    std::cout << "Discarded " << discarded << " old updates, applied "
              << applied << " updates." << std::endl;
  }
};

int main() {
  std::string symbol;

  std::cout << "Enter Binance symbol (e.g., BTCUSDT, ETHUSDT): ";
  std::getline(std::cin, symbol);

  if (symbol.empty()) {
    std::cerr << "Invalid symbol" << std::endl;
    return 1;
  }

  // Convert to uppercase for API.
  std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);

  std::cout << "Starting order book for " << symbol << "." << std::endl;
  std::cout << "Press Ctrl-C to exit." << std::endl;

  try {
    OrderBookManager manager(symbol);
    manager.start();
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
