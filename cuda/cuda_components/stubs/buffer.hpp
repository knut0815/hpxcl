// Copyright (c)		2013 Damond Howard
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#if !defined(BUFFER_3_HPP)
#define BUFFER_3_HPP

#include <hpx/runtime/components/stubs/stub_base.hpp>
#include <hpx/runtime/applier/apply.hpp>
#include <hpx/include/async.hpp>

#include "../sever/kernel.hpp"

namespace hpx
{
    namespace cuda
    {
        namespace stubs
        {
            struct buffer
                : hpx::components::stub_base<server::kernel>
            {
            	private:
                //stubs member functions
            	public:
            };
        }
    }
}
#endif //BUFFER_3_HPP