#pragma once

#include <vector>

namespace ph {

class WindowInterface {
public:
	virtual std::vector<const char*> window_extension_names() const = 0;
private:

};

}