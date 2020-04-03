#ifndef PHOBOS_EVENT_LISTENER_HPP_
#define PHOBOS_EVENT_LISTENER_HPP_

namespace ph {

template<typename Event>
class EventListener {
public:
    friend class EventDispatcher;
    
    virtual void on_event(Event const& e) = 0;
};

}

#endif