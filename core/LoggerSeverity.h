// Copyright 2019 GSI, Inc. All rights reserved.
//
//
#ifndef __ODC__LOGGERSEVERITY__
#define __ODC__LOGGERSEVERITY__

//// BOOST
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/log/expressions.hpp>

namespace odc::core
{
    /// Log Severity levels
    enum class ESeverity
    {
        debug,
        info,
        warning,
        error,
        fatal,
        stdout,
        clean, // nothing will be prepend to the output
        stderr
    };

    /// Array of log severity names
    static constexpr std::array<const char*, 8> gSeverityNames{
        { "dbg", "inf", "wrn", "err", "fat", "cout", "cout", "cerr" }
    };

    namespace logger {
        BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", ESeverity)
        BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)
    }

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
} // namespace odc::core

#endif // __ODC__LOGGERSEVERITY__
