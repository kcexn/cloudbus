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
#include <memory>
#include <tuple>
#include <vector>

#pragma once
#ifndef CLOUDBUS_MARSHALLERS
#define CLOUDBUS_MARSHALLERS
namespace cloudbus {
    template<class NorthT, class SouthT>
    class basic_marshaller
    {
        public:
            using north_type = NorthT;
            using north_ptr = std::weak_ptr<typename north_type::traits_type::stream_type>;
            using north_format = typename north_type::traits_type::format_type;
            using north_buffer = std::tuple<north_ptr, north_format>;
            using north_buffers = std::vector<north_buffer>;
            using south_type = SouthT;
            using south_ptr = std::weak_ptr<typename south_type::traits_type::stream_type>;
            using south_format = typename south_type::traits_type::format_type;
            using south_buffer = std::tuple<south_ptr, south_format>;
            using south_buffers = std::vector<south_buffer>;

            basic_marshaller() = default;
            basic_marshaller(const north_buffers& north, const south_buffers& south)
            :   _north{north}, _south{south} {}
            basic_marshaller(const basic_marshaller& other)
            :   _north{other._north}, _south{other._south}{}
            basic_marshaller(basic_marshaller&& other)
            :   _north{std::move(other._north)}, _south{std::move(other._south)}{}

            basic_marshaller& operator=(const basic_marshaller& other){
                _north = other._north;
                _south = other._south;
                return *this;
            }
            basic_marshaller& operator=(basic_marshaller&& other){
                _north = std::move(other._north);
                _south = std::move(other._south);
                return *this;
            }

            typename north_buffers::iterator unmarshal(const typename north_type::stream_type& n){ return _unmarshal(n); }
            typename south_buffers::iterator marshal(const typename south_type::stream_type& s){ return _marshal(s); }
            north_buffers& north() { return _north; }
            south_buffers& south() { return _south; }

            virtual ~basic_marshaller() = default;
        protected:
            virtual typename north_buffers::iterator _unmarshal(const typename north_type::stream_type& n) { return _north.end(); }
            virtual typename south_buffers::iterator _marshal(const typename south_type::stream_type& s) { return _south.end(); }

        private:
            north_buffers _north;
            south_buffers _south;
    };
}
#endif
