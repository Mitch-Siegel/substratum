
#ifndef _SYSTEM_HPP_
#define _SYSTEM_HPP_
#include "memory.hpp"
#include "core.hpp"

#define CORE_COUNT 1
#define MEMORY_SIZE 0x4000

class System
{
    friend class SystemMemory;

private:
    std::unique_ptr<Core> cores[CORE_COUNT];

    bool running_;

public:
    System();

    void Start();
    
    void Stop();

    bool Running();

    void Tick();

    SystemMemory memory;

    void Output(uint8_t port, uint32_t val);

    const Core &GetCore(uint8_t index) { return *(cores[index]); };
};

extern System hardware;

#endif
