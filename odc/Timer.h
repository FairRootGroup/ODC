/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CORE_TIMER
#define ODC_CORE_TIMER

#include <chrono>

namespace odc::core
{

template<typename TimeT = std::chrono::milliseconds>
struct Timer
{
    Timer()
        : mStart(std::chrono::system_clock::now())
    {}

    typename TimeT::rep duration() const { return std::chrono::duration_cast<TimeT>(std::chrono::system_clock::now() - mStart).count(); }

  private:
    std::chrono::system_clock::time_point mStart;
};

} // namespace odc::core
#endif /*ODC_CORE_TIMER*/
