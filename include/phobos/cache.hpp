#pragma once

#include <mutex>
#include <functional>
#include <unordered_map>
#include <algorithm>

#include <phobos/hash.hpp>

namespace ph {

template<typename Key, typename Value>
class Cache {
private:
    struct Entry {
        Value data{};
        Key key{};
        mutable size_t frames_since_last_usage = 0;
        mutable bool used_this_frame = false;
    };
public:
    Cache(uint32_t max_frames = 0) {
        mutex = std::make_unique<std::mutex>();
        this->max_frames = max_frames;
    }

    Cache(Cache const&) = delete;
    Cache(Cache&&) = default;
    Cache& operator=(Cache&&) = default;
    Cache& operator=(Cache const&) = delete;

    template<typename F>
    void foreach(F&& func) const {
        std::lock_guard lock(*mutex);
        for (auto const& [_, val] : cache) {
            func(val.data);
        }
    }


    template<typename F>
    void foreach_unused(F&& func) {
        std::lock_guard lock(*mutex);
        for (auto& [_, val] : cache) {
            if (val.frames_since_last_usage > max_frames) {
                func(val.data);
            }
        }
    }

    // Remove unused entries from the cache
    void erase_unused() {
        std::lock_guard lock(*mutex);
        auto find_unused_func = [this](std::pair<size_t const, Entry> const& item) -> bool {
            return item.second.frames_since_last_usage > max_frames;
        };
        for (auto it = cache.begin(); it != cache.end();) {
            if (find_unused_func(*it)) {
                it = cache.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    void insert(Key const& key, Value val) {
        size_t hash = std::hash<Key>()(key);
        std::lock_guard lock(*mutex);
        cache[hash] = Entry{ .data = std::move(val), .key = key, .frames_since_last_usage = 0, .used_this_frame = false };
    }

    void insert(Key&& key, Value&& val) {
        size_t hash = std::hash<Key>()(key);
        std::lock_guard lock(*mutex);
        cache[hash] = Entry{ .data = std::move(val), .key = std::move(key), .frames_since_last_usage = 0, .used_this_frame = false };
    }

    Value* get(Key const& key) {
        size_t hash = std::hash<Key>()(key);
        std::lock_guard lock(*mutex);
        auto it = cache.find(hash);
        if (it != cache.end()) {
            it->second.used_this_frame = true;
            return &it->second.data;
        }
        else return nullptr;
    }

    Value const* get(Key const& key) const {
        size_t hash = std::hash<Key>()(key);
        std::lock_guard lock(*mutex);
        auto it = cache.find(hash);
        if (it != cache.end()) {
            // Mark the entry as used
            it->second.used_this_frame = true;
            return &it->second.data;
        }
        else return nullptr;
    }

    Key* get_key(size_t hash) {
        std::lock_guard lock(*mutex);
        return &cache.at(hash).key;
    }

    size_t get_frames_since_last_usage(Key const& key) {
        size_t hash = std::hash<Key>()(key);
        std::lock_guard lock(*mutex);
        return cache[hash].frames_since_last_usage;
    }

    // Update frames since last usage for every entry
    void next_frame() {
        std::lock_guard lock(*mutex);
        for (auto& [key, entry] : cache) {
            if (!entry.used_this_frame) {
                entry.frames_since_last_usage += 1;
            }
            else {
                entry.frames_since_last_usage = 0;
            }
            entry.used_this_frame = false;
        }
    }

    void lock() {
        mutex->lock();
    }

    void unlock() {
        mutex->unlock();
    }

    void set_max_frames(uint32_t frames) {
        max_frames = frames;
    }

private:
    std::unordered_map<size_t, Entry> cache;
    std::unique_ptr<std::mutex> mutex;

    uint32_t max_frames = 0;
};

}