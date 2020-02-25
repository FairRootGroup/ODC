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

// Main macro to be used for logging in ODC
// Example: LOG(info) << "My message";
#define LOG(severity) BOOST_LOG_SEV(odc::core::CLogger::instance().logger(), severity)

namespace odc
{
    namespace core
    {
        /// Log Severity levels
        enum class ESeverity
        {
            debug,
            info,
            warning,
            error,
            fatal,
            log_stdout,
            log_stdout_clean, // nothing will be pre-append to the output
            log_stderr
        };

        /// Array of log severity names
        static constexpr std::array<const char*, 8> gSeverityNames{
            { "dbg", "inf", "wrn", "err", "fat", "cout", "cout", "cerr" }
        };

        BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", ESeverity)

        class CLogger
        {
          public:
            struct SConfig
            {
                SConfig(ESeverity _severity = ESeverity::info, const std::string& _logDir = "")
                    : m_severity(_severity)
                    , m_logDir(_logDir)
                {
                }

                ESeverity m_severity{ ESeverity::info };
                std::string m_logDir;
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

                boost::log::add_common_attributes();
                boost::log::core::get()->add_global_attribute("Process",
                                                              boost::log::attributes::current_process_name());

                LOG(ESeverity::info) << "Log engine is initialized with severety \"" << _config.m_severity << "\"";
            }

          private:
            void createConsoleSink(const SConfig& _config) const
            {
                using namespace boost::log;
                using ostreamSink_t = boost::shared_ptr<sinks::synchronous_sink<sinks::text_ostream_backend>>;
                ostreamSink_t stdoutSink = add_console_log(std::cout, keywords::format = "%Process%: %Message%");
                ostreamSink_t stdoutCleanSink = add_console_log(std::cout, keywords::format = "%Message%");
                ostreamSink_t stderrSink = add_console_log(std::cerr, keywords::format = "%Process%: error: %Message%");
                stdoutSink->set_filter(severity == ESeverity::log_stdout);
                stdoutCleanSink->set_filter(severity == ESeverity::log_stdout_clean);
                stderrSink->set_filter(severity == ESeverity::log_stderr);
            }

            void createFileSink(const SConfig& _config) const
            {
                using namespace boost::log;
                using fileSink_t =
                    boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>>;

                // Check that log directory is provided and exists
                if (_config.m_logDir.empty() || !boost::filesystem::exists(_config.m_logDir))
                {
                    // TODO: FIXME: throw exception. If directory is empty fallback to std::cout, std::cerr
                    return;
                }

                boost::filesystem::path logFile{ _config.m_logDir };
                logFile /= "odc_%Y-%m-%d.%N.log";

                // Default format for logger
                formatter formatter =
                    // TODO: std::setw doesn't work for the first collumn of the log (TimeStamp). Investigate!
                    expressions::stream << std::left
                                        << expressions::format_date_time<boost::posix_time::ptime>(
                                               "TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                                        << "   " << std::setw(7) << expressions::attr<ESeverity>("Severity")
                                        << std::setw(20) << expressions::attr<std::string>("Process") << " <"
                                        << expressions::attr<attributes::current_process_id::value_type>("ProcessID")
                                        << ":"
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

        // A custom streamer to convert string to odc::core::ESeverity
        inline std::istream& operator>>(std::istream& _in, ESeverity& _severity)
        {
            std::string token;
            _in >> token;
            boost::algorithm::to_lower(token);

            auto found = std::find(gSeverityNames.begin(), gSeverityNames.end(), token);
            if (found == gSeverityNames.end())
                throw std::runtime_error(std::string("Can't convert string ") + token + " to ODC log severity");

            _severity = static_cast<ESeverity>(std::distance(gSeverityNames.begin(), found));
            return _in;
        }
        // A custom streamer to convert odc::core::ESeverity to string
        inline std::ostream& operator<<(std::ostream& _out, ESeverity _severity)
        {
            const size_t idx{ static_cast<size_t>(_severity) };
            _out << gSeverityNames.at(idx);
            return _out;
        }
    }; // namespace core
};     // namespace odc

#endif // __ODC__LOGGER__
