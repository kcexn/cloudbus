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
#pragma once
#ifndef CLOUDBUS_SEGMENT
#define CLOUDBUS_SEGMENT
namespace cloudbus{
    namespace segment {
        template<class ConnectorT>
        class basic_segment: public handler_type
        {
            public:
                using Base = handler_type;
                using trigger_type = Base::trigger_type;
                using event_type = Base::event_type;
                using events_type = Base::events_type;
                using event_mask = Base::event_mask;                    

                using connector_type = ConnectorT;          

                basic_segment(): _connector{_triggers}{}

                trigger_type& triggers() { return _triggers; }              
                connector_type& connector() { return _connector; }

                virtual ~basic_segment() = default;

                basic_segment(basic_segment&& other) = delete;
                basic_segment(const basic_segment& other) = delete;
                basic_segment& operator=(basic_segment&& other) = delete;
                basic_segment& operator=(const basic_segment& other) = delete;

            private:
                trigger_type _triggers;
                connector_type _connector;
        };
        
        class segment : public basic_segment<segment_connector>
        {
            public:
                using Base = basic_segment<segment_connector>;
                using trigger_type = Base::trigger_type;
                using events_type = Base::events_type;
                using event_mask = Base::event_mask;
                using connector_type = Base::connector_type;
                
                segment();
                int run();
                virtual ~segment();

                segment(const segment& other) = delete;
                segment(segment&& other) = delete;
                segment& operator=(segment&& other) = delete;
                segment& operator=(const segment& other) = delete;
                
            protected:
                virtual int _handle(events_type& events) override;
        };
    }
}
#endif