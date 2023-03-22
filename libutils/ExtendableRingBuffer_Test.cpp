
#include <iostream>
#include <atomic>
#include <random>
#include <thread>
#include <chrono>
#include <vector>

#include <utils/ExtendableRingBuffer.h>

ExtendableRingBuffer s_buffer;
bool running = true;

void read_foo()
{
    std::vector<uint8_t> buffer;
    while (running)
    {
        std::srand(std::chrono::high_resolution_clock::now()
            .time_since_epoch().count());
        uint32_t cnt = std::rand() % 15;
        buffer.resize(cnt);
        uint32_t read_size = 0;
        read_size = s_buffer.read(buffer.data(), cnt);

        if (read_size > 0)
        {
            uint8_t aim = buffer[0];
            for (int i = 1; i < read_size; ++i)
            {
                uint8_t current = buffer[i];
                if (current == '\0')
                {
                    aim = current;
                    continue;
                }

                if (current - aim != 1)
                {
                    std::cout << "Wrong.\n";
                    std::abort();
                    break;
                }

                aim = current;
            }
        }

        std::srand(std::chrono::high_resolution_clock::now()
            .time_since_epoch().count());
        cnt = std::rand() % 15;
        std::this_thread::sleep_for(std::chrono::milliseconds(cnt));
    }
}

int main()
{

    std::thread th(read_foo);
    std::thread th2(read_foo);

    constexpr uint32_t s_test_times = 10000;
    std::vector<uint8_t> buffer;
    uint8_t incur = 0;
    uint32_t wrote_size = 0;
    uint32_t cnt = 0;
    for (int i = 0; i < s_test_times; ++i)
    {
        std::srand(std::chrono::high_resolution_clock::now()
            .time_since_epoch().count());
        cnt = std::rand() % 15;
        for (int k = 0; k < cnt; ++k)
        {
            buffer.push_back(incur++);
        }

        wrote_size = s_buffer.write(buffer.data(), cnt);
        buffer.clear();

        if (wrote_size != cnt)
        {
            std::cout << "Wrong.\n";
            std::abort();
        }

        cnt = std::rand() % 30;
        std::this_thread::sleep_for(std::chrono::milliseconds(cnt));
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
    running = false;
    th.join();
    th2.join();

    std::cout << "Testing OK.\n";
}

