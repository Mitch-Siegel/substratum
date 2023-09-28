
#ifndef _SYSTEM_HPP_
#define _SYSTEM_HPP_
#include "memory.hpp"
#include "core.hpp"

#define CORE_COUNT 1

class System
{
private:
    std::unique_ptr<Core> cores[CORE_COUNT];

    bool running_;

public:
    System();

    void Start();

    bool Running();

    void Tick();

    SystemMemory memory;

    void Output(uint8_t port, uint32_t val);

    const Core &GetCore(uint8_t index) { return *(cores[index]); };
};

extern System hardware;

#endif
