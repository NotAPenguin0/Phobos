#include <phobos/events/event_dispatcher.hpp>
#include <algorithm>

namespace ph {

#define EVENT_DISPATCH_IMPL(EvtType, EvtName) \
    void EventDispatcher::fire_event(EvtType const& e) { \
        for (auto listener : EvtName##_listeners) { \
            listener->on_event(e); \
        } \
    } \
    void EventDispatcher::add_listener(EventListener<EvtType>* listener) { \
        EvtName##_listeners.push_back(listener); \
    } \
    void EventDispatcher::remove_listener(EventListener<EvtType>* listener) { \
        EvtName##_listeners.erase( \
            std::find(EvtName##_listeners.begin(), EvtName##_listeners.end(), listener)); \
    }


EVENT_DISPATCH_IMPL(InstancingBufferResizeEvent, instancing_buffer_resize)
EVENT_DISPATCH_IMPL(WindowResizeEvent, window_resize);
EVENT_DISPATCH_IMPL(SwapchainRecreateEvent, swapchain_recreate);

#undef EVENT_DISPATCH_IMPL

}