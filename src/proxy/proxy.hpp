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
#include "proxy_connector.hpp"
#include "../node.hpp"
#pragma once
#ifndef CLOUDBUS_PROXY
#define CLOUDBUS_PROXY
namespace cloudbus{
    namespace proxy {
        class proxy : public basic_node<proxy_connector>
        {
            public:
                using Base = basic_node<proxy_connector>;
                
                proxy();
                virtual ~proxy();

                proxy(const proxy& other) = delete;
                proxy(proxy&& other) = delete;
                proxy& operator=(proxy&& other) = delete;
                proxy& operator=(const proxy& other) = delete;

            protected:
                int _signal_handler(std::uint64_t signal) override;
        };
    }
}
#endif