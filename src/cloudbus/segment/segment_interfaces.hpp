/*
*   Copyright 2025 Kevin ExtonGNU General Public License
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
#include "../../interfaces.hpp"
#pragma once
#ifndef CLOUDBUS_SEGMENT_INTERFACES
#define CLOUDBUS_SEGMENT_INTERFACES
namespace cloudbus{
    namespace segment {
        namespace {
            template<class T>
            static bool owner_equal(const std::weak_ptr<T>& p1, const std::shared_ptr<T>& p2){
                return !p1.owner_before(p2) && !p2.owner_before(p1);
            }
            template<class T>
            static bool owner_equal(const std::shared_ptr<T>& p1, const std::weak_ptr<T>& p2){
                return !p1.owner_before(p2) && !p2.owner_before(p1);
            }
            template<class T>
            static bool owner_equal(const std::weak_ptr<T>& p1, const std::weak_ptr<T>& p2){
                return !p1.owner_before(p2) && !p2.owner_before(p1);
            }
        }
        struct cbus_interface : public cs_interface {};
        struct service_interface : public ss_interface{};
    }
}
#endif