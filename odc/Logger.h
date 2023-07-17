/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CORE_LOGGER
#define ODC_CORE_LOGGER

#include <odc/InfoLogger.h>
#include <odc/LoggerSeverity.h>
#include <odc/MiscUtils.h>

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
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>

#include <unistd.h> // getpid

#include <fstream>
#include <ostream>

// Main macro to be used for logging in ODC
// Example:
// OLOG(info) << "My message";
// OLOG(info, "TYrfjf", 54321) << "My message";
// clang-format off
#define OLOG_SEVERITY(severity)                               BOOST_LOG_CHANNEL_SEV(odc::core::Logger::instance().logger(), "", odc::core::ESeverity::severity)
#define OLOG_SEVERITY_COMMON(severity, common)                BOOST_LOG_CHANNEL_SEV(odc::core::Logger::instance().logger(), odc::core::toString(common.mPartitionID, ":", common.mRunNr), odc::core::ESeverity::severity)
#define OLOG_SEVERITY_PARTITION_RUN(severity, partition, run) BOOST_LOG_CHANNEL_SEV(odc::core::Logger::instance().logger(), odc::core::toString(partition, ":", run), odc::core::ESeverity::severity)
#define OLOG_GET_MACRO(arg1, arg2, arg3, NAME, ...) NAME
#define OLOG(...) OLOG_GET_MACRO(__VA_ARGS__, OLOG_SEVERITY_PARTITION_RUN, OLOG_SEVERITY_COMMON, OLOG_SEVERITY, UNUSED)(__VA_ARGS__)
// clang-format on

namespace odc::core {
class Logger
{
  public:
    struct Config
    {
        Config(ESeverity sev = ESeverity::info, ESeverity infologgerSev = ESeverity::info, const std::string& logDir = "", bool infologger = false)
            : mSeverity(sev)
            , mInfologgerSeverity(infologgerSev)
            , mLogDir(logDir)
            , mInfologger(infologger)
        {}

        ESeverity mSeverity{ ESeverity::info };
        ESeverity mInfologgerSeverity{ ESeverity::info };
        std::string mLogDir;
        bool mInfologger{ false };
        std::string mInfologgerSystem;
        std::string mInfologgerFacility;
        std::string mInfologgerRole;
    };

  public:
    using logger_t = boost::log::sources::severity_channel_logger_mt<ESeverity>;

    /// \brief Return singleton instance
    static Logger& instance()
    {
        static Logger instance;
        return instance;
    }

    logger_t& logger() { return mLogger; }

    /// \brief Initialization of log. Has to be called in main.
    void init(const Config& cfg = Config())
    {
        static bool started = false;
        if (started) {
            return;
        }
        started = true;

        // Logging to a file
        createFileSink(cfg);
        // Optional InfoLogger sink
        CInfoLogger::instance().setContext(cfg.mInfologgerFacility, cfg.mInfologgerSystem, cfg.mInfologgerRole);
        CInfoLogger::instance().registerSink(cfg.mInfologgerSeverity, cfg.mInfologger);

        boost::log::add_common_attributes();
        boost::log::core::get()->add_global_attribute("Process", boost::log::attributes::current_process_name());

        // OLOG(info) << "Log engine is initialized with severety \"" << cfg.mSeverity << "\"";
    }

  private:
    void createFileSink(const Config& cfg) const
    {
        // Log directory is empty, don't create file sink
        if (cfg.mLogDir.empty())
            return;

        using namespace boost::log;
        using fileSink_t = boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>>;

        std::string logDir{ smart_path(cfg.mLogDir) };
        if (!boost::filesystem::exists(boost::filesystem::path(logDir)) && !boost::filesystem::create_directories(boost::filesystem::path(logDir))) {
            throw std::runtime_error(toString("Can't initialize file sink of logger: failed to create directory ", std::quoted(logDir)));
        }

        boost::filesystem::path logFile{ logDir };
        logFile /= "odc_%Y-%m-%d.%N.log";

        pid_t pid = getpid();

        // Default format for logger
        formatter formatter = expressions::stream << std::left << expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                                                  << " " << std::setw(4)
                                                  << expressions::attr<ESeverity>("Severity")
                                                  << " " << std::setw(20)
                                                  << expressions::attr<std::string>("Process")
                                                  << pid
                                               // << ":"
                                               // << expressions::attr<attributes::current_thread_id::value_type>("ThreadID") << "> " << std::setw(20)
                                                  << " " << std::setw(20)
                                                  << expressions::attr<std::string>("Channel") << " " << expressions::smessage;

        fileSink_t fileSink = add_file_log(keywords::file_name = logFile,
                                           keywords::open_mode = (std::ios::out | std::ios::app),
                                           keywords::rotation_size = 100 * 1024 * 1024,
                                           // rotate at midnight every day
                                           keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
                                           // log collector,
                                           // -- maximum total size of the stored log files is 1GB.
                                           // -- minimum free space on the drive is 2GB
                                           keywords::max_size = 1000 * 1024 * 1024,
                                           keywords::min_free_space = 2000 * 1024 * 1024,
                                           keywords::auto_flush = true);

        fileSink->set_formatter(formatter);
        fileSink->set_filter(logger::severity >= cfg.mSeverity);
    }

  private:
    logger_t mLogger; ///> Main logger object
};
} // namespace odc::core

#endif // ODC_CORE_LOGGER
