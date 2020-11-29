#include <phobos/core/queue.hpp>

namespace ph {

Queue::Queue(QueueInfo info, VkQueue handle) : info(info), handle(handle){

}

QueueType Queue::type() const {
	return info.type;
}

bool Queue::dedicated() const {
	return info.dedicated;
}

uint32_t Queue::family_index() const {
	return info.family_index;
}

}