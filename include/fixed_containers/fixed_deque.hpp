#pragma once

#include "fixed_containers/algorithm.hpp"
#include "fixed_containers/consteval_compare.hpp"
#include "fixed_containers/integer_range.hpp"
#include "fixed_containers/iterator_utils.hpp"
#include "fixed_containers/optional_storage.hpp"
#include "fixed_containers/preconditions.hpp"
#include "fixed_containers/random_access_iterator_transformer.hpp"
#include "fixed_containers/source_location.hpp"
#include "fixed_containers/string_literal.hpp"
#include "fixed_containers/type_name.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>

namespace fixed_containers::fixed_deque_customize
{
template <class T>
concept FixedDequeChecking = requires(std::size_t i,
                                      std::size_t s,
                                      const StringLiteral& error_message,
                                      const std_transition::source_location& loc) {
    T::out_of_range(i, s, loc);  // ~ std::out_of_range
    T::length_error(s, loc);     // ~ std::length_error
    T::empty_container_access(loc);
    T::invalid_argument(error_message, loc);  // ~ std::invalid_argument
};

template <typename T, std::size_t /*MAXIMUM_SIZE*/>
struct AbortChecking
{
    static constexpr auto TYPE_NAME = fixed_containers::type_name<T>();

    [[noreturn]] static constexpr void out_of_range(const std::size_t /*index*/,
                                                    const std::size_t /*size*/,
                                                    const std_transition::source_location& /*loc*/)
    {
        std::abort();
    }

    [[noreturn]] static void length_error(const std::size_t /*target_capacity*/,
                                          const std_transition::source_location& /*loc*/)
    {
        std::abort();
    }

    [[noreturn]] static constexpr void empty_container_access(
        const std_transition::source_location& /*loc*/)
    {
        std::abort();
    }

    [[noreturn]] static constexpr void invalid_argument(
        const fixed_containers::StringLiteral& /*error_message*/,
        const std_transition::source_location& /*loc*/)
    {
        std::abort();
    }
};
}  // namespace fixed_containers::fixed_deque_customize

namespace fixed_containers
{
template <typename T,
          std::size_t MAXIMUM_SIZE,
          fixed_deque_customize::FixedDequeChecking CheckingType =
              fixed_deque_customize::AbortChecking<T, MAXIMUM_SIZE>>
class FixedDeque
{
    using OptionalT = optional_storage_detail::OptionalStorage<T>;
    static_assert(IsNotReference<T>, "References are not allowed");
    static_assert(consteval_compare::equal<sizeof(OptionalT), sizeof(T)>);
    using Checking = CheckingType;

    struct Mapper
    {
        constexpr T& operator()(OptionalT& opt_storage) const noexcept { return opt_storage.value; }
        constexpr const T& operator()(const OptionalT& opt_storage) const noexcept
        {
            return opt_storage.value;
        }
    };

    template <IteratorConstness CONSTNESS>
    using IteratorImpl = RandomAccessIteratorTransformer<
        typename std::array<OptionalT, MAXIMUM_SIZE>::const_iterator,
        typename std::array<OptionalT, MAXIMUM_SIZE>::iterator,
        Mapper,
        Mapper,
        CONSTNESS>;

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using const_iterator = IteratorImpl<IteratorConstness::CONSTANT_ITERATOR>;
    using iterator = IteratorImpl<IteratorConstness::MUTABLE_ITERATOR>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    static constexpr void check_target_size(size_type target_size,
                                            const std_transition::source_location& loc)
    {
        if (preconditions::test(target_size <= MAXIMUM_SIZE))
        {
            Checking::length_error(target_size, loc);
        }
    }

public:
    std::array<OptionalT, MAXIMUM_SIZE> IMPLEMENTATION_DETAIL_DO_NOT_USE_array_;
    std::size_t IMPLEMENTATION_DETAIL_DO_NOT_USE_index_i_;
    std::size_t IMPLEMENTATION_DETAIL_DO_NOT_USE_index_j_;

public:
    constexpr FixedDeque() noexcept
      : IMPLEMENTATION_DETAIL_DO_NOT_USE_index_i_(0)
      , IMPLEMENTATION_DETAIL_DO_NOT_USE_index_j_(0)
    // Don't initialize the array
    {
    }

    constexpr FixedDeque(std::initializer_list<T> list,
                         const std_transition::source_location& loc =
                             std_transition::source_location::current()) noexcept
      : FixedDeque(list.begin(), list.end(), loc)
    {
    }

    template <InputIterator InputIt>
    constexpr FixedDeque(InputIt first,
                         InputIt last,
                         const std_transition::source_location& loc =
                             std_transition::source_location::current()) noexcept
      : FixedDeque()
    {
        insert(cend(), first, last, loc);
    }

    constexpr void resize(
        size_type count,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        this->resize(count, T{}, loc);
    }
    constexpr void resize(
        size_type count,
        const value_type& v,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_target_size(count, loc);

        // Reinitialize the new members if we are enlarging
        while (size() < count)
        {
            place_at(end_index(), v);
            increment_size();
        }
        // Destroy extras if we are making it smaller.
        while (size() > count)
        {
            decrement_size();
            destroy_at(end_index());
        }
    }

    constexpr void push_back(
        const value_type& v,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_not_full(loc);
        this->push_back_internal(v);
    }
    constexpr void push_back(
        value_type&& v,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_not_full(loc);
        this->push_back_internal(std::move(v));
    }

    template <InputIterator InputIt>
    constexpr iterator insert(
        const_iterator it,
        InputIt first,
        InputIt last,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        return insert_internal(
            typename std::iterator_traits<InputIt>::iterator_category{}, it, first, last, loc);
    }
    constexpr iterator insert(
        const_iterator it,
        std::initializer_list<T> ilist,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        return insert_internal(
            std::random_access_iterator_tag{}, it, ilist.begin(), ilist.end(), loc);
    }

    constexpr iterator erase(const_iterator first,
                             const_iterator last,
                             const std_transition::source_location& loc =
                                 std_transition::source_location::current()) noexcept
    {
        if (preconditions::test(first <= last))
        {
            Checking::invalid_argument("first > last, range is invalid", loc);
        }
        if (preconditions::test(first >= cbegin() && last <= cend()))
        {
            Checking::invalid_argument("iterators exceed container range", loc);
        }

        const std::size_t read_start = this->index_of(last);
        const std::size_t write_start = this->index_of(first);

        const auto entry_count_to_move = static_cast<std::size_t>(cend() - last);
        const auto entry_count_to_remove = static_cast<std::size_t>(last - first);

        // Clean out the gap
        destroy_range({.start = write_start, .distance = entry_count_to_remove});

        auto read_start_it = create_iterator(read_start);
        auto read_end_it = create_iterator(read_start + entry_count_to_move);
        auto write_start_it = create_iterator(write_start);

        // Do the move
        std::move(read_start_it, read_end_it, write_start_it);

        decrement_size(entry_count_to_remove);
        return create_iterator(write_start);
    }
    constexpr iterator erase(const_iterator it,
                             const std_transition::source_location& loc =
                                 std_transition::source_location::current()) noexcept
    {
        return erase(it, it + 1, loc);
    }

    constexpr iterator begin() noexcept { return create_iterator(front_index()); }
    constexpr const_iterator begin() const noexcept { return cbegin(); }
    constexpr const_iterator cbegin() const noexcept
    {
        return create_const_iterator(front_index());
    }
    constexpr iterator end() noexcept { return create_iterator(end_index()); }
    constexpr const_iterator end() const noexcept { return cend(); }
    constexpr const_iterator cend() const noexcept { return create_const_iterator(end_index()); }

    constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    constexpr const_reverse_iterator rbegin() const noexcept { return crbegin(); }
    constexpr const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }
    constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    constexpr const_reverse_iterator rend() const noexcept { return crend(); }
    constexpr const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }

    [[nodiscard]] constexpr std::size_t max_size() const noexcept { return MAXIMUM_SIZE; }
    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_index_j_ -
               IMPLEMENTATION_DETAIL_DO_NOT_USE_index_i_;
    }
    [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

    template <std::size_t MAXIMUM_SIZE_2, fixed_deque_customize::FixedDequeChecking CheckingType2>
    constexpr bool operator==(const FixedDeque<T, MAXIMUM_SIZE_2, CheckingType2>& other) const
    {
        if constexpr (MAXIMUM_SIZE == MAXIMUM_SIZE_2)
        {
            if (this == &other)
            {
                return true;
            }
        }

        if (this->size() != other.size())
        {
            return false;
        }

        for (std::size_t i = 0; i < this->size(); i++)
        {
            if (this->unchecked_at(i) != other.at(i))
            {
                return false;
            }
        }

        return true;
    }

    template <std::size_t MAXIMUM_SIZE_2, fixed_deque_customize::FixedDequeChecking CheckingType2>
    constexpr auto operator<=>(const FixedDeque<T, MAXIMUM_SIZE_2, CheckingType2>& other) const
    {
        using OrderingType = decltype(std::declval<T>() <=> std::declval<T>());
        const std::size_t min_size = (std::min)(this->size(), other.size());
        for (std::size_t i = 0; i < min_size; i++)
        {
            if (unchecked_at(i) < other.at(i))
            {
                return OrderingType::less;
            }
            if (unchecked_at(i) > other.at(i))
            {
                return OrderingType::greater;
            }
        }

        return this->size() <=> other.size();
    }

    constexpr reference operator[](size_type i) noexcept
    {
        // Cannot capture real source_location for operator[]
        // This operator should not range-check according to the spec, but we want the extra safety.
        return at(i, std_transition::source_location::current());
    }
    constexpr const_reference operator[](size_type i) const noexcept
    {
        // Cannot capture real source_location for operator[]
        // This operator should not range-check according to the spec, but we want the extra safety.
        return at(i, std_transition::source_location::current());
    }

    constexpr reference at(size_type i,
                           const std_transition::source_location& loc =
                               std_transition::source_location::current()) noexcept
    {
        if (preconditions::test(i < size()))
        {
            Checking::out_of_range(i, size(), loc);
        }
        return unchecked_at(i);
    }
    constexpr const_reference at(size_type i,
                                 const std_transition::source_location& loc =
                                     std_transition::source_location::current()) const noexcept
    {
        if (preconditions::test(i < size()))
        {
            Checking::out_of_range(i, size(), loc);
        }
        return unchecked_at(i);
    }

    constexpr reference front(
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_not_empty(loc);
        return unchecked_at(front_index());
    }
    constexpr const_reference front(const std_transition::source_location& loc =
                                        std_transition::source_location::current()) const
    {
        check_not_empty(loc);
        return unchecked_at(front_index());
    }
    constexpr reference back(
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_not_empty(loc);
        return unchecked_at(back_index());
    }
    constexpr const_reference back(const std_transition::source_location& loc =
                                       std_transition::source_location::current()) const
    {
        check_not_empty(loc);
        return unchecked_at(back_index());
    }

private:
    /*
     * Helper for insert
     * Moves everything ahead of a given const_iterator n spots forward, and
     * returns the index to insert something at that place. Increments size_.
     */
    constexpr std::size_t advance_all_after_iterator_by_n(const const_iterator it,
                                                          const std::size_t n)
    {
        const std::size_t read_start = index_of(it);
        const std::size_t write_start = read_start + n;
        const std::size_t value_count_to_move = size() - read_start;

        auto read_start_it = create_iterator(read_start);
        auto read_end_it = create_iterator(read_start + value_count_to_move);
        auto write_end_it = create_iterator(write_start + value_count_to_move);
        algorithm::emplace_move_backward(read_start_it, read_end_it, write_end_it);

        increment_size(n);

        return read_start;
    }

    constexpr void push_back_internal(const value_type& v)
    {
        place_at(end_index(), v);
        increment_size();
    }
    constexpr void push_back_internal(value_type&& v)
    {
        place_at(end_index(), std::move(v));
        increment_size();
    }

    template <InputIterator InputIt>
    constexpr iterator insert_internal(std::forward_iterator_tag,
                                       const_iterator it,
                                       InputIt first,
                                       InputIt last,
                                       const std_transition::source_location& loc)
    {
        const auto entry_count_to_add = static_cast<std::size_t>(std::distance(first, last));
        check_target_size(size() + entry_count_to_add, loc);
        const std::size_t write_index =
            this->advance_all_after_iterator_by_n(it, entry_count_to_add);

        auto write_it = create_iterator(write_index);
        for (auto w_it = write_it; first != last; std::advance(first, 1), std::advance(w_it, 1))
        {
            std::construct_at(&*w_it, *first);
        }
        return write_it;
    }

    template <InputIterator InputIt>
    constexpr iterator insert_internal(std::input_iterator_tag,
                                       const_iterator it,
                                       InputIt first,
                                       InputIt last,
                                       const std_transition::source_location& loc)
    {
        const std::size_t original_size = size();

        // Place everything at the end of the vector
        for (; first != last && size() < max_size(); ++first)
        {
            push_back_internal(*first);
        }

        if (first != last)  // Reached capacity
        {
            std::size_t excess_element_count = 0;
            for (; first != last; ++first)
            {
                excess_element_count++;
            }

            Checking::length_error(MAXIMUM_SIZE + excess_element_count, loc);
        }

        // Rotate into the correct places
        const std::size_t write_index = this->index_of(it);
        auto write_it = create_iterator(write_index);
        std::rotate({write_it}, create_iterator(original_size), create_iterator(size()));

        return write_it;
    }

    constexpr iterator create_iterator(const std::size_t start_index) noexcept
    {
        auto array_it = std::next(std::begin(IMPLEMENTATION_DETAIL_DO_NOT_USE_array_),
                                  static_cast<difference_type>(start_index));
        return iterator{array_it, Mapper{}};
    }

    constexpr const_iterator create_const_iterator(const std::size_t start_index) const noexcept
    {
        auto array_it = std::next(std::begin(IMPLEMENTATION_DETAIL_DO_NOT_USE_array_),
                                  static_cast<difference_type>(start_index));
        return const_iterator{array_it, Mapper{}};
    }

private:
    constexpr std::size_t index_of(const_iterator it) const
    {
        return static_cast<std::size_t>(std::distance(cbegin(), it));
    }

    constexpr void check_not_full(const std_transition::source_location& loc) const
    {
        if (preconditions::test(size() < MAXIMUM_SIZE))
        {
            Checking::length_error(MAXIMUM_SIZE + 1, loc);
        }
    }
    constexpr void check_not_empty(const std_transition::source_location& loc) const
    {
        if (preconditions::test(!empty()))
        {
            Checking::empty_container_access(loc);
        }
    }

    // [WORKAROUND-1] - Needed by the non-trivially-copyable flavor of FixedDeque
protected:
    [[nodiscard]] constexpr std::size_t front_index() const { return 0; }
    [[nodiscard]] constexpr std::size_t back_index() const { return end_index() - 1; }
    [[nodiscard]] constexpr std::size_t end_index() const { return size(); }

    constexpr void increment_size(const std::size_t n = 1)
    {
        IMPLEMENTATION_DETAIL_DO_NOT_USE_index_j_ =
            (IMPLEMENTATION_DETAIL_DO_NOT_USE_index_j_ + n) % (MAXIMUM_SIZE + 1);
    }
    constexpr void decrement_size(const std::size_t n = 1)
    {
        IMPLEMENTATION_DETAIL_DO_NOT_USE_index_j_ =
            (IMPLEMENTATION_DETAIL_DO_NOT_USE_index_j_ - n) % (MAXIMUM_SIZE + 1);
    }

    constexpr const OptionalT& array_unchecked_at(const std::size_t i) const
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_array_[i];
    }
    constexpr OptionalT& array_unchecked_at(const std::size_t i)
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_array_[i];
    }
    constexpr const T& unchecked_at(const std::size_t i) const
    {
        return optional_storage_detail::get(array_unchecked_at(i));
    }
    constexpr T& unchecked_at(const std::size_t i)
    {
        return optional_storage_detail::get(array_unchecked_at(i));
    }

    constexpr void destroy_at(std::size_t)
        requires TriviallyDestructible<T>
    {
    }
    constexpr void destroy_at(std::size_t i)
        requires NotTriviallyDestructible<T>
    {
        std::destroy_at(&array_unchecked_at(i).value);
    }

    constexpr void destroy_range(const StartingIntegerAndDistance&)
        requires TriviallyDestructible<T>
    {
    }
    constexpr void destroy_range(const StartingIntegerAndDistance& start_and_distance)
        requires NotTriviallyDestructible<T>
    {
        auto start = create_iterator({.start = start_and_distance.start, .distance = 0});
        auto end = create_iterator(start_and_distance);

        for (auto it = start; it != end; ++it)
        {
            std::destroy_at(&*it);
        }
    }

    constexpr void place_at(const std::size_t i, const value_type& v)
    {
        std::construct_at(&array_unchecked_at(i), v);
    }
    constexpr void place_at(const std::size_t i, value_type&& v)
    {
        std::construct_at(&array_unchecked_at(i), std::move(v));
    }
};

}  // namespace fixed_containers