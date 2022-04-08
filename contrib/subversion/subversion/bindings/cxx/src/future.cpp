/**
 * @copyright
 * ====================================================================
 *    Licensed to the Apache Software Foundation (ASF) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The ASF licenses this file
 *    to you under the Apache License, Version 2.0 (the
 *    "License"); you may not use this file except in compliance
 *    with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing,
 *    software distributed under the License is distributed on an
 *    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *    KIND, either express or implied.  See the License for the
 *    specific language governing permissions and limitations
 *    under the License.
 * ====================================================================
 * @endcopyright
 */

#include "private/future_private.hpp"

namespace apache {
namespace subversion {
namespace svnxx {
namespace detail {
namespace future_ {

future_base::future_base() noexcept {}
future_base::~future_base() noexcept {}

future_base::future_base(future_base&& that) noexcept
  : future_base(std::move(that.unique_result))
{}

future_base::future_base(unique_ptr&& unique_result_) noexcept
  : unique_result(std::move(unique_result_))
{}

shared_ptr future_base::share() noexcept
{
  return shared_ptr(unique_result.release());
}

} // namespace future_
} // namespace detail
} // namespace svnxx
} // namespace subversion
} // namespace apache
