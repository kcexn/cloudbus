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
#include "../io.hpp"
#pragma once
#ifndef CLOUDBUS_NODE
#define CLOUDBUS_NODE
namespace cloudbus{
    class node_base: public handler_type
    {
        public:
            using Base = handler_type;
            
            node_base() = default;
            trigger_type& triggers() { return _triggers; }
            int run() { return _run(); }
            virtual ~node_base() = default;

            node_base(node_base&& other) = delete;
            node_base(const node_base& other) = delete;
            node_base& operator=(node_base&& other) = delete;
            node_base& operator=(const node_base& other) = delete;                
        protected:
            virtual int _run();

        private:
            trigger_type _triggers;
    };
    template<class ConnectorT>
    class basic_node: public node_base
    {
        public:
            using Base = node_base;
            using connector_type = ConnectorT;

            basic_node(): _connector{triggers()}{}

            connector_type& connector() { return _connector; }

            virtual ~basic_node() = default;

            basic_node(basic_node&& other) = delete;
            basic_node(const basic_node& other) = delete;
            basic_node& operator=(basic_node&& other) = delete;
            basic_node& operator=(const basic_node& other) = delete;
        
        protected:
            virtual size_type _handle(events_type& events) override { return _connector.handle(events); }

        private:
            connector_type _connector;
    };
}
#endif