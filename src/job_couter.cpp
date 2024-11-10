#include "job_conter.h"
#include <cstdint>
#include <thread>

job_couter::job_couter() { this->couter.store(0); }
job_couter::~job_couter() {}

uint32_t job_couter::get() { return this->couter.load(); }
uint32_t job_couter::add(uint32_t n) { return this->couter.fetch_add(n); }
uint32_t job_couter::sub(uint32_t n) { return this->couter.fetch_sub(n); }

uint32_t job_couter::reset() { return this->couter.exchange(0); }

void job_couter::wait_zero() {
    // this->couter.wait(0);
    auto count = 0;
    while (this->couter.load() != 0) {
        if (++count > 1000)
            std::this_thread::yield();
    }
}

scoped_couter::scoped_couter(job_couter& couter) {
    this->couter = &couter;
    couter.add(1);
}
scoped_couter::~scoped_couter() { couter->sub(1); }
