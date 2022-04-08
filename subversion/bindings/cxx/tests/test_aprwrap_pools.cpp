/*
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
 */

#include <boost/test/unit_test.hpp>

#include <cstdlib>

#include "svnxx/exception.hpp"
#include "../src/aprwrap.hpp"

#include "fixture_init.hpp"

namespace svn = ::apache::subversion::svnxx;

BOOST_AUTO_TEST_SUITE(aprwrap_pools,
                      * boost::unit_test::fixture<init>());

BOOST_AUTO_TEST_CASE(initialize_global_pool)
{
  apr::pool pool;
  BOOST_TEST(pool.get() != nullptr);
  BOOST_TEST(apr_pool_parent_get(pool.get()) != nullptr);
}

BOOST_AUTO_TEST_CASE(create_subpool)
{
  apr::pool pool;
  apr::pool subpool(&pool);
  BOOST_TEST(pool.get() == apr_pool_parent_get(subpool.get()));
}

BOOST_AUTO_TEST_CASE(typed_allocate)
{
  apr::pool pool;
  const unsigned char* buffer = pool.alloc<unsigned char>(1);
  BOOST_TEST(buffer != nullptr);
}

// N.B.: This test may pass randomly even if zero-filled allocation
// does not work correctly, since we cannot make assumptions about the
// values of uninitialized memory.
BOOST_AUTO_TEST_CASE(typed_allocate_zerofill)
{
  apr::pool pool;
  static const std::size_t size = 32757;
  const unsigned char* buffer = pool.allocz<unsigned char>(size);
  BOOST_TEST_REQUIRE(buffer != nullptr);
  BOOST_TEST(std::count(buffer, buffer + size, 0) == size);
}

BOOST_AUTO_TEST_CASE(overflow)
{
  apr::pool pool;
  BOOST_CHECK_THROW(pool.alloc<long>(~std::size_t(0)), svn::allocation_failed);
  BOOST_CHECK_THROW(pool.allocz<int>(~std::size_t(0)), svn::allocation_failed);
}

BOOST_AUTO_TEST_SUITE_END();
