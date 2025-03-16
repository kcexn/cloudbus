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
#include "segment_connector.hpp"
#include "../node.hpp"
#pragma once
#ifndef CLOUDBUS_SEGMENT
#define CLOUDBUS_SEGMENT
namespace cloudbus{
    namespace segment {
        class segment : public basic_node<segment_connector>
        {
            public:
                using Base = basic_node<segment_connector>;
                
                segment(const config::configuration::section& section):
                    Base(section){}
                virtual ~segment() = default;

                segment() = delete;
                segment(const segment& other) = delete;
                segment(segment&& other) = delete;
                segment& operator=(segment&& other) = delete;
                segment& operator=(const segment& other) = delete;
        };
    }
}
#endif