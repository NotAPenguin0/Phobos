#pragma once

#include <mutex>
#include <functional>
#include <unordered_map>

#include <phobos/hash.hpp>

namespace ph {

template<typename Key, typename Value>
class Cache {
private:
    struct Entry {
        Value data;
        Key key;
        size_t frames_since_last_usage = 0;
        bool used_this_frame = false;
    };
public:
    Cache() {
        mutex = std::make_unique<std::mutex>();
    }

    Cache(Cache const&) = delete;
    Cache(Cache&&) = default;
    Cache& operator=(Cache&&) = default;
    Cache& operator=(Cache const&) = delete;

    template<typename F>
    void foreach(F&& func) const {
        for (auto const& [_, val] : cache) {
            func(val.data);
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

    void lock() {
        mutex->lock();
    }

    void unlock() {
        mutex->unlock();
    }

private:
    std::unordered_map<size_t, Entry> cache;
    std::unique_ptr<std::mutex> mutex;
};

}