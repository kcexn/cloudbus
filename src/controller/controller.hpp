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
#pragma once
#ifndef CLOUDBUS_CONTROLLER
#define CLOUDBUS_CONTROLLER
namespace cloudbus{
    namespace controller {
        class controller_base: public handler_type
        {
            public:
                using Base = handler_type;
                using trigger_type = Base::trigger_type;
                using event_type = Base::event_type;
                using events_type = Base::events_type;
                using event_mask = Base::event_mask;
                using size_type = Base::size_type;
                
                controller_base() = default;
                trigger_type& triggers() { return _triggers; }
                int run() { return _run(); }
                virtual ~controller_base() = default;

                controller_base(controller_base&& other) = delete;
                controller_base(const controller_base& other) = delete;
                controller_base& operator=(controller_base&& other) = delete;
                controller_base& operator=(const controller_base& other) = delete;                
            protected:
                virtual int _run();

            private:
                trigger_type _triggers;
        };
        template<class ConnectorT>
        class basic_controller: public controller_base
        {
            public:
                using Base = controller_base;
                using trigger_type = Base::trigger_type;
                using event_type = Base::event_type;
                using events_type = Base::events_type;
                using event_mask = Base::event_mask;
                using size_type = Base::size_type;               
                using connector_type = ConnectorT;          

                basic_controller(): _connector{Base::triggers()}{}
                connector_type& connector() { return _connector; }
                virtual ~basic_controller() = default;

                basic_controller(basic_controller&& other) = delete;
                basic_controller(const basic_controller& other) = delete;
                basic_controller& operator=(basic_controller&& other) = delete;
                basic_controller& operator=(const basic_controller& other) = delete;
            private:
                connector_type _connector;
        };
        class controller : public basic_controller<control_connector>
        {
            public:
                using Base = basic_controller<control_connector>;
                using event_type = Base::event_type;
                using events_type = Base::events_type;
                using event_mask = Base::event_mask;
                using size_type = Base::size_type;
                using connector_type = Base::connector_type;
                
                controller();
                virtual ~controller();

                controller(const controller& other) = delete;
                controller(controller&& other) = delete;
                controller& operator=(controller&& other) = delete;
                controller& operator=(const controller& other) = delete;
                
            protected:
                virtual size_type _handle(events_type& events) override;
        };
    }
}
#endif