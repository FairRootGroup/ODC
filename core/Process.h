// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__Process__
#define __ODC__Process__

// STD

// BOOST
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/process.hpp>

namespace bp = boost::process;
namespace bio = boost::asio;

namespace odc::core
{

    //
    // This function was taken from DDS
    //

    // TODO: Document me!
    // If _Timeout is 0, then function returns child pid and doesn't wait for the child process to finish. Otherwise
    // return value is 0.
    inline pid_t execute(const std::string& _Command,
                         const std::chrono::seconds& _Timeout,
                         std::string* _output = nullptr,
                         std::string* _errout = nullptr,
                         int* _exitCode = nullptr)
    {
        try
        {
            std::string smartCmd(_Command);
            // MiscCommon::smart_path(&smartCmd);

            if (std::chrono::seconds(0) == _Timeout)
            {
                boost::process::child c(smartCmd);
                pid_t pid = c.id();
                c.detach();
                return pid;
            }

            bio::io_service ios;
            bp::async_pipe outPipe(ios);
            bp::async_pipe errPipe(ios);
            bio::streambuf outBuf;
            bio::streambuf errBuf;

            bio::deadline_timer watchdog{ ios, boost::posix_time::seconds(_Timeout.count()) };

            bp::child c(smartCmd,
                        bp::std_in.close(),
                        bp::std_out > outPipe,
                        bp::std_err > errPipe,
                        ios,
                        bp::on_exit(
                            [&](int /*exit*/, const std::error_code& /*ec*/)
                            {
                                outPipe.close();
                                errPipe.close();
                                watchdog.cancel();
                                ios.stop();
                            }));

            if (!c.valid())
                throw std::runtime_error("Can't execute the given process.");

            bio::async_read(outPipe, outBuf, [](const boost::system::error_code& /*ec*/, std::size_t /*size*/) {});
            bio::async_read(errPipe, errBuf, [](const boost::system::error_code& /*ec*/, std::size_t /*size*/) {});

            bool errorFlag(false);
            watchdog.async_wait(
                [&](boost::system::error_code ec)
                {
                    // Workaround we can't use boost::process::child::wait_for because it's buged in Boost 1.70 and not
                    // yet fixed. We therefore use a deadline timer.
                    if (!ec)
                    {
                        errorFlag = true;
                        outPipe.close();
                        errPipe.close();
                        ios.stop();
                        // Child didn't yet finish. Terminating it...
                        c.terminate();
                    }
                });

            ios.run();
            // prevent leaving a zombie process
            c.wait();

            if (errorFlag)
                throw std::runtime_error("Timeout has been reached, command execution will be terminated.");

            if (_exitCode)
                *_exitCode = c.exit_code();

            if (_output)
                _output->assign(std::istreambuf_iterator<char>(&outBuf), std::istreambuf_iterator<char>());

            if (_errout)
                _errout->assign(std::istreambuf_iterator<char>(&errBuf), std::istreambuf_iterator<char>());

            return 0;
        }
        catch (std::exception& _e)
        {
            std::stringstream ss;
            ss << "execute: " << _e.what();
            throw std::runtime_error(ss.str());
        }
    }

} // namespace odc::core

#endif /* defined(__ODC__Process__) */
