// Copyright 2019 GSI, Inc. All rights reserved.
//
//
#ifndef __ODC__INFOLOGGER__
#define __ODC__INFOLOGGER__

// ODC
#include "LoggerSeverity.h"

#ifdef INFOLOGGER_AVAIL

// InfoLogger
#include <InfoLogger/InfoLogger.hxx>
#include <InfoLogger/InfoLoggerMacros.hxx>
// BOOST
#include <boost/log/sinks/basic_sink_backend.hpp>

namespace odc::core
{
    class CInfoLogger
    {

      private:
        class CBackend : public boost::log::sinks::basic_sink_backend<
                             boost::log::sinks::combine_requirements<boost::log::sinks::synchronized_feeding>::type>
        {

          public:
            // The function consumes the log records that come from the frontend
            void consume(boost::log::record_view const& rec)
            {
                ESeverity sev{ *rec[odc::core::severity] };
                std::string channel{ *rec[boost::log::expressions::attr<std::string>("Channel")] };
                std::string msg{ *rec[boost::log::expressions::smessage] };
                CInfoLogger::instance().log(sev, channel, msg);
            }
        };

        using CSink = boost::log::sinks::synchronous_sink<CBackend>;

      public:
        /// \brief Return singleton instance
        static CInfoLogger& instance()
        {
            static CInfoLogger instance;
            return instance;
        }

        void registerSink(ESeverity _severity, bool _infologger) const
        {
            if (!_infologger)
                return;
            auto sink{ boost::make_shared<CSink>() };
            sink->set_filter(severity >= _severity);
            boost::log::core::get()->add_sink(sink);
        }

      private:
        CInfoLogger()
        {
            using namespace AliceO2::InfoLogger;
            InfoLoggerContext context;
            context.setField(InfoLoggerContext::FieldName::Facility, "ODC");
            // FIXME: Set the real run number as soon as it's available from AliECS
            // context.setField(InfoLoggerContext::FieldName::PartitionID, "0");
            // context.setField(InfoLoggerContext::FieldName::Run, "0");
            m_log.setContext(context);
        }

        void log(ESeverity _severity, const std::string& /*_channel*/, const std::string& _msg)
        {
            m_log.log(convertSeverity(_severity), "%s", _msg.c_str());
        }

        AliceO2::InfoLogger::InfoLogger::Severity convertSeverity(ESeverity _severity)
        {
            using namespace AliceO2::InfoLogger;
            switch (_severity)
            {
                case ESeverity::debug:
                    return InfoLogger::Severity::Debug;
                case ESeverity::info:
                    return InfoLogger::Severity::Info;
                case ESeverity::warning:
                    return InfoLogger::Severity::Warning;
                case ESeverity::error:
                    return InfoLogger::Severity::Error;
                case ESeverity::fatal:
                    return InfoLogger::Severity::Fatal;
                default:
                    return InfoLogger::Severity::Undefined;
            }
        }

        AliceO2::InfoLogger::InfoLogger m_log;
    };

}; // namespace odc::core

#else

// Empty implementation of CInfoLogger if InfoLogger is not available
namespace odc::core
{
    class CInfoLogger
    {
      public:
        /// \brief Return singleton instance
        static CInfoLogger& instance()
        {
            static CInfoLogger instance;
            return instance;
        }

        void registerSink(ESeverity /*_severity*/, bool /*_infologger*/) const
        {
        }
    };
}; // namespace odc::core

#endif // INFOLOGGER_AVAIL

#endif // __ODC__INFOLOGGER__
