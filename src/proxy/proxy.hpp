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
#pragma once
#ifndef CLOUDBUS_PROXY
#define CLOUDBUS_PROXY
namespace cloudbus{
    namespace proxy {
        class proxy_base: public handler_type
        {
            public:
                using Base = handler_type;
                using trigger_type = Base::trigger_type;
                using event_type = Base::event_type;
                using events_type = Base::events_type;
                using event_mask = Base::event_mask;
                using size_type = Base::size_type;

                proxy_base() = default;
                trigger_type& triggers() { return _triggers; }
                int run() { return _run(); }
                virtual ~proxy_base() = default;

                proxy_base(proxy_base&& other) = delete;
                proxy_base(const proxy_base& other) = delete;
                proxy_base& operator=(proxy_base&& other) = delete;
                proxy_base& operator=(const proxy_base& other) = delete;

            protected:
                virtual int _run();

            private:
                trigger_type _triggers;
        };
        template<class ConnectorT>
        class basic_proxy: public proxy_base
        {
            public:
                using Base = proxy_base;
                using trigger_type = Base::trigger_type;
                using event_type = Base::event_type;
                using events_type = Base::events_type;
                using event_mask = Base::event_mask;
                using size_type = Base::size_type;

                using connector_type = ConnectorT;    

                basic_proxy(): _connector{Base::triggers()}{}
                connector_type& connector() { return _connector; }
                virtual ~basic_proxy() = default;

                basic_proxy(basic_proxy&& other) = delete;
                basic_proxy(const basic_proxy& other) = delete;
                basic_proxy& operator=(basic_proxy&& other) = delete;
                basic_proxy& operator=(const basic_proxy& other) = delete;
            private:
                connector_type _connector;
        };
        class proxy : public basic_proxy<proxy_connector>
        {
            public:
                using Base = basic_proxy<proxy_connector>;
                using trigger_type = Base::trigger_type;
                using events_type = Base::events_type;
                using event_mask = Base::event_mask;
                using size_type = Base::size_type;
                using connector_type = Base::connector_type;
                
                proxy();
                virtual ~proxy();

                proxy(const proxy& other) = delete;
                proxy(proxy&& other) = delete;
                proxy& operator=(proxy&& other) = delete;
                proxy& operator=(const proxy& other) = delete;
                
            protected:
                virtual size_type _handle(events_type& events) override;
        };
    }
}
#endif