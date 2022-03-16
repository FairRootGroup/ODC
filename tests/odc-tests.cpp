// Copyright 2020-2021 GSI, Inc. All rights reserved.
//
//

#define BOOST_TEST_MODULE odc_core_lib
#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>

#include <odc/AsioAsyncOp.h>
#include <odc/AsioBase.h>
#include <odc/Topology.h>
#include "odc-fixtures.h"

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
    AsioAsyncOp<DefaultExecutor, DefaultAllocator, void(std::error_code, int)> op(
        [](std::error_code ec, int v)
        {
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
    AsioAsyncOp<DefaultExecutor, DefaultAllocator, void(std::error_code)> op(
        [](std::error_code ec)
        {
            BOOST_CHECK(ec); // error
            BOOST_CHECK_EQUAL(ec, MakeErrorCode(ErrorCode::OperationCanceled));
        });

    op.Cancel();
}

BOOST_AUTO_TEST_CASE(timeout)
{
    AsyncOpFixture f;
    boost::asio::steady_timer timer(f.mIoContext.get_executor(), std::chrono::milliseconds(50));
    AsioAsyncOp<DefaultExecutor, DefaultAllocator, void(std::error_code)> op(
        f.mIoContext.get_executor(),
        [&timer](std::error_code ec)
        {
            timer.cancel();
            BOOST_TEST_MESSAGE("Completion with: " << ec.message());
            BOOST_CHECK(ec); // error
            BOOST_CHECK_EQUAL(ec, MakeErrorCode(ErrorCode::OperationTimeout));
        });
    timer.async_wait(
        [&op](boost::system::error_code ec)
        {
            BOOST_TEST_MESSAGE("Timer event");
            if (ec != boost::asio::error::operation_aborted)
            {
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
    AsioAsyncOp<DefaultExecutor, DefaultAllocator, void(std::error_code)> op(
        f.mIoContext.get_executor(),
        [&timer](std::error_code ec)
        {
            timer.cancel();
            BOOST_TEST_MESSAGE("Completion with: " << ec.message());
            BOOST_CHECK(!ec); // success
        });
    op.Complete(); // Complete before timer
    timer.async_wait(
        [&op](boost::system::error_code ec)
        {
            BOOST_TEST_MESSAGE("Timer event");
            if (ec != boost::asio::error::operation_aborted)
            {
                op.Timeout();
            }
        });

    f.mIoContext.run();
    BOOST_CHECK_THROW(op.Complete(), RuntimeError);
}

BOOST_AUTO_TEST_SUITE_END() // async_op

template <typename Functor>
void full_device_lifecycle(Functor&& functor)
{
    for (auto transition : { TopologyTransition::InitDevice,
                             TopologyTransition::CompleteInit,
                             TopologyTransition::Bind,
                             TopologyTransition::Connect,
                             TopologyTransition::InitTask,
                             TopologyTransition::Run,
                             TopologyTransition::Stop,
                             TopologyTransition::ResetTask,
                             TopologyTransition::ResetDevice,
                             TopologyTransition::End })
    {
        functor(transition);
    }
}

BOOST_AUTO_TEST_SUITE(topology)

BOOST_AUTO_TEST_CASE(construction)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mDDSSession);
}

BOOST_AUTO_TEST_CASE(construction2)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mIoContext.get_executor(), f.mDDSTopo, f.mDDSSession);
}

BOOST_AUTO_TEST_CASE(async_change_state)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    SharedSemaphore blocker;
    Topology topo(f.mDDSTopo, f.mDDSSession);
    topo.AsyncChangeState(TopologyTransition::InitDevice,
                          [=](std::error_code ec, TopologyState) mutable
                          {
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

    Topology topo(f.mIoContext.get_executor(), f.mDDSTopo, f.mDDSSession);
    topo.AsyncChangeState(TopologyTransition::InitDevice,
                          [](std::error_code ec, TopologyState)
                          {
                              BOOST_TEST_MESSAGE(ec);
                              BOOST_CHECK_EQUAL(ec, std::error_code());
                          });

    f.mIoContext.run();
}

BOOST_AUTO_TEST_CASE(async_change_state_future)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mIoContext.get_executor(), f.mDDSTopo, f.mDDSSession);
    auto fut(topo.AsyncChangeState(TopologyTransition::InitDevice, boost::asio::use_future));
    std::thread t([&]() { f.mIoContext.run(); });
    bool success(false);

    try
    {
        auto state = fut.get();
        success = true;
    }
    catch (const std::system_error& ex)
    {
        BOOST_TEST_MESSAGE(ex.what());
    }

    BOOST_TEST(success);
    t.join();
}

#if defined(ASIO_HAS_CO_AWAIT)
BOOST_AUTO_TEST_CASE(async_change_state_coroutine)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    bool success(false);
    boost::asio::co_spawn(
        f.mIoContext.get_executor(),
        [&]() mutable -> boost::asio::awaitable<void>
        {
            auto executor = co_await boost::asio::this_coro::executor;
            Topology topo(executor, f.mDDSTopo, f.mDDSSession);
            try
            {
                TopologyState state =
                    co_await topo.AsyncChangeState(TopologyTransition::InitDevice, asio::use_awaitable);
                success = true;
            }
            catch (const std::system_error& ex)
            {
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

    Topology topo(f.mDDSTopo, f.mDDSSession);
    auto result(topo.ChangeState(TopologyTransition::InitDevice));
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

    Topology topo(f.mDDSTopo, f.mDDSSession);
    auto result1(topo.ChangeState(TopologyTransition::InitDevice, ".*/Sampler.*"));
    BOOST_TEST_MESSAGE(result1.first);

    BOOST_CHECK_EQUAL(result1.first, std::error_code());
    BOOST_CHECK_EQUAL(AggregateState(result1.second), AggregatedState::Mixed);
    BOOST_CHECK_EQUAL(StateEqualsTo(result1.second, DeviceState::InitializingDevice), false);
    auto const currentState1 = topo.GetCurrentState();
    BOOST_CHECK_EQUAL(AggregateState(currentState1), AggregatedState::Mixed);
    BOOST_CHECK_EQUAL(StateEqualsTo(currentState1, DeviceState::InitializingDevice), false);

    auto result2(topo.ChangeState(TopologyTransition::InitDevice, ".*/(Processor|Sink).*"));
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

    Topology topo(f.mDDSTopo, f.mDDSSession);
    topo.AsyncChangeState(TopologyTransition::InitDevice,
                          ".*/(Sampler|Sink).*",
                          [](std::error_code ec, TopologyState) mutable
                          {
                              BOOST_TEST_MESSAGE("ChangeState for Sampler|Sink: " << ec);
                              BOOST_CHECK_EQUAL(ec, std::error_code());
                          });
    topo.AsyncChangeState(TopologyTransition::InitDevice,
                          ".*/Processor.*",
                          [](std::error_code ec, TopologyState) mutable
                          {
                              BOOST_TEST_MESSAGE("ChangeState for Processors: " << ec);
                              BOOST_CHECK_EQUAL(ec, std::error_code());
                          });

    topo.WaitForState(DeviceState::InitializingDevice);
    auto const currentState = topo.GetCurrentState();
    BOOST_CHECK_NO_THROW(AggregateState(currentState));
    BOOST_CHECK_EQUAL(StateEqualsTo(currentState, DeviceState::InitializingDevice), true);
}

BOOST_AUTO_TEST_CASE(async_change_state_timeout)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mIoContext.get_executor(), f.mDDSTopo, f.mDDSSession);
    topo.AsyncChangeState(TopologyTransition::InitDevice,
                          std::chrono::milliseconds(1),
                          [](std::error_code ec, TopologyState)
                          {
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
    Topology topo(f.mDDSTopo, f.mDDSSession);
    topo.AsyncChangeState(TopologyTransition::InitDevice,
                          [=](std::error_code ec, TopologyState state) mutable
                          {
                              BOOST_TEST_MESSAGE(ec);
                              TopologyStateByCollection cstate(GroupByCollectionId(state));
                              BOOST_TEST_MESSAGE("num collections: " << cstate.size());
                              BOOST_REQUIRE_EQUAL(cstate.size(), 1);
                              for (const auto& c : cstate)
                              {
                                  BOOST_TEST_MESSAGE("\t" << c.first);
                                  AggregatedState s;
                                  BOOST_REQUIRE_NO_THROW(s = AggregateState(c.second));
                                  BOOST_REQUIRE_EQUAL(
                                      s, static_cast<AggregatedState>(fair::mq::State::InitializingDevice));
                                  BOOST_TEST_MESSAGE("\tAggregated state: " << s);
                                  for (const auto& ds : c.second)
                                  {
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

    Topology topo(f.mDDSTopo, f.mDDSSession);
    full_device_lifecycle([&](TopologyTransition transition)
                          { BOOST_CHECK_EQUAL(topo.ChangeState(transition).first, std::error_code()); });
}

BOOST_AUTO_TEST_CASE(wait_for_state_full_device_lifecycle)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mDDSSession);
    topo.AsyncWaitForState(DeviceState::ResettingDevice, [](std::error_code ec) { BOOST_REQUIRE_EQUAL(ec, std::error_code()); });
    full_device_lifecycle([&](TopologyTransition transition) {
        topo.ChangeState(transition);
        BOOST_REQUIRE_EQUAL(topo.WaitForState(gExpectedState.at(transition)), std::error_code());
    });
}

BOOST_AUTO_TEST_CASE(change_state_full_device_lifecycle2)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mDDSSession);
    for (int i(0); i < 10; ++i)
    {
        for (auto transition : { TopologyTransition::InitDevice,
                                 TopologyTransition::CompleteInit,
                                 TopologyTransition::Bind,
                                 TopologyTransition::Connect,
                                 TopologyTransition::InitTask,
                                 TopologyTransition::Run })
        {
            BOOST_REQUIRE_EQUAL(topo.ChangeState(transition).first, std::error_code());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for (auto transition :
             { TopologyTransition::Stop, TopologyTransition::ResetTask, TopologyTransition::ResetDevice })
        {
            BOOST_REQUIRE_EQUAL(topo.ChangeState(transition).first, std::error_code());
        }
    }
}

BOOST_AUTO_TEST_CASE(set_properties)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mDDSSession);
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::InitDevice).first, std::error_code());

    auto const result1 = topo.SetProperties({ { "key1", "val1" } });
    BOOST_TEST_MESSAGE(result1.first);
    BOOST_REQUIRE_EQUAL(result1.first, std::error_code());
    BOOST_REQUIRE_EQUAL(result1.second.size(), 0);
    auto const result2 = topo.SetProperties({ { "key2", "val2" }, { "key3", "val3" } });
    BOOST_TEST_MESSAGE(result2.first);
    BOOST_REQUIRE_EQUAL(result2.first, std::error_code());
    BOOST_REQUIRE_EQUAL(result2.second.size(), 0);

    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::CompleteInit).first, std::error_code());
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::ResetDevice).first, std::error_code());
}

BOOST_AUTO_TEST_CASE(async_set_properties_concurrent)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mDDSSession);
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::InitDevice).first, std::error_code());

    SharedSemaphore blocker(2);
    topo.AsyncSetProperties({ { "key1", "val1" } },
                            [=](std::error_code ec, FailedDevices failed) mutable
                            {
                                BOOST_TEST_MESSAGE(ec);
                                BOOST_REQUIRE_EQUAL(ec, std::error_code());
                                BOOST_REQUIRE_EQUAL(failed.size(), 0);
                                blocker.Signal();
                            });
    topo.AsyncSetProperties({ { "key2", "val2" }, { "key3", "val3" } },
                            [=](std::error_code ec, FailedDevices failed) mutable
                            {
                                BOOST_TEST_MESSAGE(ec);
                                BOOST_REQUIRE_EQUAL(ec, std::error_code());
                                BOOST_REQUIRE_EQUAL(failed.size(), 0);
                                blocker.Signal();
                            });
    blocker.Wait();

    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::CompleteInit).first, std::error_code());
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::ResetDevice).first, std::error_code());
}

BOOST_AUTO_TEST_CASE(async_set_properties_timeout)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mDDSSession);
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::InitDevice).first, std::error_code());

    auto devices = topo.GetCurrentState();

    topo.AsyncSetProperties({ { "key1", "val1" } },
                            "",
                            std::chrono::microseconds(1),
                            [&](std::error_code ec, FailedDevices failed) mutable
                            {
                                BOOST_TEST_MESSAGE(ec);
                                BOOST_CHECK_EQUAL(ec, MakeErrorCode(ErrorCode::OperationTimeout));
                                BOOST_CHECK_EQUAL(failed.size(), devices.size());
                                for (const auto& device : devices)
                                {
                                    BOOST_CHECK_EQUAL(failed.count(device.taskId), 1);
                                }
                            });

    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::CompleteInit).first, std::error_code());
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::ResetDevice).first, std::error_code());
}

BOOST_AUTO_TEST_CASE(set_properties_mixed)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mDDSSession);
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::InitDevice).first, std::error_code());

    SharedSemaphore blocker;
    topo.AsyncSetProperties({ { "key1", "val1" } },
                            [=](std::error_code ec, FailedDevices failed) mutable
                            {
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

    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::CompleteInit).first, std::error_code());
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::ResetDevice).first, std::error_code());
}

BOOST_AUTO_TEST_CASE(get_properties)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mDDSSession);
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::InitDevice).first, std::error_code());

    auto const result = topo.GetProperties("^(session|id)$");
    BOOST_TEST_MESSAGE(result.first);
    BOOST_REQUIRE_EQUAL(result.first, std::error_code());
    BOOST_REQUIRE_EQUAL(result.second.failed.size(), 0);
    for (auto const& d : result.second.devices)
    {
        BOOST_TEST_MESSAGE(d.first);
        BOOST_REQUIRE_EQUAL(d.second.props.size(), 2);
        for (auto const& p : d.second.props)
        {
            BOOST_TEST_MESSAGE(" " << p.first << " : " << p.second);
        }
    }

    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::CompleteInit).first, std::error_code());
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::ResetDevice).first, std::error_code());
}

BOOST_AUTO_TEST_CASE(set_and_get_properties)
{
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    Topology topo(f.mDDSTopo, f.mDDSSession);
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::InitDevice).first, std::error_code());

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
    for (auto const& d : result2.second.devices)
    {
        BOOST_REQUIRE(d.second.props == props);
    }

    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::CompleteInit).first, std::error_code());
    BOOST_REQUIRE_EQUAL(topo.ChangeState(TopologyTransition::ResetDevice).first, std::error_code());
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

BOOST_AUTO_TEST_CASE(device_crashed)
{
    using namespace std::chrono_literals;
    BOOST_REQUIRE(framework::master_test_suite().argc >= 3);
    BOOST_REQUIRE_EQUAL(framework::master_test_suite().argv[1], "--topo-file");
    TopologyFixture f(framework::master_test_suite().argv[2]);

    {
        Topology topo(f.mDDSTopo, f.mDDSSession);
        BOOST_CHECK_EQUAL(topo.ChangeState(TopologyTransition::InitDevice).first, std::error_code());
        BOOST_CHECK_EQUAL(topo.ChangeState(TopologyTransition::CompleteInit).first, std::error_code());
        try
        {
            topo.SetProperties({ { "crash", "yes" } }, "", 10ms);
        }
        catch (std::system_error const& e)
        {
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
        Topology topo(f.mDDSTopo, f.mDDSSession);
        BOOST_CHECK_EQUAL(topo.ChangeState(TopologyTransition::InitDevice).first, std::error_code());
        BOOST_CHECK_EQUAL(topo.ChangeState(TopologyTransition::CompleteInit).first, std::error_code());
        f.mDDSSession->shutdown();
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
        Topology(ioContext.get_executor(), f[0].mDDSTopo, f[0].mDDSSession),
        Topology(ioContext.get_executor(), f[1].mDDSTopo, f[1].mDDSSession),
        Topology(ioContext.get_executor(), f[2].mDDSTopo, f[2].mDDSSession),
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
        Topology(f[0].mDDSTopo, f[0].mDDSSession),
        Topology(f[1].mDDSTopo, f[1].mDDSSession),
        Topology(f[2].mDDSTopo, f[2].mDDSSession),
    };

    boost::asio::io_context ioContext;
    // schedule transitions serial
    for (int i = 0; i < num; ++i)
    {
        full_device_lifecycle(
            [&](TopologyTransition transition)
            {
                ioContext.post(
                    [&f, &topos, i, transition]()
                    {
                        auto [ec, state] = topos[i].ChangeState(transition);
                        BOOST_REQUIRE_EQUAL(ec, std::error_code());
                        BOOST_TEST_MESSAGE(f[i].mDDSSession->getSessionID()
                                           << ": " << AggregateState(state) << " -> " << ec);
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
        Topology(f[0].mDDSTopo, f[0].mDDSSession),
        Topology(f[1].mDDSTopo, f[1].mDDSSession),
        Topology(f[2].mDDSTopo, f[2].mDDSSession),
    };

    boost::asio::io_context ioContext;
    // schedule transitions interleaved
    full_device_lifecycle(
        [&](TopologyTransition transition)
        {
            for (int i = 0; i < num; ++i)
            {
                ioContext.post(
                    [&f, &topos, i, transition]()
                    {
                        auto [ec, state] = topos[i].ChangeState(transition);
                        BOOST_REQUIRE_EQUAL(ec, std::error_code());
                        BOOST_TEST_MESSAGE(f[i].mDDSSession->getSessionID()
                                           << ": " << AggregateState(state) << " -> " << ec);
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

    Topology topo(f.mDDSTopo, f.mDDSSession);
    full_device_lifecycle([&](TopologyTransition transition)
                          { BOOST_REQUIRE_EQUAL(topo.ChangeState(transition).first, std::error_code()); });
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

int main(int argc, char* argv[]) {
    return boost::unit_test::unit_test_main(init_unit_test, argc, argv);
}
