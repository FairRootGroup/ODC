/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__INFOLOGGER__
#define __ODC__INFOLOGGER__

// ODC
#include <odc/LoggerSeverity.h>

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
                ESeverity sev{ *rec[odc::core::logger::severity] };
                std::string channel{ *rec[odc::core::logger::channel] };
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
            sink->set_filter(logger::severity >= _severity);
            boost::log::core::get()->add_sink(sink);
        }

      private:
        CInfoLogger()
        {
            m_context.setField(AliceO2::InfoLogger::InfoLoggerContext::FieldName::Facility, std::string("ODC"));
            m_context.setField(AliceO2::InfoLogger::InfoLoggerContext::FieldName::System, std::string("ODC"));
            m_log.setContext(m_context);
        }

        void log(ESeverity _severity, const std::string& _channel, const std::string& _msg)
        {
            using namespace AliceO2::InfoLogger;
            InfoLogger::InfoLoggerMessageOption options{ convertSeverity(_severity), 0, -1, __FILE__, __LINE__ };

            // Channel string contains "partition:run"
            std::string partition;
            std::string run;
            const auto idx{ _channel.find_first_of(':') };
            if (std::string::npos != idx)
            {
                partition = _channel.substr(0, idx);
                run = _channel.substr(idx + 1);
            }
            InfoLoggerContext context(
                m_context,
                { { InfoLoggerContext::FieldName::Partition, partition }, { InfoLoggerContext::FieldName::Run, run } });
            m_log.log(options, context, "%s", _msg.c_str());
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

        AliceO2::InfoLogger::InfoLoggerContext m_context;
        AliceO2::InfoLogger::InfoLogger m_log;
    };
} // namespace odc::core

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
} // namespace odc::core

#endif // INFOLOGGER_AVAIL

#endif // __ODC__INFOLOGGER__
