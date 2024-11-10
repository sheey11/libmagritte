#pragma once

#include <cstdint>
extern "C" {

__attribute__((visibility("default"))) int magritte_init(const char* filepath);
__attribute__((visibility("default"))) bool magritte_put(int m_no, int32_t key, char* value, int len);
__attribute__((visibility("default"))) bool magritte_probe(int m_no, int32_t key);
__attribute__((visibility("default"))) bool magritte_get(int m_no, int32_t key, char* value, int* len);
__attribute__((visibility("default"))) bool magritte_remove(int m_no, int32_t key, char* value, int len);
__attribute__((visibility("default"))) bool magritte_shutdown(int m_no);
__attribute__((visibility("default"))) bool last_error(char* error);

}

