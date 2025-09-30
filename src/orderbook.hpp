#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct PriceLevel {
  std::string price;
  std::string quantity;
};

struct DepthUpdate {
  uint64_t firstUpdateId, finalUpdateId;
  std::vector<PriceLevel> bids, asks;
};

// Descending sort for bids.
struct BidComparator {
  bool operator()(const std::string &a, const std::string &b) const {
    return std::stod(a) > std::stod(b);
  }
};

// Ascending sort for asks.
struct AskComparator {
  bool operator()(const std::string &a, const std::string &b) const {
    return std::stod(a) < std::stod(b);
  }
};

class OrderBook {
public:
  OrderBook();
  void setSnapshot(const std::vector<PriceLevel> &bids,
                   const std::vector<PriceLevel> &asks, uint64_t lastUpdateId);
  bool update(const DepthUpdate &update);
  void display() const;
  uint64_t getLastUpdateId() const;

private:
  std::map<std::string, std::string, BidComparator> bids_;
  std::map<std::string, std::string, AskComparator> asks_;
  uint64_t lastUpdateId_;
  // Mutable allows for access through a const object / member function.
  mutable std::mutex mutex_;
  void
  updatePriceLevels(std::map<std::string, std::string, BidComparator> &book,
                    const std::vector<PriceLevel> &levels);
  void
  updatePriceLevels(std::map<std::string, std::string, AskComparator> &book,
                    const std::vector<PriceLevel> &levels);
};
