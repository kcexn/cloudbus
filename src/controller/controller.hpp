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
#include "control_connector.hpp"
#include "../node.hpp"
#pragma once
#ifndef CLOUDBUS_CONTROLLER
#define CLOUDBUS_CONTROLLER
namespace cloudbus{
    namespace controller {
        class controller : public basic_node<control_connector>
        {
            public:
                using Base = basic_node<control_connector>;
                
                controller();
                virtual ~controller();

                controller(const controller& other) = delete;
                controller(controller&& other) = delete;
                controller& operator=(controller&& other) = delete;
                controller& operator=(const controller& other) = delete;

            protected:
                virtual int _signal_handler(std::uint64_t signal) override;
        };
    }
}
#endif