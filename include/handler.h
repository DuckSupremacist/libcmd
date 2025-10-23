#pragma once

#include "command.h"
#include "result.h"
#include <memory>

/* ―――――――――――――――― Concepts ―――――――――――――――― */

/**
 * @brief Concept to ensure a type behaves like a Command
 * Usage: static_assert(CommandLike<T>);
 * @tparam C The type to be checked
 */
template <typename C> concept CommandLike =
    // 1) static ID
    requires {
        { C::ID } -> std::convertible_to<std::uint8_t>;
    } &&
    // 2) constructor from raw bytes (nested requirement)
    requires { requires std::constructible_from<C, const std::vector<std::uint8_t>&>; } &&
    // 3) execute(const Communicator&) const -> void (compound requirement)
    requires(const C& c, const Communicator& comm) {
        { c.execute(comm) } -> std::same_as<void>;
    };

/**
 * @brief Namespace containing internal helper templates and functions
 */
namespace command_helpers
{
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
    : std::bool_constant<((C::ID != Rest::ID) && ...) && UniqueIds<Rest...>::value>
{};
} // namespace command_helpers

/* ―――――――――――――――― Classes ―――――――――――――――― */
/**
 * @brief Enumeration representing the status of command execution
 */
enum class HANDLER_EXECUTE_STATUS : std::uint8_t
{
    ERROR_ID_NOT_FOUND = 1,
    ERROR_MESSAGE_LENGTH_ERROR = 2,
    ERROR_EXCEPTION_DURING_EXECUTION = 3,
    ERROR_EMPTY_MESSAGE = 4,
};
/**
 * @brief Structure representing an error that occurred during command execution
 */
struct HandlerExecuteError
{
    /** @brief The status code of the error */
    HANDLER_EXECUTE_STATUS code;
    /** @brief A descriptive message about the error */
    std::string msg;
};

using HandlerExecuteResult = Result<void, HandlerExecuteError>;

/**
 * @brief Class that handles execution of commands based on incoming data
 *
 * This class takes a variadic list of Command-like types and provides a static
 * method to execute the appropriate command based on the ID found in the
 * incoming data. It ensures at compile-time that all command IDs are unique.
 *
 * @tparam HandlerID Unique identifier for this handler
 * @tparam Commands Variadic list of Command-like types
 */
template <std::uint8_t HandlerID, CommandLike... Commands> class Handler final
{
    static_assert(command_helpers::UniqueIds<Commands...>::value, "Duplicate command IDs registered in Handler");

  public:
    static constexpr std::uint8_t ID = HandlerID; ///< Unique identifier for this handler

    /**
     * @brief Executes the appropriate command based on incoming data
     * @param data Raw byte data containing the command ID and payload
     * @param communicator The Communicator instance to handle responses and requests
     * @return HandlerExecuteResult The result of the command execution
     */
    [[nodiscard]] static HandlerExecuteResult
    execute(const std::vector<std::uint8_t>& data, const Communicator& communicator) noexcept {
        if (data.empty()) {
            return unexpected(
                HandlerExecuteError{
                    .code = HANDLER_EXECUTE_STATUS::ERROR_EMPTY_MESSAGE, .msg = "Empty message received"
                }
            );
        }
        const std::uint8_t id = data.front();

        // Short-circuit fold: constructs and execute only the matching command
        try {
            const bool matched =
                ((id == Commands::input_message_t::ID && (Commands{data}.execute(communicator), true)) || ...);
            if (!matched) {
                return unexpected(
                    HandlerExecuteError{
                        .code = HANDLER_EXECUTE_STATUS::ERROR_ID_NOT_FOUND,
                        .msg = "Unknown command ID: " + std::to_string(id)
                    }
                );
            }
        }
        catch (const MessageLengthError& e) {
            return unexpected(
                HandlerExecuteError{.code = HANDLER_EXECUTE_STATUS::ERROR_MESSAGE_LENGTH_ERROR, .msg = e.what()}
            );
        }
        catch (const std::exception& e) {
            return unexpected(
                HandlerExecuteError{.code = HANDLER_EXECUTE_STATUS::ERROR_EXCEPTION_DURING_EXECUTION, .msg = e.what()}
            );
        }
        catch (...) {
            return unexpected(
                HandlerExecuteError{
                    .code = HANDLER_EXECUTE_STATUS::ERROR_EXCEPTION_DURING_EXECUTION, .msg = "Unknown exception"
                }
            );
        }
        return {};
    }
};
