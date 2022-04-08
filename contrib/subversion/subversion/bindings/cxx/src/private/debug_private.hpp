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

#ifndef SVNXX_PRIVATE_DEBUG_HPP
#define SVNXX_PRIVATE_DEBUG_HPP

// We can only write pool debug logs with SVN_DEBUG enabled.
#if defined(SVNXX_POOL_DEBUG) && !defined(SVN_DEBUG)
#  undef SVNXX_POOL_DEBUG
#endif

#ifdef SVNXX_POOL_DEBUG
#include "private/svn_debug.h"
#endif

namespace apache {
namespace subversion {
namespace svnxx {
namespace impl {

extern const char root_pool_tag[];
extern const char root_pool_key[];

} // namespace impl
} // namespace svnxx
} // namespace subversion
} // namespace apache

#endif // SVNXX_PRIVATE_DEBUG_HPP
