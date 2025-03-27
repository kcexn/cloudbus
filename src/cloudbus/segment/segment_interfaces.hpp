/* 
*   Copyright 2025 Kevin ExtonGNU Affero General Public License
*   This file is part of Cloudbus.
*
*   Cloudbus is free software: you can redistribute it and/or modify it under the 
*   terms of the GNU Affero General Public License as published by the Free Software 
*   Foundation, either version 3 of the License, or any later version.
*
*   Cloudbus is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
*   without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
*   See the GNU Affero General Public License for more details.
*
*   You should have received a copy of the GNU Affero General Public License along with Cloudbus. 
*   If not, see <https://www.gnu.org/licenses/>. 
*/
#include "../../interfaces.hpp"
#pragma once
#ifndef CLOUDBUS_SEGMENT_INTERFACES
#define CLOUDBUS_SEGMENT_INTERFACES
namespace cloudbus{
    namespace segment {
        struct cbus_interface : public cs_interface {};
        struct service_interface : public ss_interface{};
    }
}
#endif