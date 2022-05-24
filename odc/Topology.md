### odc::core::Topology Usage examples

With lambda:

```cpp
topo.AsyncChangeState(odc::core::TopoTransition::InitDevice,
                      std::chrono::milliseconds(500),
                      [](std::error_code ec, odc::core::TopoState state) {
        if (!ec) {
            // success
         } else if (ec.category().name() == "fairmq") {
            switch (static_cast<odc::ErrorCode>(ec.value())) {
              case odc::core::ErrorCode::OperationTimeout:
                // async operation timed out
              case odc::core::ErrorCode::OperationCanceled:
                // async operation canceled
              case odc::core::ErrorCode::DeviceChangeStateFailed:
                // failed to change state of a fairmq device
              default:
            }
        }
    }
);
```

With future:

```cpp
auto fut = topo.AsyncChangeState(odc::core::TopoTransition::InitDevice,
                                 std::chrono::milliseconds(500),
                                 boost::asio::use_future);
try {
    odc::core::TopoState state = fut.get();
    // success
} catch (const std::system_error& ex) {
    auto ec(ex.code());
    if (ec.category().name() == "odc") {
        switch (static_cast<odc::core::ErrorCode>(ec.value())) {
          case odc::core::ErrorCode::OperationTimeout:
            // async operation timed out
          case odc::core::ErrorCode::OperationCanceled:
            // async operation canceled
          case odc::core::ErrorCode::DeviceChangeStateFailed:
            // failed to change state of a fairmq device
          default:
        }
    }
}
```

With coroutine (C++20, see https://en.cppreference.com/w/cpp/language/coroutines):

```cpp
try {
    odc::core::TopoState state = co_await topo.AsyncChangeState(odc::core::TopoTransition::InitDevice,
                                                                std::chrono::milliseconds(500),
                                                                boost::asio::use_awaitable);
    // success
} catch (const std::system_error& ex) {
    auto ec(ex.code());
    if (ec.category().name() == "odc") {
        switch (static_cast<odc::core::ErrorCode>(ec.value())) {
          case odc::core::ErrorCode::OperationTimeout:
            // async operation timed out
          case odc::core::ErrorCode::OperationCanceled:
            // async operation canceled
          case odc::core::ErrorCode::DeviceChangeStateFailed:
            // failed to change state of a fairmq device
          default:
        }
    }
}
```
