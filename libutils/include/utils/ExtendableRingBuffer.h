/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * This is not component from android code, just this project added.
 */

#pragma once

#include <utils/utils_export.h>

#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <cstdint>

class UTILS_EXPORT DetailBuffer
{

public:

    DetailBuffer(uint32_t capacity);

    void clear();

    uint32_t bytes_can_be_contain()const;

    uint32_t get_valid_size()const;

    uint32_t write_new_buffer(uint8_t const* buffer, uint32_t size);

    uint32_t read_buffer(uint8_t* buffer, uint32_t size);

    uint32_t capacity()const;

    std::vector<uint8_t> peer()const;

private:

    std::size_t m_validSize = 0;
    uint8_t* m_valid_buffer_addr = nullptr;
    std::shared_ptr<std::vector<uint8_t>> m_buffer;
    std::chrono::steady_clock::time_point m_lastUsedTime;
};

class UTILS_EXPORT ExtendableRingBuffer
{

public:

    ExtendableRingBuffer();

    ~ExtendableRingBuffer();

    std::size_t read(uint8_t* buffer, uint32_t size);

    std::size_t write(uint8_t const* buffer, uint32_t size);

    std::size_t size();

    std::size_t capacity();

    std::vector<uint8_t> peer_all();

private:

    DetailBuffer get_buffer( uint32_t size_need );

    std::recursive_timed_mutex m_mutex;
    std::vector<DetailBuffer> m_buffersToUse;  // The data waiting for user to read
    std::vector<DetailBuffer> m_buffersFree;   // The free buffers can be used for incoming data
};

