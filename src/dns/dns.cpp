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
#include "dns.hpp"
#include <stdexcept>
namespace cloudbus {
    namespace dns {
        dns_base::ares_handle_type dns_base::make_handle(const options_type& options, const int& optmask){
            ares_handle_type tmp{nullptr, {}};
            auto&[handle, time] = tmp;
            if(ares_init_options(&handle, &options, optmask) != ARES_SUCCESS)
                throw std::runtime_error("Unable to initialize ares channel.");
            return tmp;
        }
        dns_base::dns_base(const options_type& options, const int& optmask):
            _opts{options},
            _optmask{optmask}
        {
            if(ares_library_init(ARES_LIB_INIT_ALL))
                throw std::runtime_error("c-ares failed to initialize.");
            _handles.push_back(std::move(make_handle(options, optmask)));
        }

        dns_base::~dns_base(){
            for(auto&[handle, time]: _handles)
                ares_destroy(handle);
            ares_library_cleanup();
        }
    }
}