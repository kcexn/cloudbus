/*
*   Copyright 2025 Kevin Exton
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
#include "node/node.hpp"
#include "cloudbus/controller/controller_connector.hpp"
#include "cloudbus/segment/segment_connector.hpp"
#pragma once
#ifndef CLOUDBUS_NODES
#define CLOUDBUS_NODES
namespace cloudbus {
    using controller_type = basic_node<controller::connector>;
    using segment_type = basic_node<segment::connector>;
}
#endif