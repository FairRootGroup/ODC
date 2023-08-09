/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__LOGGERSEVERITY__
#define __ODC__LOGGERSEVERITY__

//// BOOST
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/log/expressions.hpp>

namespace odc::core {
/// Log Severity levels
enum class ESeverity
{
    trace,
    debug,
    info,
    warning,
    error,
    fatal
};

/// Array of log severity names
static constexpr std::array<const char*, 8> gSeverityNames{ { "trc", "dbg", "inf", "wrn", "err", "fat" } };

namespace logger {
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", ESeverity)
BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)
} // namespace logger

// A custom streamer to convert string to odc::core::ESeverity
inline std::istream& operator>>(std::istream& in, ESeverity& severity)
{
    std::string token;
    in >> token;
    boost::algorithm::to_lower(token);

    auto found = std::find(gSeverityNames.begin(), gSeverityNames.end(), token);
    if (found == gSeverityNames.end()) {
        throw std::runtime_error(std::string("Can't convert string ") + token + " to ODC log severity");
    }

    severity = static_cast<ESeverity>(std::distance(gSeverityNames.begin(), found));
    return in;
}
// A custom streamer to convert odc::core::ESeverity to string
inline std::ostream& operator<<(std::ostream& out, ESeverity severity)
{
    const size_t idx{ static_cast<size_t>(severity) };
    out << gSeverityNames.at(idx);
    return out;
}
} // namespace odc::core

#endif // __ODC__LOGGERSEVERITY__
