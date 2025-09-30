# OrderBook

Submission for E4 Capital's take home assignment. Completed on Apple Silicon environment (MacOS). Boost is required, install either with `brew install boost` or clone from source.

Real-time order book client for Binance that maintains a synchronised local copy using WebSocket streams and REST API snapshots, implementing Binance's recommended synchronisation algorithm to ensure data consistency.

### Dependencies

- C++17 compiler
- CMake
- Boost 1.70+ (system, asio, beast)
- OpenSSL 1.1+
- nlohmann/json

### Build

```bash
chmod +x compile.sh
./compile.sh
./build/orderbook
```

### Components

```
┌───────────────────────────────┐
│       OrderBookManager        │
│- Orchestrates initialisation  │
│- Buffers WebSocket updates    │
│- Coordinates synchronisation  │
└──────────────┬────────────────┘
               │
         ┌─────┴─────┐
         │           │
         ▼           ▼
    ┌─────────┐  ┌─────────────┐
    │  HTTP   │  │  WebSocket  │
    │ Client  │  │   Client    │
    │         │  │             │
    │Snapshot │  │ Live Updates│
    └────┬────┘  └──────┬──────┘
         │              │
         └──────┬───────┘
                ▼
        ┌──────────────┐
        │  OrderBook   │
        │- Thread-safe │
        │- Sorted maps │
        └──────────────┘
```

### Synchronisation Protocol

Following [Binance's documentation](https://developers.binance.com/docs/binance-spot-api-docs/web-socket-streams#how-to-manage-a-local-order-book-correctly):

1. Open WebSocket connection to `wss://stream.binance.com:9443/ws/<symbol>@depth`
2. Buffer incoming messages while preparing snapshot
3. Fetch snapshot via `GET /api/v3/depth?symbol=<SYMBOL>&limit=5000`
4. Validate snapshot timing: If snapshot's `lastUpdateId` < first buffered event's `U`, refetch
5. Discard stale events: Remove buffered events where `u <= lastUpdateId`
6. Verify continuity: First valid event must have `U <= lastUpdateId + 1 <= u`
7. Apply updates: Process all subsequent events

### Update Application Rules

For each depth update event:

- Ignore old: If event's `u <= localLastUpdateId`, skip
- Detect gaps: If event's `U > localLastUpdateId + 1`, flag error
- Update prices:
  - If quantity = "0": remove price level
  - Otherwise: insert/update price level
- Advance state: Set `localLastUpdateId = event.u`

### STL Threading

- `std::thread`: WebSocket runs in separate thread
- `std::mutex`: Protects order book state
- `std::atomic<bool>`: Thread-safe running flag
- `std::queue`: Buffers updates during initialization

### Price Level Storage

```cpp
std::map<std::string, std::string, BidComparator> bids_;
std::map<std::string, std::string, AskComparator> asks_;
```

**Notes**

- Binance uses string representation to avoid float precision issues
- Preserves exact decimal values, compare via `std::stod()`
- Automatic sorting with `std::map` in O(logn) time via custom comparators

### Thread Safety

```cpp
mutable std::mutex mutex_;
```

- All public methods use `std::lock_guard<std::mutex>`
- Marked `mutable` to allow locking in const methods (e.g., `display()`)
- Coarse-grained locking sufficient, can improve with fine-grained locking / switch to lock free

### WebSocket Connection

```cpp
websocket::stream<beast::ssl_stream<tcp::socket>> ws(ioc, ctx);
```

- Uses SSL/TLS on port 9443 (Binance requirement)
- Certificate verification disabled (`ssl::verify_none`)
- Blocking reads in dedicated thread

### HTTP Snapshot Fetch

```cpp
beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);
```

- HTTPS to `api.binance.com:443`
- 30-second timeout for reliability
- Fetches 5000 levels (Binance maximum)

### Time Complexity

- Update application: O(logn) per price level via map insertion
- Top-N display: O(n) where n = 5
- Snapshot load: O(nlogn) for 5000 levels

### Potential Improvements

- Proper websocket failure support and rate limit handling
- Fine-grained locking / lock-free orderbook
- Better logging protocol with spdlog or quill
- Use a configuration file instead of hardcoding API links, configs, etc.
- GUI (Qt/QML)
- Performance metrics and logging
- Data replay

Thank you for checking out this project! :)

Done By: Yeoh Han Yi
