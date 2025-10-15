#pragma once

#include "command.h"
#include <functional>

/* ―――――――――――――――― Concepts ―――――――――――――――― */

/**
 * @brief Concept to ensure a type behaves like a Command
 * Usage: static_assert(CommandLike<T>);
 * @tparam C The type to be checked
 */
template <typename C> concept CommandLike = requires(const std::vector<std::uint8_t>& raw, const C& c) {
    typename C::input_message_t;
    requires std::derived_from<C, Command<typename C::input_message_t::message_format_t>>;
};

/**
 * @brief Namespace containing internal helper templates and functions
 */
namespace command_helpers
{
/**
 * @brief Retrieves the command ID from a Command-like type at compile-time
 * @tparam C The Command-like type
 * @return std::uint8_t The command ID
 */
template <CommandLike C> consteval std::uint8_t cmdId() { return C::input_message_t::message_format_t::ID; }

/**
 * @brief Base case for UniqueIds: no types means all IDs are unique
 * @tparam ...
 */
template <CommandLike...> struct UniqueIds : std::true_type
{};

/**
 * @brief Compile-time check to ensure all command IDs are unique
 * Works by recursively checking that the ID of the first type
 * is different from all subsequent types, and then recursing on the rest.
 * @tparam C The first Command-like type
 * @tparam Rest The remaining Command-like types
 */
template <CommandLike C, CommandLike... Rest> struct UniqueIds<C, Rest...>
    : std::bool_constant<((cmdId<C>() != cmdId<Rest>()) && ...) && UniqueIds<Rest...>::value>
{};
} // namespace command_helpers

/** @brief Type alias for serialized message array format */
using serialized_message_array_t = std::vector<serialized_message_t>;

/* ―――――――――――――――― Classes ―――――――――――――――― */

/**
 * @brief Class that handles execution of commands based on incoming data
 *
 * This class takes a variadic list of Command-like types and provides a static
 * method to execute the appropriate command based on the ID found in the
 * incoming data. It ensures at compile-time that all command IDs are unique.
 *
 * @tparam Commands Variadic list of Command-like types
 */
template <CommandLike... Commands> class Handler final
{
    static_assert(command_helpers::UniqueIds<Commands...>::value, "Duplicate command IDs registered in Handler");

  public:
    /**
     * @brief Enumeration representing the status of command execution
     */
    enum class EXECUTE_STATUS : std::uint8_t
    {
        SUCCESS = 0,
        ERROR_ID_NOT_FOUND = 1,
        ERROR_WRONG_MESSAGE_FORMAT = 2,
        ERROR_COMMAND_FAILED = 3,
    };

    /**
     * @brief Executes the appropriate command based on incoming data
     * @param data Raw byte data containing the command ID and payload
     * @param communicator The Communicator instance to handle responses and requests
     * @return EXECUTE_STATUS The status of the command execution
     * @throws std::runtime_error if the data is empty or the command ID is unknown
     */
    [[nodiscard]] static EXECUTE_STATUS execute(const serialized_message_t& data, const Communicator& communicator) {
        if (data.empty()) {
            return EXECUTE_STATUS::ERROR_WRONG_MESSAGE_FORMAT;
        }

        const std::uint8_t id = data.front();

        // Short-circuit fold: constructs and execute only the matching command
        try {
            const bool matched =
                ((id == command_helpers::cmdId<Commands>() && (Commands{data}.execute(communicator), true)) || ...);
            if (!matched) {
                return EXECUTE_STATUS::ERROR_ID_NOT_FOUND;
            }
        }
        catch (const std::exception& e) {
            // TODO: handle different exception types differently
            return EXECUTE_STATUS::ERROR_WRONG_MESSAGE_FORMAT;
        }
        return EXECUTE_STATUS::SUCCESS;
    }
};
