/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__Semaphore__
#define __ODC__Semaphore__

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

namespace odc::core {

/**
 * @brief A simple blocking semaphore.
 */
struct Semaphore
{
    Semaphore()
        : Semaphore(0)
    {}
    explicit Semaphore(std::size_t initial_count)
        : fCount(initial_count)
    {}

    void Wait()
    {
        std::unique_lock<std::mutex> lk(fMutex);
        if (fCount > 0) {
            --fCount;
        } else {
            fCv.wait(lk, [this] { return fCount > 0; });
            --fCount;
        }
    }
    void Signal()
    {
        std::unique_lock<std::mutex> lk(fMutex);
        ++fCount;
        lk.unlock();
        fCv.notify_one();
    }
    std::size_t GetCount() const
    {
        std::unique_lock<std::mutex> lk(fMutex);
        return fCount;
    }

  private:
    std::size_t fCount;
    mutable std::mutex fMutex;
    std::condition_variable fCv;
};

/**
 * @brief A simple copyable blocking semaphore.
 */
struct SharedSemaphore
{
    SharedSemaphore()
        : fSemaphore(std::make_shared<Semaphore>())
    {}
    explicit SharedSemaphore(std::size_t initial_count)
        : fSemaphore(std::make_shared<Semaphore>(initial_count))
    {}

    void Wait() { fSemaphore->Wait(); }
    void Signal() { fSemaphore->Signal(); }
    std::size_t GetCount() const { return fSemaphore->GetCount(); }

  private:
    std::shared_ptr<Semaphore> fSemaphore;
};

} // namespace odc::core

#endif /* __ODC__Semaphore__ */
