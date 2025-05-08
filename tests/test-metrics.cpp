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
#include "../src/metrics.hpp"
using namespace cloudbus;
static int test_metrics_constructor() {
    auto& m1 = metrics::get();
    auto& m2 = metrics::get();
    FAIL_IF(&m1 != &m2);
    return TEST_PASS;
}
static int test_metrics_make_node() {
    const std::thread::id t1{1}, t2{2};
    auto& node1 = metrics::get().make_node(std::thread::id(1));
    auto& node2 = metrics::get().make_node(std::thread::id(2));
    FAIL_IF(&node1 == &node2);
    auto m = metrics::get().get_all_measurements();
    FAIL_IF(m.size() != 2);
    metrics::get().erase_node(t1);
    metrics::get().erase_node(t2);
    return TEST_PASS;
}
static int test_metrics_get_arrivals() {
    const std::thread::id t1{1}, t2{2};
    auto& node1 = metrics::get().make_node(t1);
    node1.arrivals = 100;
    auto& node2 = metrics::get().make_node(t2);
    node2.arrivals = 200;
    FAIL_IF(node1.arrivals == node2.arrivals);
    FAIL_IF(metrics::get().arrivals(t1) != node1.arrivals);
    FAIL_IF(metrics::get().arrivals(t2) != node2.arrivals);
    metrics::get().erase_node(t1);
    metrics::get().erase_node(t2);
    return TEST_PASS;
}
static int test_metrics_get_streams() {
    using clock_type = stream_metrics::clock_type;
    using weak_ptr = stream_metrics::weak_ptr;
    auto t = clock_type::now();
    weak_ptr ptr;
    auto interval = metrics::get().streams().add_arrival(ptr, t);
    FAIL_IF(interval.count() != -1);
    metrics::get().erase_node();
    return TEST_PASS;
}
static int test_metrics_add_arrival() {
    using clock_type = stream_metrics::clock_type;
    using duration_type = stream_metrics::duration_type;
    using weak_ptr = stream_metrics::weak_ptr;
    using shared_ptr = std::shared_ptr<stream_metrics::stream_type>;
    shared_ptr sp = std::make_shared<stream_metrics::stream_type>();
    weak_ptr wp = sp;
    auto t = clock_type::now();
    auto interval = metrics::get().streams().add_arrival(wp, t);
    FAIL_IF(interval.count() != 0);
    t += duration_type(100);
    interval = metrics::get().streams().add_arrival(wp, t);
    FAIL_IF(interval.count() != 50);
    metrics::get().erase_node();
    return TEST_PASS;
}
static int test_metrics_add_completion() {
    using clock_type = stream_metrics::clock_type;
    using weak_ptr = stream_metrics::weak_ptr;
    using shared_ptr = std::shared_ptr<stream_metrics::stream_type>;
    shared_ptr sp = std::make_shared<stream_metrics::stream_type>();
    weak_ptr wp = sp;
    auto t = clock_type::now();
    auto interval = metrics::get().streams().add_completion(wp, t);
    FAIL_IF(interval.count() != 0);
    const auto& m = metrics::get().streams().get_all_measurements();
    FAIL_IF(m.size() != 1);
    metrics::get().erase_node();
    return TEST_PASS;
}
int main(int argc, char **argv) {
    std::cout << "================================= TEST METRICS =================================" << std::endl;
    EXEC_TEST(test_metrics_constructor);
    EXEC_TEST(test_metrics_make_node);
    EXEC_TEST(test_metrics_get_arrivals);
    EXEC_TEST(test_metrics_get_streams);
    EXEC_TEST(test_metrics_add_arrival);
    EXEC_TEST(test_metrics_add_completion);
    std::cout << "================================================================================" << std::endl;
    return TEST_PASS;
}