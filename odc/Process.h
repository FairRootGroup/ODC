/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CORE_PROCESS
#define ODC_CORE_PROCESS

#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/process.hpp>

#include <chrono>
#include <iterator>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>
#include <utility> // pair

namespace bp = boost::process;
namespace ba = boost::asio;

struct ExecuteResult
{
    std::string stdOut;
    std::string errOut;
    int exitCode;
};

namespace odc::core
{

inline pid_t execute(const std::string& cmd,
                     const std::chrono::seconds& timeout,
                     std::string* stdOut = nullptr,
                     std::string* errOut = nullptr,
                     int* exitCode = nullptr,
                     const std::vector<std::pair<std::string, std::string>>& extraEnv = {})
{
    try {
        ba::io_service ios;
        bp::async_pipe outPipe(ios);
        bp::async_pipe errPipe(ios);
        ba::streambuf outBuf;
        ba::streambuf errBuf;

        ba::deadline_timer watchdog{ ios, boost::posix_time::seconds(timeout.count()) };

        auto env = boost::this_process::environment();
        for (const auto& e : extraEnv) {
            env[e.first] = e.second;
        }

        bp::child c("/bin/bash",
                    "-c",
                    cmd,
                    env,
                    bp::std_in.close(),
                    bp::std_out > outPipe,
                    bp::std_err > errPipe,
                    ios,
                    bp::on_exit([&](int /*exit*/, const std::error_code& /*ec*/) {
                        watchdog.cancel();
                        outPipe.async_close();
                        errPipe.async_close();
                    })
        );

        if (!c.valid()) {
            throw std::runtime_error("Can't execute the given process.");
        }

        ba::async_read(outPipe, outBuf, [](const boost::system::error_code& /*ec*/, std::size_t /*size*/) {});
        ba::async_read(errPipe, errBuf, [](const boost::system::error_code& /*ec*/, std::size_t /*size*/) {});

        bool errorFlag = false;
        watchdog.async_wait([&](boost::system::error_code ec) {
            // Workaround we can't use boost::process::child::wait_for because it's buged in Boost 1.70 and not
            // yet fixed. We therefore use a deadline timer.
            if (!ec) {
                errorFlag = true;
                outPipe.close();
                errPipe.close();
                ios.stop();
                c.terminate();
            }
        });

        ios.run();
        c.wait();

        if (exitCode) {
            *exitCode = c.exit_code();
        }

        if (stdOut) {
            stdOut->assign(std::istreambuf_iterator<char>(&outBuf), std::istreambuf_iterator<char>());
        }

        if (errOut) {
            errOut->assign(std::istreambuf_iterator<char>(&errBuf), std::istreambuf_iterator<char>());
        }

        if (errorFlag) {
            throw std::runtime_error("Timeout has been reached, command execution will be terminated.");
        }

        return 0;
    } catch (std::exception& e) {
        std::stringstream ss;
        ss << "execute: " << e.what();
        throw std::runtime_error(ss.str());
    }
}

} // namespace odc::core

#endif /* defined(ODC_CORE_PROCESS) */
