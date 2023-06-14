/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
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

struct Timer
{
    Timer()
        : mStart(std::chrono::steady_clock::now())
    {}

    int64_t duration() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - mStart).count();
    }

  private:
    std::chrono::steady_clock::time_point mStart;
};

} // namespace odc::core

#endif /*ODC_CORE_TIMER*/
