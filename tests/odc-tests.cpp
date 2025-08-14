// Copyright 2020-2021 GSI, Inc. All rights reserved.
//
//

#define BOOST_TEST_MODULE odc_core_lib
#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>

#include "odc-fixtures.h"
#include <odc/AsioAsyncOp.h>
#include <odc/AsioBase.h>
#include <odc/Topology.h>

#include <array>
#include <boost/asio.hpp>
#include <thread>

using namespace boost::unit_test;
using namespace odc::core;

BOOST_AUTO_TEST_SUITE(async_op)

BOOST_AUTO_TEST_CASE(default_construction)
{
    AsioAsyncOp<DefaultExecutor, DefaultAllocator, void(std::error_code, int)> op;
    BOOST_REQUIRE(op.IsCompleted());
}

BOOST_AUTO_TEST_CASE(construction_with_handler)
{
    AsioAsyncOp<DefaultExecutor, DefaultAllocator, void(std::error_code, int)> op([](std::error_code, int) {});
    BOOST_REQUIRE(!op.IsCompleted());
}

BOOST_AUTO_TEST_CASE(complete)
{
    AsioAsyncOp<DefaultExecutor, DefaultAllocator, void(std::error_code, int)> op([](std::error_code ec, int v) {
        BOOST_CHECK(!ec); // success
        BOOST_CHECK_EQUAL(v, 42);
    });

    BOOST_REQUIRE(!op.IsCompleted());
    op.Complete(42);
    BOOST_REQUIRE(op.IsCompleted());

    BOOST_CHECK_THROW(op.Complete(6), RuntimeError); // No double completion!
}

BOOST_AUTO_TEST_CASE(cancel)
{
    AsioAsyncOp<DefaultExecutor, DefaultAllocator, void(std::error_code)> op([](std::error_code ec) {
        BOOST_CHECK(ec); // error
        BOOST_CHECK_EQUAL(ec, MakeErrorCode(ErrorCode::OperationCanceled));
    });

    op.Cancel();
}

BOOST_AUTO_TEST_CASE(timeout)
{
    AsyncOpFixture f;
    boost::asio::steady_timer timer(f.mIoContext.get_executor(), std::chrono::milliseconds(50));
    AsioAsyncOp<DefaultExecutor, DefaultAllocator, void(std::error_code)> op(f.mIoContext.get_executor(), [&timer](std::error_code ec) {
        timer.cancel();
        BOOST_TEST_MESSAGE("Completion with: " << ec.message());
        BOOST_CHECK(ec); // error
        BOOST_CHECK_EQUAL(ec, MakeErrorCode(ErrorCode::OperationTimeout));
    });
    timer.async_wait([&op](boost::system::error_code ec) {
        BOOST_TEST_MESSAGE("Timer event");
        if (ec != boost::asio::error::operation_aborted) {
            op.Timeout();
        }
    });

    f.mIoContext.run();
    BOOST_CHECK_THROW(op.Complete(), RuntimeError);
}

BOOST_AUTO_TEST_CASE(timeout2)
{
    AsyncOpFixture f;
    boost::asio::steady_timer timer(f.mIoContext.get_executor(), std::chrono::milliseconds(50));
    AsioAsyncOp<DefaultExecutor, DefaultAllocator, void(std::error_code)> op(f.mIoContext.get_executor(), [&timer](std::error_code ec) {
        timer.cancel();
        BOOST_TEST_MESSAGE("Completion with: " << ec.message());
        BOOST_CHECK(!ec); // success
    });
    op.Complete(); // Complete before timer
    timer.async_wait([&op](boost::system::error_code ec) {
        BOOST_TEST_MESSAGE("Timer event");
        if (ec != boost::asio::error::operation_aborted) {
            op.Timeout();
        }
    });

    f.mIoContext.run();
    BOOST_CHECK_THROW(op.Complete(), RuntimeError);
}

BOOST_AUTO_TEST_SUITE_END() // async_op

template<typename Functor>
void full_device_lifecycle(Functor&& functor)
{
    for (auto transition : { TopoTransition::InitDevice,
                             TopoTransition::CompleteInit,
                             TopoTransition::Bind,
                             TopoTransition::Connect,
                             TopoTransition::InitTask,
                             TopoTransition::Run,
                             TopoTransition::Stop,
                             TopoTransition::ResetTask,
                             TopoTransition::ResetDevice,
                             TopoTransition::End }) {
        functor(transition);
    }
}

BOOST_AUTO_TEST_SUITE(topology)

BOOST_AUTO_TEST_CASE(construction)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
}

BOOST_AUTO_TEST_CASE(construction2)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mIoContext.get_executor(), f.mDDSTopo, f.mSession);
}

BOOST_AUTO_TEST_CASE(async_change_state)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    SharedSemaphore blocker;
    Topology topo(f.mDDSTopo, f.mSession);
    topo.AsyncChangeState(TopoTransition::InitDevice, "", Duration(0), [=](std::error_code ec, TopoState) mutable {
        BOOST_TEST_MESSAGE(ec);
        BOOST_CHECK_EQUAL(ec, std::error_code());
        blocker.Signal();
    });
    blocker.Wait();
}

BOOST_AUTO_TEST_CASE(async_change_state_with_executor)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mIoContext.get_executor(), f.mDDSTopo, f.mSession);
    topo.AsyncChangeState(TopoTransition::InitDevice, "", Duration(0), [](std::error_code ec, TopoState) {
        BOOST_TEST_MESSAGE(ec);
        BOOST_CHECK_EQUAL(ec, std::error_code());
    });

    f.mIoContext.run();
}

// BOOST_AUTO_TEST_CASE(async_change_state_future)
// {
//     BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
//     BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
//     TopologyFixture f(framework::master_test_suite().argv[2]);

//     Topology topo(f.mIoContext.get_executor(), f.mDDSTopo, f.mSession);
//     auto fut(topo.AsyncChangeState(TopoTransition::InitDevice, "", Duration(0), boost::asio::use_future));
//     std::thread t([&]() { f.mIoContext.run(); });
//     bool success(false);

//     try {
//         auto state = fut.get();
//         success = true;
//     } catch (const std::system_error& ex) {
//         BOOST_TEST_MESSAGE(ex.what());
//     }

//     BOOST_TEST(success);
//     t.join();
// }

#if defined(ASIO_HAS_CO_AWAIT)
BOOST_AUTO_TEST_CASE(async_change_state_coroutine)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    bool success(false);
    boost::asio::co_spawn(
        f.mIoContext.get_executor(),
        [&]() mutable -> boost::asio::awaitable<void> {
            auto executor = co_await boost::asio::this_coro::executor;
            Topology topo(executor, f.mDDSTopo, f.mSession);
            try {
                TopoState state = co_await topo.AsyncChangeState(TopoTransition::InitDevice, "", Duration(0), asio::use_awaitable);
                success = true;
            } catch (const std::system_error& ex) {
                BOOST_TEST_MESSAGE(ex.what());
            }
        },
        boost::asio::detached);

    f.mIoContext.run();
    BOOST_REQUIRE(success);
}
#endif

BOOST_AUTO_TEST_CASE(change_state)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
    auto result(topo.ChangeState(TopoTransition::InitDevice));
    BOOST_TEST_MESSAGE(result.first);

    BOOST_CHECK_EQUAL(result.first, std::error_code());
    BOOST_CHECK_NO_THROW(AggregateState(result.second));
    BOOST_CHECK_EQUAL(StateEqualsTo(result.second, DeviceState::InitializingDevice), true);
    auto const currentState = topo.GetCurrentState();
    BOOST_CHECK_NO_THROW(AggregateState(currentState));
    BOOST_CHECK_EQUAL(StateEqualsTo(currentState, DeviceState::InitializingDevice), true);
}

BOOST_AUTO_TEST_CASE(mixed_state)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
    auto result1(topo.ChangeState(TopoTransition::InitDevice, ".*/Sampler.*"));
    BOOST_TEST_MESSAGE(result1.first);

    BOOST_CHECK_EQUAL(result1.first, std::error_code());
    BOOST_CHECK_EQUAL(AggregateState(result1.second), AggregatedState::Mixed);
    BOOST_CHECK_EQUAL(StateEqualsTo(result1.second, DeviceState::InitializingDevice), false);
    auto const currentState1 = topo.GetCurrentState();
    BOOST_CHECK_EQUAL(AggregateState(currentState1), AggregatedState::Mixed);
    BOOST_CHECK_EQUAL(StateEqualsTo(currentState1, DeviceState::InitializingDevice), false);

    auto result2(topo.ChangeState(TopoTransition::InitDevice, ".*/(Processor|Sink).*"));
    BOOST_TEST_MESSAGE(result2.first);

    BOOST_CHECK_EQUAL(result2.first, std::error_code());
    BOOST_CHECK_EQUAL(AggregateState(result2.second), AggregatedState::InitializingDevice);
    BOOST_CHECK_EQUAL(StateEqualsTo(result2.second, DeviceState::InitializingDevice), true);
    auto const currentState2 = topo.GetCurrentState();
    BOOST_CHECK_EQUAL(AggregateState(currentState2), AggregatedState::InitializingDevice);
    BOOST_CHECK_EQUAL(StateEqualsTo(currentState2, DeviceState::InitializingDevice), true);
}

BOOST_AUTO_TEST_CASE(async_change_state_concurrent)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
    topo.AsyncChangeState(TopoTransition::InitDevice, ".*/(Sampler|Sink).*", Duration(0), [](std::error_code ec, TopoState) mutable {
        BOOST_TEST_MESSAGE("ChangeState for Sampler|Sink: " << ec);
        BOOST_CHECK_EQUAL(ec, std::error_code());
    });
    topo.AsyncChangeState(TopoTransition::InitDevice, ".*/Processor.*", Duration(0), [](std::error_code ec, TopoState) mutable {
        BOOST_TEST_MESSAGE("ChangeState for Processors: " << ec);
        BOOST_CHECK_EQUAL(ec, std::error_code());
    });

    topo.WaitForState(DeviceState::Undefined, DeviceState::InitializingDevice);
    auto const currentState = topo.GetCurrentState();
    BOOST_CHECK_NO_THROW(AggregateState(currentState));
    BOOST_CHECK_EQUAL(StateEqualsTo(currentState, DeviceState::InitializingDevice), true);
}

BOOST_AUTO_TEST_CASE(async_change_state_timeout)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mIoContext.get_executor(), f.mDDSTopo, f.mSession);
    topo.AsyncChangeState(TopoTransition::InitDevice, "", std::chrono::milliseconds(1), [](std::error_code ec, TopoState) {
        BOOST_TEST_MESSAGE(ec);
        BOOST_CHECK_EQUAL(ec, MakeErrorCode(ErrorCode::OperationTimeout));
    });

    f.mIoContext.run();
}

BOOST_AUTO_TEST_CASE(async_change_state_collection_view)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    SharedSemaphore blocker;
    Topology topo(f.mDDSTopo, f.mSession);
    topo.AsyncChangeState(TopoTransition::InitDevice, "", Duration(0), [=](std::error_code ec, TopoState state) mutable {
        BOOST_TEST_MESSAGE(ec);
        TopoStateByCollection cstate(GroupByCollectionId(state));
        BOOST_TEST_MESSAGE("num collections: " << cstate.size());
        BOOST_REQUIRE_EQUAL(cstate.size(), 1);
        for (const auto& c : cstate) {
            BOOST_TEST_MESSAGE("\t" << c.first);
            AggregatedState s;
            BOOST_REQUIRE_NO_THROW(s = AggregateState(c.second));
            BOOST_REQUIRE_EQUAL(s, static_cast<AggregatedState>(fair::mq::State::InitializingDevice));
            BOOST_TEST_MESSAGE("\tAggregated state: " << s);
            for (const auto& ds : c.second) {
                BOOST_TEST_MESSAGE("\t\t" << ds.state);
            }
        }
        BOOST_CHECK_EQUAL(ec, std::error_code());
        blocker.Signal();
    });
    blocker.Wait();
}

BOOST_AUTO_TEST_CASE(change_state_full_device_lifecycle)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
    full_device_lifecycle([&](TopoTransition transition) { BOOST_CHECK_EQUAL(topo.ChangeState(transition).first, std::error_code()); });
}

BOOST_AUTO_TEST_CASE(wait_for_state_full_device_lifecycle)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
    topo.AsyncWaitForState(DeviceState::Undefined, DeviceState::ResettingDevice, "", Duration(0), [](std::error_code ec, FailedDevices failed) {
        BOOST_REQUIRE_EQUAL(ec, std::error_code());
        BOOST_REQUIRE_EQUAL(failed.size(), 0);
    });
    full_device_lifecycle([&](TopoTransition transition) {
        std::cout << "transition: " << transition << std::endl;
        topo.ChangeState(transition);
        auto const result = topo.WaitForState(DeviceState::Undefined, gExpectedState.at(transition));
        BOOST_REQUIRE_EQUAL(result.first, std::error_code());
        BOOST_REQUIRE_EQUAL(result.second.size(), 0);
    });
}

BOOST_AUTO_TEST_CASE(change_state_full_device_lifecycle2)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
    for (int i(0); i < 10; ++i) {
        for (auto transition : { TopoTransition::InitDevice,
                                 TopoTransition::CompleteInit,
                                 TopoTransition::Bind,
                                 TopoTransition::Connect,
                                 TopoTransition::InitTask,
                                 TopoTransition::Run }) {
            BOOST_REQUIRE_EQUAL(topo.ChangeState(transition).first, std::error_code());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for (auto transition : { TopoTransition::Stop, TopoTransition::ResetTask, TopoTransition::ResetDevice }) {
            BOOST_REQUIRE_EQUAL(topo.ChangeState(transition).first, std::error_code());
        }
    }
}

BOOST_AUTO_TEST_CASE(set_properties)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::InitDevice).first, std::error_code());

    auto const result1 = topo.SetProperties({ { "key1", "val1" } });
    BOOST_TEST_MESSAGE(result1.first);
    BOOST_REQUIRE_EQUAL(result1.first, std::error_code());
    BOOST_REQUIRE_EQUAL(result1.second.size(), 0);
    auto const result2 = topo.SetProperties({ { "key2", "val2" }, { "key3", "val3" } });
    BOOST_TEST_MESSAGE(result2.first);
    BOOST_REQUIRE_EQUAL(result2.first, std::error_code());
    BOOST_REQUIRE_EQUAL(result2.second.size(), 0);

    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::CompleteInit).first, std::error_code());
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::ResetDevice).first, std::error_code());
}

BOOST_AUTO_TEST_CASE(async_set_properties_concurrent)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::InitDevice).first, std::error_code());

    SharedSemaphore blocker(2);
    topo.AsyncSetProperties({ { "key1", "val1" } }, "", Duration(0), [=](std::error_code ec, FailedDevices failed) mutable {
        BOOST_TEST_MESSAGE(ec);
        BOOST_REQUIRE_EQUAL(ec, std::error_code());
        BOOST_REQUIRE_EQUAL(failed.size(), 0);
        blocker.Signal();
    });
    topo.AsyncSetProperties({ { "key2", "val2" }, { "key3", "val3" } }, "", Duration(0), [=](std::error_code ec, FailedDevices failed) mutable {
        BOOST_TEST_MESSAGE(ec);
        BOOST_REQUIRE_EQUAL(ec, std::error_code());
        BOOST_REQUIRE_EQUAL(failed.size(), 0);
        blocker.Signal();
    });
    blocker.Wait();

    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::CompleteInit).first, std::error_code());
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::ResetDevice).first, std::error_code());
}

BOOST_AUTO_TEST_CASE(async_set_properties_timeout)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::InitDevice).first, std::error_code());

    auto devices = topo.GetCurrentState();

    topo.AsyncSetProperties({ { "key1", "val1" } }, "", std::chrono::microseconds(1), [&](std::error_code ec, FailedDevices failed) mutable {
        BOOST_TEST_MESSAGE(ec);
        BOOST_CHECK_EQUAL(ec, MakeErrorCode(ErrorCode::OperationTimeout));
        BOOST_CHECK_EQUAL(failed.size(), devices.size());
        for (const auto& device : devices) {
            BOOST_CHECK_EQUAL(failed.count(device.taskId), 1);
        }
    });

    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::CompleteInit).first, std::error_code());
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::ResetDevice).first, std::error_code());
}

BOOST_AUTO_TEST_CASE(set_properties_mixed)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::InitDevice).first, std::error_code());

    SharedSemaphore blocker;
    topo.AsyncSetProperties({ { "key1", "val1" } }, "", Duration(0), [=](std::error_code ec, FailedDevices failed) mutable {
        BOOST_TEST_MESSAGE(ec);
        BOOST_REQUIRE_EQUAL(ec, std::error_code());
        BOOST_REQUIRE_EQUAL(failed.size(), 0);
        blocker.Signal();
    });

    auto result = topo.SetProperties({ { "key2", "val2" } });
    BOOST_TEST_MESSAGE(result.first);
    BOOST_REQUIRE_EQUAL(result.first, std::error_code());
    BOOST_REQUIRE_EQUAL(result.second.size(), 0);

    blocker.Wait();

    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::CompleteInit).first, std::error_code());
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::ResetDevice).first, std::error_code());
}

BOOST_AUTO_TEST_CASE(get_properties)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::InitDevice).first, std::error_code());

    auto const result = topo.GetProperties("^(session|id)$");
    BOOST_TEST_MESSAGE(result.first);
    BOOST_REQUIRE_EQUAL(result.first, std::error_code());
    BOOST_REQUIRE_EQUAL(result.second.failed.size(), 0);
    for (auto const& d : result.second.devices) {
        BOOST_TEST_MESSAGE(d.first);
        BOOST_REQUIRE_EQUAL(d.second.props.size(), 2);
        for (auto const& p : d.second.props) {
            BOOST_TEST_MESSAGE(" " << p.first << " : " << p.second);
        }
    }

    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::CompleteInit).first, std::error_code());
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::ResetDevice).first, std::error_code());
}

BOOST_AUTO_TEST_CASE(set_and_get_properties)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::InitDevice).first, std::error_code());

    DeviceProperties const props{ { "key1", "val1" }, { "key2", "val2" } };

    auto const result1 = topo.SetProperties(props);
    BOOST_TEST_MESSAGE(result1.first);
    BOOST_REQUIRE_EQUAL(result1.first, std::error_code());
    BOOST_REQUIRE_EQUAL(result1.second.size(), 0);

    auto const result2 = topo.GetProperties("^key.*");
    BOOST_TEST_MESSAGE(result2.first);
    BOOST_REQUIRE_EQUAL(result2.first, std::error_code());
    BOOST_REQUIRE_EQUAL(result2.second.failed.size(), 0);
    BOOST_REQUIRE_EQUAL(result2.second.devices.size(), 6);
    for (auto const& d : result2.second.devices) {
        BOOST_REQUIRE(d.second.props == props);
    }

    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::CompleteInit).first, std::error_code());
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopoTransition::ResetDevice).first, std::error_code());
}

BOOST_AUTO_TEST_CASE(aggregated_topology_state_comparison)
{
    BOOST_REQUIRE(DeviceState::Undefined == AggregatedState::Undefined);
    BOOST_REQUIRE(AggregatedState::Undefined == DeviceState::Undefined);
    BOOST_REQUIRE(DeviceState::Ok == AggregatedState::Ok);
    BOOST_REQUIRE(DeviceState::Error == AggregatedState::Error);
    BOOST_REQUIRE(DeviceState::Idle == AggregatedState::Idle);
    BOOST_REQUIRE(DeviceState::InitializingDevice == AggregatedState::InitializingDevice);
    BOOST_REQUIRE(DeviceState::Initialized == AggregatedState::Initialized);
    BOOST_REQUIRE(DeviceState::Binding == AggregatedState::Binding);
    BOOST_REQUIRE(DeviceState::Bound == AggregatedState::Bound);
    BOOST_REQUIRE(DeviceState::Connecting == AggregatedState::Connecting);
    BOOST_REQUIRE(DeviceState::DeviceReady == AggregatedState::DeviceReady);
    BOOST_REQUIRE(DeviceState::InitializingTask == AggregatedState::InitializingTask);
    BOOST_REQUIRE(DeviceState::Ready == AggregatedState::Ready);
    BOOST_REQUIRE(DeviceState::Running == AggregatedState::Running);
    BOOST_REQUIRE(DeviceState::ResettingTask == AggregatedState::ResettingTask);
    BOOST_REQUIRE(DeviceState::ResettingDevice == AggregatedState::ResettingDevice);
    BOOST_REQUIRE(DeviceState::Exiting == AggregatedState::Exiting);

    BOOST_REQUIRE(GetAggregatedState("UNDEFINED") == AggregatedState::Undefined);
    BOOST_REQUIRE(GetAggregatedState("OK") == AggregatedState::Ok);
    BOOST_REQUIRE(GetAggregatedState("ERROR") == AggregatedState::Error);
    BOOST_REQUIRE(GetAggregatedState("IDLE") == AggregatedState::Idle);
    BOOST_REQUIRE(GetAggregatedState("INITIALIZING DEVICE") == AggregatedState::InitializingDevice);
    BOOST_REQUIRE(GetAggregatedState("INITIALIZED") == AggregatedState::Initialized);
    BOOST_REQUIRE(GetAggregatedState("BINDING") == AggregatedState::Binding);
    BOOST_REQUIRE(GetAggregatedState("BOUND") == AggregatedState::Bound);
    BOOST_REQUIRE(GetAggregatedState("CONNECTING") == AggregatedState::Connecting);
    BOOST_REQUIRE(GetAggregatedState("DEVICE READY") == AggregatedState::DeviceReady);
    BOOST_REQUIRE(GetAggregatedState("INITIALIZING TASK") == AggregatedState::InitializingTask);
    BOOST_REQUIRE(GetAggregatedState("READY") == AggregatedState::Ready);
    BOOST_REQUIRE(GetAggregatedState("RUNNING") == AggregatedState::Running);
    BOOST_REQUIRE(GetAggregatedState("RESETTING TASK") == AggregatedState::ResettingTask);
    BOOST_REQUIRE(GetAggregatedState("RESETTING DEVICE") == AggregatedState::ResettingDevice);
    BOOST_REQUIRE(GetAggregatedState("EXITING") == AggregatedState::Exiting);
    BOOST_REQUIRE(GetAggregatedState("MIXED") == AggregatedState::Mixed);

    BOOST_REQUIRE("UNDEFINED" == GetAggregatedStateName(AggregatedState::Undefined));
    BOOST_REQUIRE("OK" == GetAggregatedStateName(AggregatedState::Ok));
    BOOST_REQUIRE("ERROR" == GetAggregatedStateName(AggregatedState::Error));
    BOOST_REQUIRE("IDLE" == GetAggregatedStateName(AggregatedState::Idle));
    BOOST_REQUIRE("INITIALIZING DEVICE" == GetAggregatedStateName(AggregatedState::InitializingDevice));
    BOOST_REQUIRE("INITIALIZED" == GetAggregatedStateName(AggregatedState::Initialized));
    BOOST_REQUIRE("BINDING" == GetAggregatedStateName(AggregatedState::Binding));
    BOOST_REQUIRE("BOUND" == GetAggregatedStateName(AggregatedState::Bound));
    BOOST_REQUIRE("CONNECTING" == GetAggregatedStateName(AggregatedState::Connecting));
    BOOST_REQUIRE("DEVICE READY" == GetAggregatedStateName(AggregatedState::DeviceReady));
    BOOST_REQUIRE("INITIALIZING TASK" == GetAggregatedStateName(AggregatedState::InitializingTask));
    BOOST_REQUIRE("READY" == GetAggregatedStateName(AggregatedState::Ready));
    BOOST_REQUIRE("RUNNING" == GetAggregatedStateName(AggregatedState::Running));
    BOOST_REQUIRE("RESETTING TASK" == GetAggregatedStateName(AggregatedState::ResettingTask));
    BOOST_REQUIRE("RESETTING DEVICE" == GetAggregatedStateName(AggregatedState::ResettingDevice));
    BOOST_REQUIRE("EXITING" == GetAggregatedStateName(AggregatedState::Exiting));
    BOOST_REQUIRE("MIXED" == GetAggregatedStateName(AggregatedState::Mixed));
}

BOOST_AUTO_TEST_CASE(aggregate_state_include_ignored_errors)
{
    // Test AggregateState function with includeIgnoredErrors parameter

    // Test case 1: All devices in OK state (ignored flag doesn't matter)
    DeviceStatus dev1, dev2;
    dev1.state = DeviceState::Ok;
    dev1.ignored = false;
    dev1.taskId = 1;
    dev2.state = DeviceState::Ok;
    dev2.ignored = true;  // ignored
    dev2.taskId = 2;

    TopoState allOkState = {dev1, dev2};
    BOOST_CHECK_EQUAL(AggregateState(allOkState), AggregatedState::Ok);
    BOOST_CHECK_EQUAL(AggregateState(allOkState, true), AggregatedState::Ok);

    // Test case 2: Mixed states with ignored error device
    DeviceStatus dev3, dev4;
    dev3.state = DeviceState::Ok;
    dev3.ignored = false;
    dev3.taskId = 3;
    dev4.state = DeviceState::Error;
    dev4.ignored = true;  // ignored error
    dev4.taskId = 4;

    TopoState mixedWithIgnoredError = {dev3, dev4};
    // Without includeIgnoredErrors, should return Ok (ignores error device)
    BOOST_CHECK_EQUAL(AggregateState(mixedWithIgnoredError), AggregatedState::Ok);
    // With includeIgnoredErrors, should return Error (includes ignored error device)
    BOOST_CHECK_EQUAL(AggregateState(mixedWithIgnoredError, true), AggregatedState::Error);

    // Test case 3: All devices ignored with errors
    DeviceStatus dev5, dev6;
    dev5.state = DeviceState::Error;
    dev5.ignored = true;
    dev5.taskId = 5;
    dev6.state = DeviceState::Error;
    dev6.ignored = true;
    dev6.taskId = 6;

    TopoState allIgnoredErrors = {dev5, dev6};
    // Without includeIgnoredErrors, should return Mixed (no non-ignored devices)
    BOOST_CHECK_EQUAL(AggregateState(allIgnoredErrors), AggregatedState::Mixed);
    // With includeIgnoredErrors, should return Error
    BOOST_CHECK_EQUAL(AggregateState(allIgnoredErrors, true), AggregatedState::Error);

    // Test case 4: Mix of ignored and non-ignored devices, some with errors
    DeviceStatus dev7, dev8, dev9;
    dev7.state = DeviceState::Ready;
    dev7.ignored = false;
    dev7.taskId = 7;
    dev8.state = DeviceState::Error;
    dev8.ignored = true;  // ignored error
    dev8.taskId = 8;
    dev9.state = DeviceState::Ready;
    dev9.ignored = false;
    dev9.taskId = 9;

    TopoState complexMix = {dev7, dev8, dev9};
    // Without includeIgnoredErrors, should return Ready (homogeneous non-ignored devices)
    BOOST_CHECK_EQUAL(AggregateState(complexMix), AggregatedState::Ready);
    // With includeIgnoredErrors, should return Error (includes ignored error)
    BOOST_CHECK_EQUAL(AggregateState(complexMix, true), AggregatedState::Error);

    // Test case 5: Non-ignored error device (should always return Error)
    DeviceStatus dev10, dev11;
    dev10.state = DeviceState::Ready;
    dev10.ignored = false;
    dev10.taskId = 10;
    dev11.state = DeviceState::Error;
    dev11.ignored = false;  // non-ignored error
    dev11.taskId = 11;

    TopoState nonIgnoredError = {dev10, dev11};
    BOOST_CHECK_EQUAL(AggregateState(nonIgnoredError), AggregatedState::Error);
    BOOST_CHECK_EQUAL(AggregateState(nonIgnoredError, true), AggregatedState::Error);

    // Test case 6: Ignored non-error devices (should be handled normally)
    DeviceStatus dev12, dev13;
    dev12.state = DeviceState::Ready;
    dev12.ignored = false;
    dev12.taskId = 12;
    dev13.state = DeviceState::Idle;
    dev13.ignored = true;  // ignored non-error
    dev13.taskId = 13;

    TopoState ignoredNonError = {dev12, dev13};
    // Both should return Ready (ignored non-error devices don't affect result)
    BOOST_CHECK_EQUAL(AggregateState(ignoredNonError), AggregatedState::Ready);
    BOOST_CHECK_EQUAL(AggregateState(ignoredNonError, true), AggregatedState::Ready);
}

BOOST_AUTO_TEST_CASE(device_crashed)
{
    using namespace std::chrono_literals;
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    {
        Topology topo(f.mDDSTopo, f.mSession);
        BOOST_CHECK_EQUAL(topo.ChangeState(TopoTransition::InitDevice).first, std::error_code());
        BOOST_CHECK_EQUAL(topo.ChangeState(TopoTransition::CompleteInit).first, std::error_code());
        try {
            topo.SetProperties({ { "crash", "yes" } }, "", 10ms);
        } catch (std::system_error const& e) {
            BOOST_TEST_MESSAGE("system_error >> code: " << e.code() << ", what: " << e.what());
        }
        BOOST_TEST_CHECKPOINT("Processors crashed.");
    }
    BOOST_TEST_CHECKPOINT("Topology destructed.");
}

BOOST_AUTO_TEST_CASE(underlying_session_terminated)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    {
        Topology topo(f.mDDSTopo, f.mSession);
        BOOST_CHECK_EQUAL(topo.ChangeState(TopoTransition::InitDevice).first, std::error_code());
        BOOST_CHECK_EQUAL(topo.ChangeState(TopoTransition::CompleteInit).first, std::error_code());
        f.mSession.mDDSSession.shutdown();
        BOOST_TEST_CHECKPOINT("Session shut down.");
    }
    BOOST_TEST_CHECKPOINT("Topology destructed.");
}

BOOST_AUTO_TEST_SUITE_END() // topology

BOOST_AUTO_TEST_SUITE(multiple_topologies)

BOOST_AUTO_TEST_CASE(construction)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");

    constexpr auto num(3);
    std::array<TopologyFixture, num> f{ TopologyFixture(framework::master_test_suite().argv[2]),
                                        TopologyFixture(framework::master_test_suite().argv[2]),
                                        TopologyFixture(framework::master_test_suite().argv[2]) };

    boost::asio::io_context ioContext;
    std::array<Topology, num> topos{
        Topology(ioContext.get_executor(), f[0].mDDSTopo, f[0].mSession),
        Topology(ioContext.get_executor(), f[1].mDDSTopo, f[1].mSession),
        Topology(ioContext.get_executor(), f[2].mDDSTopo, f[2].mSession),
    };
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(change_state_full_lifecycle_serial)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");

    constexpr auto num(3);
    std::array<TopologyFixture, num> f{ TopologyFixture(framework::master_test_suite().argv[2]),
                                        TopologyFixture(framework::master_test_suite().argv[2]),
                                        TopologyFixture(framework::master_test_suite().argv[2]) };

    std::array<Topology, num> topos{
        Topology(f[0].mDDSTopo, f[0].mSession),
        Topology(f[1].mDDSTopo, f[1].mSession),
        Topology(f[2].mDDSTopo, f[2].mSession),
    };

    boost::asio::io_context ioContext;
    // schedule transitions serial
    for (int i = 0; i < num; ++i) {
        full_device_lifecycle([&](TopoTransition transition) {
            ioContext.post([&f, &topos, i, transition]() {
                auto [ec, state] = topos[i].ChangeState(transition);
                BOOST_REQUIRE_EQUAL(ec, std::error_code());
                BOOST_TEST_MESSAGE(f[i].mSession.mDDSSession.getSessionID() << ": " << AggregateState(state) << " -> " << ec);
            });
        });
    }
    ioContext.run();
}

BOOST_AUTO_TEST_CASE(change_state_full_lifecycle_interleaved)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");

    constexpr auto num(3);
    std::array<TopologyFixture, num> f{ TopologyFixture(framework::master_test_suite().argv[2]),
                                        TopologyFixture(framework::master_test_suite().argv[2]),
                                        TopologyFixture(framework::master_test_suite().argv[2]) };

    std::array<Topology, num> topos{
        Topology(f[0].mDDSTopo, f[0].mSession),
        Topology(f[1].mDDSTopo, f[1].mSession),
        Topology(f[2].mDDSTopo, f[2].mSession),
    };

    boost::asio::io_context ioContext;
    // schedule transitions interleaved
    full_device_lifecycle([&](TopoTransition transition) {
        for (int i = 0; i < num; ++i) {
            ioContext.post([&f, &topos, i, transition]() {
                auto [ec, state] = topos[i].ChangeState(transition);
                BOOST_REQUIRE_EQUAL(ec, std::error_code());
                BOOST_TEST_MESSAGE(f[i].mSession.mDDSSession.getSessionID() << ": " << AggregateState(state) << " -> " << ec);
            });
        }
    });
    ioContext.run();
}

auto run_full_cycle()
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mSession);
    full_device_lifecycle([&](TopoTransition transition) { BOOST_REQUIRE_EQUAL(topo.ChangeState(transition).first, std::error_code()); });
}

BOOST_AUTO_TEST_CASE(change_state_full_lifecycle_concurrent)
{
    std::thread t0(&run_full_cycle);
    std::thread t1(&run_full_cycle);
    run_full_cycle();
    t0.join();
    t1.join();
}

BOOST_AUTO_TEST_SUITE_END() // multiple_topologies

int main(int argc, char* argv[]) { return boost::unit_test::unit_test_main(init_unit_test, argc, argv); }
