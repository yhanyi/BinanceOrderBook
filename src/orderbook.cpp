#include "orderbook.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

OrderBook::OrderBook() : lastUpdateId_(0) {}

void OrderBook::setSnapshot(const std::vector<PriceLevel> &bids,
                            const std::vector<PriceLevel> &asks,
                            uint64_t lastUpdateId) {
  // Write operation, lock guard.
  std::lock_guard<std::mutex> lock(mutex_);
  bids_.clear(), asks_.clear();
  for (const auto &level : bids) {
    if (level.quantity != "0") {
      bids_[level.price] = level.quantity;
    }
  }
  for (const auto &level : asks) {
    if (level.quantity != "0") {
      asks_[level.price] = level.quantity;
    }
  }
  lastUpdateId_ = lastUpdateId;
}

bool OrderBook::update(const DepthUpdate &update) {
  std::lock_guard<std::mutex> lock(mutex_);
  // Ignore old events.
  if (update.finalUpdateId <= lastUpdateId_) {
    return true;
  }
  // Check for gap.
  if (update.firstUpdateId > lastUpdateId_ + 1) {
    std::cerr << "Gap detected, expected update after" << lastUpdateId_
              << " but got " << update.firstUpdateId << std::endl;
    return false;
  }

  // Apply bid updates.
  for (const auto &level : update.bids) {
    if (level.quantity == "0" || level.quantity == "0.00000000") {
      bids_.erase(level.price);
    } else {
      bids_[level.price] = level.quantity;
    }
  }

  // Apply ask updates.
  for (const auto &level : update.asks) {
    if (level.quantity == "0" || level.quantity == "0.00000000") {
      asks_.erase(level.price);
    } else {
      asks_[level.price] = level.quantity;
    }
  }

  lastUpdateId_ = update.finalUpdateId;
  return true;
}

// Console display. Maybe can use QT or threadsafe logger?
void OrderBook::display() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "Order Book (Top 5)" << std::endl;
  std::cout << std::fixed << std::setprecision(8);

  // Display asks.
  std::cout << "--- ASKS ---" << std::endl;
  std::cout << std::setw(20) << "Price" << std::setw(20) << "Quantity"
            << std::endl;
  auto ask_it = asks_.begin();
  std::vector<std::pair<std::string, std::string>> top_asks;
  for (int i = 0; i < 5 && ask_it != asks_.end(); ++i, ++ask_it) {
    top_asks.push_back(*ask_it);
  }

  // Print asks in reverse.
  for (auto it = top_asks.rbegin(); it != top_asks.rend(); ++it) {
    std::cout << std::setw(20) << it->first << std::setw(20) << it->second
              << std::endl;
  }

  // Big midprice thing.
  if (!bids_.empty() && !asks_.empty()) {
    double best_bid = std::stod(bids_.begin()->first);
    double best_ask = std::stod(asks_.begin()->first);
    double mid = (best_bid + best_ask) / 2.0;
    std::cout << "--- MID: " << mid << " ---" << std::endl;
  }

  // Display bids.
  std::cout << "--- BIDS ---" << std::endl;
  std::cout << std::setw(20) << "Price" << std::setw(20) << "Quantity"
            << std::endl;

  auto bid_it = bids_.begin();
  for (int i = 0; i < 5 && bid_it != bids_.end(); ++i, ++bid_it) {
    std::cout << std::setw(20) << bid_it->first << std::setw(20)
              << bid_it->second << std::endl;
  }

  std::cout << "[Last Update ID: " << lastUpdateId_ << "]" << std::endl;
}

uint64_t OrderBook::getLastUpdateId() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return lastUpdateId_;
}
