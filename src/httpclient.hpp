#pragma once

#include "orderbook.hpp"
#include <string>
#include <vector>

class HttpClient {
public:
  HttpClient();

  struct SnapshotResponse {
    std::vector<PriceLevel> bids, asks;
    uint64_t lastUpdateId;
    bool success;
  };

  SnapshotResponse getSnapshot(const std::string &symbol);

private:
  std::string host_, port_;
};
