#pragma once

#include <atomic>
#include <cstdint>

class job_couter {
  public:
    job_couter();
    ~job_couter();
    uint32_t add(uint32_t n);
    uint32_t sub(uint32_t n);
    uint32_t reset();
    uint32_t get();
    void wait_zero();

  private:
    std::atomic<uint32_t> couter;
};

class scoped_couter {
  public:
    scoped_couter(job_couter& couter);
    ~scoped_couter();

  private:
    job_couter* couter;
};
