#include <utils/ExtendableRingBuffer.h>


#ifdef _DEBUG
//#define EXTENDED_RING_BUFFER_DEBUG
#endif

static constexpr uint32_t s_buffer_total_size =
#ifdef EXTENDED_RING_BUFFER_DEBUG
    8;
#else
    2048;
#endif

DetailBuffer::DetailBuffer(uint32_t capacity)
{
    capacity = capacity < s_buffer_total_size ? s_buffer_total_size : capacity;
    m_buffer = std::make_shared<std::vector<uint8_t>>();
    m_buffer->reserve(capacity);
    m_buffer->resize(m_buffer->capacity());
    clear();
}

void DetailBuffer::clear()
{
    m_validSize = 0;
    m_valid_buffer_addr = m_buffer->data();
}

uint32_t DetailBuffer::bytes_can_be_contain()const
{
    uint8_t const* buffer_start = m_buffer->data();
    uint8_t const* buffer_end = buffer_start;
    buffer_end += m_buffer->capacity();
#ifdef EXTENDED_RING_BUFFER_DEBUG
    if (m_valid_buffer_addr < buffer_start ||
        m_valid_buffer_addr > buffer_end)
    {
        std::abort();
    }
#endif
    return buffer_end - m_valid_buffer_addr - m_validSize;
}

uint32_t DetailBuffer::get_valid_size()const
{
    return m_validSize;
}

uint32_t DetailBuffer::write_new_buffer(uint8_t const* buffer, uint32_t size)
{
    uint32_t available_size = bytes_can_be_contain();
    constexpr uint32_t minimum_available_size = (s_buffer_total_size > 20 ? 20 : s_buffer_total_size);
    if (available_size <
#ifdef EXTENDED_RING_BUFFER_DEBUG
        1
#else
        minimum_available_size
#endif
        )
    {
        return 0;
    }

    uint32_t size_to_write = available_size > size ? size : available_size;
    memcpy(m_valid_buffer_addr + m_validSize, buffer, size_to_write);
    m_validSize += size_to_write;

    if (m_validSize > m_buffer->capacity())
    {
        std::abort();
    }

    return size_to_write;
}

uint32_t DetailBuffer::read_buffer(uint8_t* buffer, uint32_t size)
{
    uint32_t size_to_read = m_validSize > size ? size : m_validSize;
    memcpy(buffer, m_valid_buffer_addr, size_to_read);

#ifdef EXTENDED_RING_BUFFER_DEBUG
    uint8_t const* buffer_start = m_buffer->data();
    uint8_t const* buffer_end = buffer_start;
    buffer_end += m_buffer->capacity();
    if (m_valid_buffer_addr < buffer_start ||
        m_valid_buffer_addr > buffer_end)
    {
        std::abort();
    }
#endif

    m_validSize -= size_to_read;
    m_valid_buffer_addr += size_to_read;

    if (m_validSize > m_buffer->capacity())
    {
        std::abort();
    }

    if (m_validSize == 0)
    {
        clear();
    }

    return size_to_read;
}

uint32_t DetailBuffer::capacity()const
{
    return m_buffer->capacity();
}

std::vector<uint8_t> DetailBuffer::peer()const
{
    std::vector<uint8_t> ret;
    auto beg = m_buffer->begin();
    auto end = beg;
    std::advance(end, m_validSize);
    ret.insert(ret.end(), beg, end);
    return ret;
}


ExtendableRingBuffer::ExtendableRingBuffer()
{

}

ExtendableRingBuffer::~ExtendableRingBuffer()
{

}

std::size_t ExtendableRingBuffer::read(uint8_t* buffer, uint32_t size)
{
    uint32_t ret = 0;
    uint32_t size_need_read = size;
    uint8_t* p_buffer = buffer;

    std::lock_guard<std::recursive_timed_mutex> locker(m_mutex);
    while (size_need_read > 0)
    {
        if (m_buffersToUse.empty())
        {
            break;
        }

        uint32_t size_read = 0;
        DetailBuffer& front = m_buffersToUse.front();
        size_read = front.read_buffer(p_buffer, size_need_read);
        size_need_read -= size_read;
        p_buffer += size_read;
        ret += size_read;
        if (front.get_valid_size() < 1)
        {
            m_buffersFree.push_back(front);
            m_buffersToUse.erase(m_buffersToUse.begin());
        }
    }

    return ret;
}

std::size_t ExtendableRingBuffer::write(uint8_t const* buffer, uint32_t size)
{
    uint32_t ret = 0;
    uint32_t size_need_write = size;
    uint32_t size_rest = size;
    uint32_t size_wrote = 0;
    uint8_t const* p_buffer = buffer;

    std::lock_guard<std::recursive_timed_mutex> locker(m_mutex);

    if(!m_buffersToUse.empty())
    {
        DetailBuffer& back = m_buffersToUse.back();
        size_wrote = back.write_new_buffer(p_buffer, size_rest);
        p_buffer += size_wrote;
        size_rest -= size_wrote;
        ret += size_wrote;
    }

    while (size_rest > 0)
    {
        DetailBuffer new_buffer = get_buffer(size_rest);
        size_wrote = new_buffer.write_new_buffer(p_buffer, size_rest);
        p_buffer += size_wrote;
        size_rest -= size_wrote;
        ret += size_wrote;

        m_buffersToUse.push_back(new_buffer);
    }

    return ret;
}

std::size_t ExtendableRingBuffer::size()
{
    std::size_t ret = 0;
    std::lock_guard<std::recursive_timed_mutex> locker(m_mutex);
    for (auto& ele : m_buffersToUse)
    {
        ret += ele.get_valid_size();
    }

    return ret;
}

std::size_t ExtendableRingBuffer::capacity()
{
    std::size_t ret = 0;

    std::lock_guard<std::recursive_timed_mutex> locker(m_mutex);
    for (auto& ele : m_buffersFree)
    {
        ret += ele.capacity();
    }

    for (auto& ele : m_buffersToUse)
    {
        ret += ele.capacity();
    }

    return ret;
}

std::vector<uint8_t> ExtendableRingBuffer::peer_all()
{
    std::vector<uint8_t> ret;
    std::lock_guard<std::recursive_timed_mutex> locker(m_mutex);

    for (auto& ele : m_buffersToUse)
    {
        std::vector<uint8_t> get_one = ele.peer();
        ret.insert(ret.end(), get_one.begin(), get_one.end());
    }
    return ret;
}

DetailBuffer ExtendableRingBuffer::get_buffer(uint32_t size_need)
{
    DetailBuffer new_buffer(0);
    std::lock_guard<std::recursive_timed_mutex> locker(m_mutex);

    if (m_buffersFree.size() > 10)
    {
        std::vector<DetailBuffer> buffersFree(m_buffersFree.begin(), m_buffersFree.begin() + 10);
        m_buffersFree.swap(buffersFree);
    }

    if (!m_buffersFree.empty())
    {
        DetailBuffer new_buffer = m_buffersFree.front();
        m_buffersFree.erase(m_buffersFree.begin());
        new_buffer.clear();
        return new_buffer;
    }

    return DetailBuffer(size_need);
}

