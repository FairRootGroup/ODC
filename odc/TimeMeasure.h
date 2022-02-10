/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

//
// This file contains a number of helpers to calculate execution time of a function.
//
#ifndef __ODC__STimeMeasure__
#define __ODC__STimeMeasure__

#include <chrono>

namespace odc::core
{
    template <typename TimeT = std::chrono::milliseconds>
    struct STimeMeasure
    {
        STimeMeasure()
            : m_start(std::chrono::system_clock::now()){};

        typename TimeT::rep duration() const
        {
            return std::chrono::duration_cast<TimeT>(std::chrono::system_clock::now() - m_start).count();
        }

      private:
        std::chrono::system_clock::time_point m_start;
    };
} // namespace odc::core
#endif /*__ODC__STimeMeasure__*/
