#pragma once

#include <phobos/image.hpp>
#include <optional>

namespace ph {

struct Attachment {
	ph::ImageView view {};
    std::optional<ph::RawImage> image = std::nullopt;

    explicit inline operator bool() const {
        return view && image;
    }
};

}