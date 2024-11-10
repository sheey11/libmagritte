#pragma once

#include <atomic>
#include <iostream>
#include <unistd.h>
#include <vector>

template <typename T> class channel {
  public:
    channel(int capacity);
    ~channel();
    channel& operator<<(T value);
    channel& operator>>(T& dest);

    void push(T value);
    T pop();
    std::pair<T, bool> pop_timeout(uint32_t timeout);
    int length();

  private:
    int full_w_pipe_no;
    int empty_r_pipe_no;
    int empty_w_pipe_no;
    int full_r_pipe_no;
    std::vector<T> buffer;
    std::atomic<bool> closed;
    std::atomic<int> len;
};

#include <bits/types/struct_timeval.h>
#include <channel.h>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/select.h>
#include <unistd.h>
#include <utility>

template <typename T> channel<T>::channel(int size) : closed(false), len(size) {
    std::fstream pipe_max_file("/proc/sys/fs/pipe-max-size", std::ios_base::in);
    int curr_pipe_size;
    pipe_max_file >> curr_pipe_size;

    if (size * 4 > curr_pipe_size) {
        throw std::invalid_argument("channel size is too large, try setting "
                                    "/proc/sys/fs/pipe-max-size to " +
                                    std::to_string(size * 4) +
                                    " or larger(currently " +
                                    std::to_string(curr_pipe_size) + ")");
    }

    int pipedes[2];
    pipe(pipedes);
    this->empty_r_pipe_no = pipedes[0];
    this->empty_w_pipe_no = pipedes[1];

    pipe(pipedes);
    this->full_r_pipe_no = pipedes[0];
    this->full_w_pipe_no = pipedes[1];

    buffer.resize(size);

    for (uint32_t i = 0; i < size; i++)
        write(this->empty_w_pipe_no, &i, 4);
}

template <typename T> channel<T>::~channel() {
    close(this->empty_r_pipe_no);
    close(this->empty_w_pipe_no);
    close(this->full_r_pipe_no);
    close(this->full_w_pipe_no);
}

template <typename T> channel<T>& channel<T>::operator<<(T value) {
    if (closed)
        throw std::invalid_argument("channel is closed");

    uint32_t idx;
    read(this->empty_r_pipe_no, &idx, 4);
    this->buffer[idx] = value;
    write(this->full_w_pipe_no, &idx, 4);

    len--;
    return *this;
}

template <typename T> channel<T>& channel<T>::operator>>(T& dest) {
    if (closed)
        throw std::invalid_argument("channel is closed");

    uint32_t idx;
    read(this->full_r_pipe_no, &idx, 4);
    dest = this->buffer[idx];
    write(this->empty_w_pipe_no, &idx, 4);

    len++;
    return *this;
}

template <typename T> void channel<T>::push(T value) { this << value; }

template <typename T> T channel<T>::pop() {
    uint32_t idx;
    read(this->full_r_pipe_no, &idx, 4);
    auto dest = this->buffer[idx];
    write(this->empty_w_pipe_no, &idx, 4);

    len++;
    return dest;
}

template <typename T> int channel<T>::length() { return this->len; }

template <typename T>
std::pair<T, bool> channel<T>::pop_timeout(uint32_t timeout) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(this->full_r_pipe_no, &set);

    struct timeval timeout_tv{
        .tv_sec = timeout / 1000,
        .tv_usec = (timeout % 1000) * 1000,
    };
    auto retval = select(this->full_r_pipe_no + 1, &set, nullptr, nullptr, &timeout_tv);

    if (retval > 0) {
        return std::make_pair(this->pop(), true);
    } else {
        return std::make_pair(nullptr, false);
    }
}

