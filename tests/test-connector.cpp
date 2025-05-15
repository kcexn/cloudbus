
/*
*   Copyright 2025 Kevin Exton
*   This file is part of Cloudbus.
*
*   Cloudbus is free software: you can redistribute it and/or modify it under the
*   terms of the GNU General Public License as published by the Free Software
*   Foundation, either version 3 of the License, or any later version.
*
*   Cloudbus is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
*   without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*   See the GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License along with Cloudbus.
*   If not, see <https://www.gnu.org/licenses/>.
*/
#include "tests.hpp"
#include "../src/connectors.hpp"
#include <thread>
using namespace cloudbus;
static int test_timer_initial_state() {
    TimerQueue tq;
    FAIL_IF(tq.processEvents() != 0); // No events to process
    return TEST_PASS;
}

static int test_timer_add_single_event_not_expired() {
    TimerQueue tq;
    bool callback_called = false;
    auto stream_sp = std::make_shared<interface_base::stream_type>();
    TimerQueue::weak_ptr stream_wp = stream_sp;

    tq.addEvent(stream_wp, std::chrono::steady_clock::now() + std::chrono::hours(1), // Expires far in the future
                [&]() { callback_called = true; });

    FAIL_IF(tq.processEvents() != 0); // Should not process the event
    FAIL_IF(callback_called);        // Callback should not have been called

    return TEST_PASS;
}

static int test_timer_add_and_process_single_expired_event() {
    TimerQueue tq;
    bool callback_called = false;
    auto stream_sp = std::make_shared<interface_base::stream_type>();
    TimerQueue::weak_ptr stream_wp = stream_sp;

    tq.addEvent(stream_wp, std::chrono::steady_clock::now() - std::chrono::seconds(1), // Already expired
                [&]() { callback_called = true; });

    FAIL_IF(tq.processEvents() != 1); // Should process one event
    FAIL_IF(!callback_called);       // Callback should have been called
    FAIL_IF(tq.processEvents() != 0); // Queue should be empty now

    return TEST_PASS;
}

static int test_timer_ordered_insertion_and_processing() {
    TimerQueue tq;
    std::vector<int> execution_order;
    auto stream_sp1 = std::make_shared<interface_base::stream_type>();
    auto stream_sp2 = std::make_shared<interface_base::stream_type>();
    auto stream_sp3 = std::make_shared<interface_base::stream_type>();

    auto now = std::chrono::steady_clock::now();

    // Add events with expiry times that will require ordered insertion
    tq.addEvent(stream_sp1, now + std::chrono::milliseconds(200), [&]() { execution_order.push_back(2); }); // Event 2
    tq.addEvent(stream_sp2, now + std::chrono::milliseconds(100), [&]() { execution_order.push_back(1); }); // Event 1 (earlier)
    tq.addEvent(stream_sp3, now + std::chrono::milliseconds(300), [&]() { execution_order.push_back(3); }); // Event 3

    // No events should be processed immediately
    FAIL_IF(tq.processEvents() != 0);
    FAIL_IF(!execution_order.empty());

    std::this_thread::sleep_for(std::chrono::milliseconds(150)); // Pass time for event 1
    FAIL_IF(tq.processEvents() != 1); // Process event 1
    FAIL_IF(execution_order.size() != 1 || execution_order[0] != 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Pass time for event 2 (total ~250ms)
    FAIL_IF(tq.processEvents() != 1); // Process event 2
    FAIL_IF(execution_order.size() != 2 || execution_order[1] != 2);

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Pass time for event 3 (total ~350ms)
    FAIL_IF(tq.processEvents() != 1); // Process event 3
    FAIL_IF(execution_order.size() != 3 || execution_order[2] != 3);

    FAIL_IF(tq.processEvents() != 0); // Queue should be empty

    std::vector<int> expected_order = {1, 2, 3};
    FAIL_IF(execution_order != expected_order);

    return TEST_PASS;
}

static int test_timer_multiple_events_expire_together() {
    TimerQueue tq;
    int callback1_count = 0;
    int callback2_count = 0;
    auto stream_sp1 = std::make_shared<interface_base::stream_type>();
    auto stream_sp2 = std::make_shared<interface_base::stream_type>();
    auto now = std::chrono::steady_clock::now();

    // Add two events that will expire by the next processing call
    tq.addEvent(stream_sp1, now + std::chrono::milliseconds(50), [&]() { callback1_count++; });
    tq.addEvent(stream_sp2, now + std::chrono::milliseconds(75), [&]() { callback2_count++; });

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Ensure both are expired

    FAIL_IF(tq.processEvents() != 2); // Both events should be processed
    FAIL_IF(callback1_count != 1);
    FAIL_IF(callback2_count != 1);
    FAIL_IF(tq.processEvents() != 0); // Queue should be empty

    return TEST_PASS;
}

static int test_timer_callback_handles_expired_stream_ptr() {
    TimerQueue tq;
    bool callback_run = false;
    bool attempted_action_on_null_stream = false;
    TimerQueue::weak_ptr stream_wp_that_will_expire;
    {
        auto stream_sp_temp = std::make_shared<interface_base::stream_type>();
        stream_wp_that_will_expire = stream_sp_temp;
    }
    tq.addEvent(stream_wp_that_will_expire,
        std::chrono::steady_clock::now() - std::chrono::milliseconds(50),
        [wp = stream_wp_that_will_expire, &callback_run, &attempted_action_on_null_stream]() {
            callback_run = true;
            if (auto locked_sp = wp.lock()) {
                // This block should not be entered
                attempted_action_on_null_stream = true;
            }
        }
    );
    FAIL_IF(tq.processEvents() != 1);
    FAIL_IF(!callback_run);
    FAIL_IF(attempted_action_on_null_stream); // Should be false, proving .lock() failed
    return TEST_PASS;
}

static int test_timer_no_events_processed_if_not_expired() {
    TimerQueue tq;
    int call_count = 0;
    auto stream_sp1 = std::make_shared<interface_base::stream_type>();
    auto stream_sp2 = std::make_shared<interface_base::stream_type>();

    tq.addEvent(stream_sp1, std::chrono::steady_clock::now() + std::chrono::seconds(1), [&](){ call_count++; });
    tq.addEvent(stream_sp2, std::chrono::steady_clock::now() + std::chrono::seconds(2), [&](){ call_count++; });

    FAIL_IF(tq.processEvents() != 0);
    FAIL_IF(call_count != 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(1100)); // First event expires
    FAIL_IF(tq.processEvents() != 1);
    FAIL_IF(call_count != 1);

    FAIL_IF(tq.processEvents() != 0); // Second event not yet expired
    FAIL_IF(call_count != 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Second event expires
    FAIL_IF(tq.processEvents() != 1);
    FAIL_IF(call_count != 2);
    FAIL_IF(tq.processEvents() != 0); // Queue empty

    return TEST_PASS;
}

int main(int argc, char **argv) {
    std::cout << "================================ TEST CONNECTOR ================================" << std::endl;
    EXEC_TEST(test_timer_initial_state);
    EXEC_TEST(test_timer_add_single_event_not_expired);
    EXEC_TEST(test_timer_add_and_process_single_expired_event);
    EXEC_TEST(test_timer_ordered_insertion_and_processing);
    EXEC_TEST(test_timer_multiple_events_expire_together);
    EXEC_TEST(test_timer_callback_handles_expired_stream_ptr);
    EXEC_TEST(test_timer_no_events_processed_if_not_expired);
    std::cout << "================================================================================" << std::endl;
    return TEST_PASS;
}
