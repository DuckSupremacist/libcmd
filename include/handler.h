#pragma once

#include "command.h"
#include <stdexcept>

/* ―――――――――――――――― Concepts ―――――――――――――――― */

/**
 * @brief Concept to ensure a type behaves like a Command
 * Usage: CommandLike<T>
 * @tparam C The type to be checked
 */
template <typename C> concept CommandLike = requires(const std::vector<std::uint8_t>& raw, const C& c) {
    typename C::input_message_t;
    typename C::output_message_t;
    requires std::derived_from<
        C, Command<typename C::input_message_t::message_format_t, typename C::output_message_t::message_format_t>
    >;
};

/**
 * @brief Namespace containing internal helper templates and functions
 */
namespace command_helpers
{
/** @brief Retrieves the command ID from a Command-like type at compile-time
 * @tparam C The Command-like type
 * @return std::uint8_t The command ID
 */
template <typename C> consteval std::uint8_t cmdId() { return C::input_message_t::message_format_t::ID; }

/**
 * @brief Base case for UniqueIds: no types means all IDs are unique
 * @tparam ...
 */
template <typename...> struct UniqueIds : std::true_type
{};

/**
 * @brief Compile-time check to ensure all command IDs are unique
 * Works by recursively checking that the ID of the first type
 * is different from all subsequent types, and then recursing on the rest.
 * @tparam C The first Command-like type
 * @tparam Rest The remaining Command-like types
 */
template <typename C, typename... Rest> struct UniqueIds<C, Rest...>
    : std::bool_constant<(cmdId<C>() != ... != cmdId<Rest>()) && UniqueIds<Rest...>::value>
{};
} // namespace command_helpers

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
     * @brief Executes the appropriate command based on incoming data
     * @param data Raw byte data containing the command ID and payload
     * @return std::vector<std::vector<std::uint8_t>> Vector of serialized
     * response messages
     * @throws std::runtime_error if the data is empty or the command ID is
     * unknown
     */
    [[nodiscard]] static std::vector<std::vector<std::uint8_t>> execute(const std::vector<std::uint8_t>& data) {
        if (data.empty()) {
            throw std::runtime_error("Empty message");
        }

        const std::uint8_t id = data.front();
        std::vector<std::vector<std::uint8_t>> out;

        // Short-circuit fold: constructs and execute only the matching command
        const bool matched =
            ((id == command_helpers::cmdId<Commands>() && (out = Commands{data}.execute(), true)) || ...);

        if (!matched) {
            throw std::runtime_error("Unknown command ID");
        }

        return out;
    }
};
