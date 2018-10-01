/*
 * Copyright (c) 2002-2018 "Neo4j,"
 * Neo4j Sweden AB [http://neo4j.com]
 *
 * This file is part of Neo4j.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "integration.hpp"
#include "catch.hpp"

SCENARIO("Test using a pooled connection", "[integration][ipv6][secure][pooling]")
{
    GIVEN("a new connection pool") {
        const auto auth_token = BoltAuth_basic(BOLT_USER, BOLT_PASSWORD, NULL);
        struct BoltTrust trust{nullptr, 0, 1, 1};
        struct BoltConfig config{BOLT_DIRECT, BOLT_SECURE_SOCKET, &trust, BOLT_USER_AGENT, nullptr, nullptr, nullptr,
                                 10};
        struct BoltConnector* connector = BoltConnector_create(&BOLT_IPV6_ADDRESS, auth_token, &config);
        WHEN("a connection is acquired") {
            struct BoltConnectionResult result = BoltConnector_acquire(connector, BOLT_ACCESS_MODE_READ);
            THEN("the connection should be connected") {
                REQUIRE(result.connection_status==BOLT_READY);
                REQUIRE(result.connection_error==BOLT_SUCCESS);
                REQUIRE(result.connection_error_ctx==nullptr);
                REQUIRE(result.connection!=nullptr);
                REQUIRE(result.connection->status==BOLT_READY);
            }
            BoltConnector_release(connector, result.connection);
        }
        BoltConnector_destroy(connector);
        BoltValue_destroy(auth_token);
    }
}

SCENARIO("Test reusing a pooled connection", "[integration][ipv6][secure][pooling]")
{
    GIVEN("a new connection pool with one entry") {
        const auto auth_token = BoltAuth_basic(BOLT_USER, BOLT_PASSWORD, NULL);
        struct BoltTrust trust{nullptr, 0, 1, 1};
        struct BoltConfig config{BOLT_DIRECT, BOLT_SECURE_SOCKET, &trust, BOLT_USER_AGENT, nullptr, nullptr, nullptr,
                                 1};
        struct BoltConnector* connector = BoltConnector_create(&BOLT_IPV6_ADDRESS, auth_token, &config);
        WHEN("a connection is acquired, released and acquired again") {
            struct BoltConnectionResult result1 = BoltConnector_acquire(connector, BOLT_ACCESS_MODE_READ);
            BoltConnector_release(connector, result1.connection);
            struct BoltConnectionResult result2 = BoltConnector_acquire(connector, BOLT_ACCESS_MODE_READ);
            THEN("the connection should be connected") {
                REQUIRE(result2.connection_status==BOLT_READY);
                REQUIRE(result2.connection_error==BOLT_SUCCESS);
                REQUIRE(result2.connection_error_ctx==nullptr);
                REQUIRE(result2.connection!=nullptr);
                REQUIRE(result2.connection->status==BOLT_READY);
            }
            AND_THEN("the same connection should have been reused") {
                REQUIRE(result1.connection==result2.connection);
            }
            BoltConnector_release(connector, result2.connection);
        }
        BoltConnector_destroy(connector);
        BoltValue_destroy(auth_token);
    }
}

SCENARIO("Test reusing a pooled connection that was abandoned", "[integration][ipv6][secure][pooling]")
{
    GIVEN("a new connection pool with one entry") {
        const auto auth_token = BoltAuth_basic(BOLT_USER, BOLT_PASSWORD, NULL);
        struct BoltTrust trust{nullptr, 0, 1, 1};
        struct BoltConfig config{BOLT_DIRECT, BOLT_SECURE_SOCKET, &trust, BOLT_USER_AGENT, nullptr, nullptr,
                                 nullptr, 1};
        struct BoltConnector* connector = BoltConnector_create(&BOLT_IPV6_ADDRESS, auth_token, &config);
        WHEN("a connection is acquired, released and acquired again") {
            struct BoltConnectionResult result1 = BoltConnector_acquire(connector, BOLT_ACCESS_MODE_READ);
            THEN("handle should include a valid connection") {
                REQUIRE(result1.connection_status==BOLT_READY);
                REQUIRE(result1.connection_error==BOLT_SUCCESS);
                REQUIRE(result1.connection_error_ctx==nullptr);
                REQUIRE(result1.connection!=nullptr);
            }
            const char* cypher = "RETURN 1";
            BoltConnection_set_run_cypher(result1.connection, cypher, strlen(cypher), 0);
            BoltConnection_load_run_request(result1.connection);
            BoltConnection_send(result1.connection);
            BoltConnector_release(connector, result1.connection);
            struct BoltConnectionResult result2 = BoltConnector_acquire(connector, BOLT_ACCESS_MODE_READ);
            THEN("handle should include a valid connection") {
                REQUIRE(result2.connection_status==BOLT_READY);
                REQUIRE(result2.connection_error==BOLT_SUCCESS);
                REQUIRE(result2.connection_error_ctx==nullptr);
                REQUIRE(result2.connection!=nullptr);
            }
            THEN("the connection should be connected") {
                REQUIRE(result2.connection->status==BOLT_READY);
            }
            AND_THEN("the same connection should have been reused") {
                REQUIRE(result1.connection==result2.connection);
            }
            BoltConnector_release(connector, result2.connection);
        }
        BoltConnector_destroy(connector);
        BoltValue_destroy(auth_token);
    }
}

SCENARIO("Test running out of connections", "[integration][ipv6][secure][pooling]")
{
    GIVEN("a new connection pool with one entry") {
        const auto auth_token = BoltAuth_basic(BOLT_USER, BOLT_PASSWORD, NULL);
        struct BoltTrust trust{nullptr, 0, 1, 1};
        struct BoltConfig config{BOLT_DIRECT, BOLT_SECURE_SOCKET, &trust, BOLT_USER_AGENT, nullptr, nullptr, nullptr,
                                 1};
        struct BoltConnector* connector = BoltConnector_create(&BOLT_IPV6_ADDRESS, auth_token, &config);
        WHEN("two connections are acquired in turn") {
            struct BoltConnectionResult result1 = BoltConnector_acquire(connector, BOLT_ACCESS_MODE_READ);
            struct BoltConnectionResult result2 = BoltConnector_acquire(connector, BOLT_ACCESS_MODE_READ);
            THEN("the first connection should be connected") {
                REQUIRE(result1.connection_status==BOLT_READY);
                REQUIRE(result1.connection_error==BOLT_SUCCESS);
                REQUIRE(result1.connection_error_ctx==nullptr);
                REQUIRE(result1.connection!=nullptr);
                REQUIRE(result1.connection->status==BOLT_READY);
            }
            AND_THEN("the second connection should be invalid") {
                REQUIRE(result2.connection_status==BOLT_DISCONNECTED);
                REQUIRE(result2.connection_error==BOLT_POOL_FULL);
                REQUIRE(result2.connection_error_ctx==nullptr);
                REQUIRE(result2.connection==nullptr);
            }
            BoltConnector_release(connector, result2.connection);
            BoltConnector_release(connector, result1.connection);
        }
        BoltConnector_destroy(connector);
        BoltValue_destroy(auth_token);
    }
}
