#pragma once
#include <cstdint>
#include <functional>
#include <vector>

/**
 * @brief Abstract base class for communication interfaces
 *
 * This class defines the interface for communication mechanisms that can
 */
class Communication
{
public:
    virtual ~Communication() = default;

    /**
     * @brief Listens for incoming messages and processes them, sending responses as needed
     */
    virtual void listen() = 0;
};
