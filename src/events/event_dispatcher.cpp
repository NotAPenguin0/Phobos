#include <phobos/events/event_dispatcher.hpp>
#include <algorithm>

namespace ph {

template<typename T>
static void fire_event_impl(T const& evt, stl::vector<EventListener<T>*>& listeners) {
    for (auto listener : listeners) {
        listener->on_event(evt);
    }
}

template<typename T>
static void add_listener_impl(stl::vector<EventListener<T>*>& listeners, EventListener<T>* listener) {
    listeners.push_back(listener);
}

template<typename T>
static void remove_listener_impl(stl::vector<EventListener<T>*>& listeners, EventListener<T>* listener) {
    auto it = std::find(listeners.begin(), listeners.end(), listener);
    if (it != listeners.end()) {
        listeners.erase(it);
    }
}


#define EVENT_DISPATCH_IMPL(EvtType, EvtName) \
    void EventDispatcher::fire_event(EvtType const& e) { \
        fire_event_impl(e, EvtName##_listeners); \
    } \
    void EventDispatcher::add_listener(EventListener<EvtType>* listener) { \
       add_listener_impl(EvtName##_listeners, listener); \
    } \
    void EventDispatcher::remove_listener(EventListener<EvtType>* listener) { \
        remove_listener_impl(EvtName##_listeners, listener); \
    }


EVENT_DISPATCH_IMPL(DynamicGpuBufferResizeEvent, dynamic_gpu_buffer_resize)
EVENT_DISPATCH_IMPL(WindowResizeEvent, window_resize);
EVENT_DISPATCH_IMPL(SwapchainRecreateEvent, swapchain_recreate);

#undef EVENT_DISPATCH_IMPL

}