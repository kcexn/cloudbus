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
#include <ares.h>
#pragma once
#ifndef CLOUDBUS_DNS
#define CLOUDBUS_DNS
namespace cloudbus {
    namespace dns {
        class resolver
        {
            public:
                resolver();
                ~resolver();

                resolver(const resolver& other) = delete;
                resolver& operator=(const resolver& other) = delete;
                resolver(resolver&& other) = delete;
                resolver& operator=(resolver&& other) = delete;

            private:
                ares_channel _channel;
        };
    }
}
#endif