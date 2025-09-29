#pragma once
#include "command.h"

/**
 * @brief Concept to ensure a type behaves like a Command
 * This concept checks if a given type C meets the requirements to be considered
 * a Command. It verifies the presence of necessary type aliases, inheritance from the
 * Command base class, constructibility from raw byte data, and the existence of an
 * execute() method that returns the expected type.
 * @tparam C The type to be checked
 */
template <typename C>
concept CommandLike = requires(const std::vector<std::uint8_t>& raw, const C& c) {
    // Exposed types (handy for diagnostics)
    typename C::input_message_t;
    typename C::output_message_t;
    typename C::input_format;    // optional: if you keep these typedefs
    typename C::response_format; // optional: if you keep these typedefs

    // Command must derive from the right base (good misuse guard)
    requires std::derived_from<
        C, Command<typename C::input_message_t::message_format_t, typename C::output_message_t::message_format_t>>;
    // Must be constructible from raw bytes (forwarded to ReceivedMessage ctor)
    { C{raw} };
    // Must have an execute() method returning vector of byte vectors
    { c.execute() } -> std::same_as<std::vector<std::vector<std::uint8_t>>>;
};

namespace detail
{
template <typename C> consteval std::uint8_t cmdId() {
    // Pulls the message ID from the input message's format
    return C::input_message_t::message_format_t::ID;
}
// Compile-time unique-ID check across all Commands...
template <typename...> struct UniqueIds : std::true_type
{};
template <typename C, typename... Rest>
struct UniqueIds<C, Rest...> : std::bool_constant<((cmdId<C>() != cmdId<Rest>()) && ...) && UniqueIds<Rest...>::value>
{};
} // namespace detail

/**
 * @brief Class that handles execution of commands based on incoming data
 *
 * This class takes a variadic list of Command-like types and provides a static
 * method to execute the appropriate command based on the ID found in the incoming data.
 * It ensures at compile-time that all command IDs are unique.
 *
 * @tparam Commands Variadic list of Command-like types
 */
template <CommandLike... Commands> class Handler final
{
    static_assert(detail::UniqueIds<Commands...>::value, "Duplicate command IDs registered in Handler");

  public:
    /**
     * @brief Executes the appropriate command based on incoming data
     * @param data Raw byte data containing the command ID and payload
     * @return std::vector<std::vector<std::uint8_t>> Vector of serialized response messages
     * @throws std::runtime_error if the data is empty or the command ID is unknown
     */
    [[nodiscard]] static std::vector<std::vector<std::uint8_t>> execute(const std::vector<std::uint8_t>& data) {
        if (data.empty())
            throw std::runtime_error("Empty message");

        const std::uint8_t id = data.front();
        std::vector<std::vector<std::uint8_t>> out;

        // Short-circuit fold: constructs only the matching command
        const bool matched = ((id == detail::cmdId<Commands>() && (out = Commands{data}.execute(), true)) || ...);

        if (!matched)
            throw std::runtime_error("Unknown command ID");

        return out;
    }
};
