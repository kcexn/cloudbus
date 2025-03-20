/*     
*   Copyright 2024 Kevin Exton
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
#include "io.hpp"
#include <poll.h>
namespace io{
    static constexpr std::size_t SHRINK_THRESHOLD = 1024;
    poller::size_type poller::_add(native_handle_type handle, events_type& events, event_type event){
        for(const auto& e: events)
            if(e.fd == handle)
                return npos;
        events.push_back(event);
        if(events.capacity() > SHRINK_THRESHOLD
                && events.size() < events.capacity()/2)
            events.shrink_to_fit();
        return events.size();
    }
    
    poller::size_type poller::_update(native_handle_type handle, events_type& events, event_type event){
        for(auto& e: events){
            if(e.fd == handle){
                e.events = event.events;
                return events.size();
            }
        }
        return npos;
    }
    
    poller::size_type poller::_del(native_handle_type handle, events_type& events){
        for(auto cit=events.cbegin(); cit < events.cend(); ++cit){
            if(cit->fd == handle){
                events.erase(cit);
                return events.size();
            }
        }
        return npos;
    }
    
    poller::size_type poller::_poll(duration_type timeout){
        while(int nfds = poll(Base::events(), Base::size(), timeout.count())){
            if(nfds < 0){
                switch(errno){
                    case EAGAIN:
                    case EINTR:
                        continue;
                    default:
                        return npos;
                }
            }
            return nfds;
        }
        return 0;
    }
    
    trigger::event_type trigger::mkevent(native_handle_type handle, trigger_type trigger){
        return event_type{handle, static_cast<short>(trigger)};
    }
}
