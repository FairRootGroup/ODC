/********************************************************************************
 * Copyright (C) 2019-2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__Traits__
#define __ODC__Traits__

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <type_traits>

namespace boost::asio::detail
{

    /// Specialize to match our coding conventions
    template <typename T, typename Executor>
    struct associated_executor_impl<T, Executor, std::enable_if_t<is_executor<typename T::ExecutorType>::value>>
    {
        using type = typename T::ExecutorType;

        static auto get(const T& obj, const Executor& /*ex = Executor()*/) noexcept -> type
        {
            return obj.GetExecutor();
        }
    };

    /// Specialize to match our coding conventions
    template <typename T, typename Allocator>
    struct associated_allocator_impl<T, Allocator, std::enable_if_t<T::AllocatorType>>
    {
        using type = typename T::AllocatorType;

        static auto get(const T& obj, const Allocator& /*alloc = Allocator()*/) noexcept -> type
        {
            return obj.GetAllocator();
        }
    };

} /* namespace boost::asio::detail */

#endif /* __ODC__Traits__ */
