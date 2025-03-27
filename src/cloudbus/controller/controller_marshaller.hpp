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
#include "controller_interfaces.hpp"
#include "../../marshallers.hpp"

#pragma once
#ifndef CLOUDBUS_CONTROLLER_MARSHALLERS
#define CLOUDBUS_CONTROLLER_MARSHALLERS
namespace cloudbus{
    namespace controller {
        class marshaller : public basic_marshaller<cs_north, cs_south>
        {
            public:
                using Base = basic_marshaller<cs_north, cs_south>;

            protected:
                virtual north_buffers::iterator _unmarshal(const north_type::handle_type& stream) override;
                virtual south_buffers::iterator _marshal(const north_type::handle_type& stream) override;
        };
    }
}
#endif