// Copyright 2019 GSI, Inc. All rights reserved.
//
//
#ifndef __ODC__LOGGER__
#define __ODC__LOGGER__

// BOOST
#ifndef BOOST_LOG_DYN_LINK
#define BOOST_LOG_DYN_LINK
#endif
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/attributes/current_process_id.hpp>
#include <boost/log/attributes/current_process_name.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
// STD
#include <fstream>
#include <ostream>
// ODC
#include "InfoLogger.h"
#include "LoggerSeverity.h"

// Main macro to be used for logging in ODC
// Example: LOG(info) << "My message";
// TODO: FIXME: OLOG has to be changed to LOG. For the moment this is a workaround, because FairMQ SDK uses FairLogger
// which also defines LOG macro.
#define OLOG(severity) BOOST_LOG_SEV(odc::core::CLogger::instance().logger(), severity)

namespace odc::core
{
    class CLogger
    {
      public:
        struct SConfig
        {
            SConfig(ESeverity _severity = ESeverity::info, const std::string& _logDir = "", bool _infologger = false)
                : m_severity(_severity)
                , m_logDir(_logDir)
                , m_infologger(_infologger)
            {
            }

            ESeverity m_severity{ ESeverity::info };
            std::string m_logDir;
            bool m_infologger{ false };
        };

      public:
        using logger_t = boost::log::sources::severity_logger_mt<ESeverity>;

        /// \brief Return singleton instance
        static CLogger& instance()
        {
            static CLogger instance;
            return instance;
        }

        logger_t& logger()
        {
            return m_logger;
        }

        /// \brief Initialization of log. Has to be called in main.
        void init(const SConfig& _config = SConfig())
        {
            static bool started = false;
            if (started)
                return;
            started = true;

            // Logging to a file
            createFileSink(_config);
            // Logging to console
            createConsoleSink(_config);
            // Optional InfoLogger sink
            CInfoLogger::instance().registerSink(_config.m_severity, _config.m_infologger);

            boost::log::add_common_attributes();
            boost::log::core::get()->add_global_attribute("Process", boost::log::attributes::current_process_name());

            OLOG(ESeverity::info) << "Log engine is initialized with severety \"" << _config.m_severity << "\"";
        }

      private:
        void createConsoleSink(const SConfig& /*_config*/) const
        {
            using namespace boost::log;
            using ostreamSink_t = boost::shared_ptr<sinks::synchronous_sink<sinks::text_ostream_backend>>;
            ostreamSink_t stdoutSink = add_console_log(std::cout, keywords::format = "%Process%: %Message%");
            ostreamSink_t stdoutCleanSink = add_console_log(std::cout, keywords::format = "%Message%");
            ostreamSink_t stderrSink = add_console_log(std::cerr, keywords::format = "%Process%: error: %Message%");
            stdoutSink->set_filter(severity == ESeverity::stdout);
            stdoutCleanSink->set_filter(severity == ESeverity::clean);
            stderrSink->set_filter(severity == ESeverity::stderr);
        }

        void createFileSink(const SConfig& _config) const
        {
            // Log directory is empty, don't create file sink
            if (_config.m_logDir.empty())
                return;

            using namespace boost::log;
            using fileSink_t =
                boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>>;

            // If log directory doesn't exist throw an exception
            if (!boost::filesystem::exists(_config.m_logDir))
            {
                throw std::runtime_error("Can't initialize file sink of logger. Directory \"" + _config.m_logDir +
                                         "\" doesn't exist");
            }

            boost::filesystem::path logFile{ _config.m_logDir };
            logFile /= "odc_%Y-%m-%d.%N.log";

            // Default format for logger
            formatter formatter =
                // TODO: std::setw doesn't work for the first collumn of the log (TimeStamp). Investigate!
                expressions::stream << std::left
                                    << expressions::format_date_time<boost::posix_time::ptime>("TimeStamp",
                                                                                               "%Y-%m-%d %H:%M:%S.%f")
                                    << "   " << std::setw(7) << expressions::attr<ESeverity>("Severity")
                                    << std::setw(20) << expressions::attr<std::string>("Process") << " <"
                                    << expressions::attr<attributes::current_process_id::value_type>("ProcessID") << ":"
                                    << expressions::attr<attributes::current_thread_id::value_type>("ThreadID")
                                    << ">    " << expressions::smessage;

            fileSink_t fileSink =
                add_file_log(keywords::file_name = logFile,
                             keywords::open_mode = (std::ios::out | std::ios::app),
                             keywords::rotation_size = 10 * 1024 * 1024,
                             // rotate at midnight every day
                             keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
                             // log collector,
                             // -- maximum total size of the stored log files is 1GB.
                             // -- minimum free space on the drive is 2GB
                             keywords::max_size = 1000 * 1024 * 1024,
                             keywords::min_free_space = 2000 * 1024 * 1024,
                             keywords::auto_flush = true);

            fileSink->set_formatter(formatter);
            fileSink->set_filter(severity >= _config.m_severity);
        }

      private:
        logger_t m_logger; ///> Main logger object
    };
}; // namespace odc::core

#endif // __ODC__LOGGER__
