/********************************************************************************
 * Copyright (C) 2019-2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__AsioBase__
#define __ODC__AsioBase__

#include "Traits.h"

#include <boost/asio/any_io_executor.hpp>
#include <memory>
#include <utility>

namespace odc::core
{

    using DefaultExecutor = boost::asio::any_io_executor;
    using DefaultAllocator = std::allocator<int>;

    /**
     * @class AsioBase AsioBase.h <fairmq/sdk/AsioBase.h>
     * @tparam Executor Associated I/O executor
     * @tparam Allocator Associated default allocator
     * @brief Base for creating Asio-enabled I/O objects
     *
     * @par Thread Safety
     * @e Distinct @e objects: Safe.@n
     * @e Shared @e objects: Unsafe.
     */
    template <typename Executor, typename Allocator>
    class AsioBase
    {
      public:
        /// Member type of associated I/O executor
        using ExecutorType = Executor;
        /// Get associated I/O executor
        auto GetExecutor() const noexcept -> ExecutorType
        {
            return fExecutor;
        }

        /// Member type of associated default allocator
        using AllocatorType = Allocator;
        /// Get associated default allocator
        auto GetAllocator() const noexcept -> AllocatorType
        {
            return fAllocator;
        }

        /// NO default ctor
        AsioBase() = delete;

        /// Construct with associated I/O executor
        explicit AsioBase(Executor ex, Allocator alloc)
            : fExecutor(std::move(ex))
            , fAllocator(std::move(alloc))
        {
        }

        /// NOT copyable
        AsioBase(const AsioBase&) = delete;
        AsioBase& operator=(const AsioBase&) = delete;

        /// movable
        AsioBase(AsioBase&&) noexcept = default;
        AsioBase& operator=(AsioBase&&) noexcept = default;

        ~AsioBase() = default;

      private:
        ExecutorType fExecutor;
        AllocatorType fAllocator;
    };

} // namespace odc::core

#endif /* __ODC__AsioBase__ */
