#include "magritte.h"
#include "magritte_impl.h"
#include <cstring>
#include <unordered_map>
#include <utility>

std::unordered_map<uint, Magritte> instances;
std::atomic<uint> couter;
std::string last_error_str;

int magritte_init(const char* filepath) {
    auto index = couter.fetch_add(1);
    instances.emplace(std::make_pair(index, Magritte(filepath, nullptr)));
    return index;
}

bool magritte_put(int m_no, int32_t key, char* value, int len) {
    if (len > 1024) {
        last_error_str = "Value too long";
        return false;
    }

    std::vector<char> v;
    v.reserve(len);
    memcpy(v.data(), value, len);

    return instances.at(m_no).put(key, v);
}

bool magritte_get(int m_no, int32_t key, char* value, int* len) {
    std::vector<char> v;
    if (!instances.at(m_no).get(key, v)) {
        last_error_str = "Key not found";
        return false;
    }
    if (v.size() > *len) {
        last_error_str = "Buffer too small";
        return false;
    }
    memcpy(value, v.data(), v.size());
    *len = v.size();
    return true;
}

bool magritte_probe(int m_no, int32_t key) {
    return instances.at(m_no).probe(key);
}

bool magritte_remove(int m_no, int32_t key, char* value, int* len) {
    std::vector<char> v;
    if (!instances.at(m_no).remove(key, v)) {
        last_error_str = "Key not found";
        return false;
    }
    if (v.size() > *len) {
        last_error_str = "Buffer too small";
        return false;
    }
    memcpy(value, v.data(), v.size());
    *len = v.size();
    return true;
}

bool magritte_shutdown(int m_no) {
    instances.at(m_no).shutdown();
    instances.erase(m_no);
    return true;
}

bool last_error(char* error) {
    if (last_error_str.size() == 0) {
        return false;
    }
    memcpy(error, last_error_str.data(), last_error_str.size());
    return true;
}

