#pragma once
#include <cassert>
#include <optional>
#include <utility>

/**
 * @brief Tag type to indicate an error result
 */
struct UnexpectT
{
    explicit UnexpectT() = default;
};

inline constexpr UnexpectT UNEXPECT{};

/**
 * @brief Tag type to indicate a successful result
 */
struct ExpectT
{
    explicit ExpectT() = default;
};

inline constexpr ExpectT EXPECT{};

/**
 * @brief A class representing the result of an operation that can succeed or fail
 * @tparam T The type of the successful value
 * @tparam E The type of the error value
 */
template <typename T, typename E> class Result
{
    const bool _is_ok;
    const std::optional<T> _value;
    const std::optional<E> _error;

  public:
    /**
     * @brief Constructor for successful result
     * @param v The value
     */
    Result(ExpectT, T v) noexcept : _is_ok(true), _value(std::move(v)), _error(std::nullopt) {}

    /**
     * @brief Constructor for error result
     * @param e The error
     */
    Result(UnexpectT, E e) noexcept : _is_ok(false), _value(std::nullopt), _error(std::move(e)) {}

    /**
     * @brief Observer
     * @return true if the result is successful, false otherwise
     */
    [[nodiscard]] bool ok() const noexcept { return _is_ok; }

    /**
     * @brief Bool conversion operator
     * @return true if the result is successful, false otherwise
     */
    [[nodiscard]] explicit operator bool() const noexcept { return _is_ok; }

    /**
     * @brief Value access (precondition: ok())
     * @return The successful value
     */
    [[nodiscard]] const T& value() const& {
        assert(_is_ok && "Accessing value() on an error Result");
        return _value.value();
    }

    /**
     * @brief Error access (precondition: !ok())
     * @return The error value
     */
    [[nodiscard]] const E& error() const& {
        assert(!_is_ok && "Accessing value() on an error Result");
        return _error.value();
    }
};

/**
 * @brief Partial specialization for void value type
 * @tparam E The error type
 */
template <typename E> class Result<void, E>
{
    const bool _is_ok;
    const std::optional<E> _error;

  public:
    /**
     * @brief Constructor for successful result
     */
    explicit Result(ExpectT) noexcept : _is_ok(true), _error(std::nullopt) {}

    /**
     * @brief Constructor for error result
     * @param e The error
     */
    Result(UnexpectT, E e) noexcept : _is_ok(false), _error(std::move(e)) {}

    /**
     * @brief Default constructor for successful void result
     */
    Result() : _is_ok(true), _error(std::nullopt) {}

    /**
     * @brief Observer
     * @return true if the result is successful, false otherwise
     */
    [[nodiscard]] bool ok() const noexcept { return _is_ok; }

    /**
     * @brief Bool conversion operator
     * @return true if the result is successful, false otherwise
     */
    [[nodiscard]] explicit operator bool() const noexcept { return _is_ok; }

    /**
     * @brief Value access (precondition: ok())
     * No value to return for void type, just used for symmetry
     */
    void value() const noexcept { assert(_is_ok && "Accessing value() on an error Result"); }

    /**
     * @brief Error access (precondition: !ok())
     * @return The error value
     */
    [[nodiscard]] const E& error() const& {
        assert(!_is_ok && "Accessing value() on an error Result");
        return _error.value();
    }
};

/**
 * @brief Factory method to create a successful Result instance
 * @param value The successful value
 * @return Result instance
 */
template <typename T, typename E>
[[nodiscard]] static Result<T, E> expected(T value) noexcept(std::is_nothrow_move_constructible_v<T>) {
    return Result(EXPECT, std::move(value));
}

/**
 * @brief Factory method to create an error Result instance
 * @param error The error value
 * @return Result instance
 */
template <typename T, typename E>
[[nodiscard]] static Result<T, E> unexpected(E error) noexcept(std::is_nothrow_move_constructible_v<E>) {
    return Result<T, E>(UNEXPECT, std::move(error));
}

/**
 * @brief Factory method to create a successful Result instance for void type
 * @return Result instance
 */
template <typename E> [[nodiscard]] static Result<void, E> expected() noexcept { return Result<void, E>(EXPECT); }

/**
 * @brief Factory method to create an error Result instance for void type
 * @param error The error value
 * @return Result instance
 */
template <typename E>
[[nodiscard]] static Result<void, E> unexpected(E error) noexcept(std::is_nothrow_move_constructible_v<E>) {
    return Result<void, E>(UNEXPECT, error);
}
