#ifndef PHOBOS_VERSION_HPP_
#define PHOBOS_VERSION_HPP_

namespace ph {

struct Version {
    size_t major, minor, patch;
};

}

#define PHOBOS_VERSION ::ph::Version{0, 0, 1}

#endif