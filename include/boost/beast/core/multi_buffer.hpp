//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_MULTI_BUFFER_HPP
#define BOOST_BEAST_MULTI_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/detail/allocator.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/core/empty_value.hpp>
#include <boost/intrusive/list.hpp>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>

namespace boost {
namespace beast {

/** A dynamic buffer providing sequences of variable length.

    A dynamic buffer encapsulates memory storage that may be
    automatically resized as required, where the memory is
    divided into two regions: readable bytes followed by
    writable bytes. These memory regions are internal to
    the dynamic buffer, but direct access to the elements
    is provided to permit them to be efficiently used with
    I/O operations.

    The implementation uses a sequence of one or more byte
    arrays of varying sizes to represent the readable and
    writable bytes. Additional byte array objects are
    appended to the sequence to accommodate changes in the
    desired size. The behavior and implementation of this
    container is most similar to `std::deque`.

    Objects of this type meet the requirements of <em>DynamicBuffer</em>
    and have the following additional properties:

    @li A mutable buffer sequence representing the readable
    bytes is returned by @ref data when `this` is non-const.

    @li Buffer sequences representing the readable and writable
    bytes, returned by @ref data and @ref prepare, may have
    length greater than one.

    @li A configurable maximum size may be set upon construction
    and adjusted afterwards. Calls to @ref prepare that would
    exceed this size will throw `std::length_error`.

    @li Sequences previously obtained using @ref data remain
    valid after calls to @ref prepare or @ref commit.

    @tparam Allocator The allocator to use for managing memory.
*/
template<class Allocator>
class basic_multi_buffer
#if ! BOOST_BEAST_DOXYGEN
    : private boost::empty_value<
        typename detail::allocator_traits<Allocator>::
            template rebind_alloc<char>>
#endif
{
    using base_alloc_type = typename
        detail::allocator_traits<Allocator>::
            template rebind_alloc<char>;

    using alloc_traits =
        beast::detail::allocator_traits<base_alloc_type>;

    // Storage for the list of buffers representing the input
    // and output sequences. The allocation for each element
    // contains `element` followed by raw storage bytes.
    class element;

    using list_type = typename boost::intrusive::make_list<element,
        boost::intrusive::constant_time_size<true>>::type;

    using iter = typename list_type::iterator;

    std::size_t max_;
    list_type list_;            // list of allocated buffers
    iter out_;                  // element that contains out_pos_
    std::size_t in_size_ = 0;   // size of the input sequence
    std::size_t in_pos_ = 0;    // input offset in list_.front()
    std::size_t out_pos_ = 0;   // output offset in *out_
    std::size_t out_end_ = 0;   // output end offset in list_.back()

    static bool constexpr default_nothrow =
        std::is_nothrow_default_constructible<Allocator>::value;

    template<bool>
    class readable_bytes;

    using const_iter = typename list_type::const_iterator;

    using pocma = typename
        alloc_traits::propagate_on_container_move_assignment;

    using pocca = typename
        alloc_traits::propagate_on_container_copy_assignment;

    static_assert(std::is_base_of<std::bidirectional_iterator_tag,
        typename std::iterator_traits<iter>::iterator_category>::value,
            "BidirectionalIterator type requirements not met");

    static_assert(std::is_base_of<std::bidirectional_iterator_tag,
        typename std::iterator_traits<const_iter>::iterator_category>::value,
            "BidirectionalIterator type requirements not met");

    template<class>
    friend class detail::dynamic_buffer_adaptor;

public:
    /// The type of allocator used.
    using allocator_type = Allocator;

    /// Destructor
    ~basic_multi_buffer();

    /** Constructor

        After construction, @ref capacity will return zero, and
        @ref max_size will return the largest value which may
        be passed to the allocator's `allocate` function.
    */
    basic_multi_buffer() noexcept(default_nothrow);

    /** Constructor

        After construction, @ref capacity will return zero, and
        @ref max_size will return the specified value of `limit`.

        @param limit The desired maximum size.
    */
    explicit
    basic_multi_buffer(
        std::size_t limit) noexcept(default_nothrow);

    /** Constructor

        After construction, @ref capacity will return zero, and
        @ref max_size will return the largest value which may
        be passed to the allocator's `allocate` function.

        @param alloc The allocator to use for the object.

        @esafe

        No-throw guarantee.
    */
    explicit
    basic_multi_buffer(Allocator const& alloc) noexcept;

    /** Constructor

        After construction, @ref capacity will return zero, and
        @ref max_size will return the specified value of `limit`.

        @param limit The desired maximum size.

        @param alloc The allocator to use for the object.

        @esafe

        No-throw guarantee.
    */
    basic_multi_buffer(
        std::size_t limit, Allocator const& alloc) noexcept;

    /** Move Constructor

        The container is constructed with the contents of `other`
        using move semantics. The maximum size will be the same
        as the moved-from object.

        Buffer sequences previously obtained from `other` using
        @ref data or @ref prepare remain valid after the move.

        @param other The object to move from. After the move, the
        moved-from object will have zero capacity, zero readable
        bytes, and zero writable bytes.

        @esafe

        No-throw guarantee.
    */
    basic_multi_buffer(basic_multi_buffer&& other) noexcept;

    /** Move Constructor

        Using `alloc` as the allocator for the new container, the
        contents of `other` are moved. If `alloc != other.get_allocator()`,
        this results in a copy. The maximum size will be the same
        as the moved-from object.

        Buffer sequences previously obtained from `other` using
        @ref data or @ref prepare become invalid after the move.

        @param other The object to move from. After the move,
        the moved-from object will have zero capacity, zero readable
        bytes, and zero writable bytes.

        @param alloc The allocator to use for the object.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of `alloc`.
    */
    basic_multi_buffer(
        basic_multi_buffer&& other,
        Allocator const& alloc);

    /** Copy Constructor

        This container is constructed with the contents of `other`
        using copy semantics. The maximum size will be the same
        as the copied object.

        @param other The object to copy from.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of the allocator.
    */
    basic_multi_buffer(basic_multi_buffer const& other);

    /** Copy Constructor

        This container is constructed with the contents of `other`
        using copy semantics and the specified allocator. The maximum
        size will be the same as the copied object.

        @param other The object to copy from.

        @param alloc The allocator to use for the object.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of `alloc`.
    */
    basic_multi_buffer(basic_multi_buffer const& other,
        Allocator const& alloc);

    /** Copy Constructor

        This container is constructed with the contents of `other`
        using copy semantics. The maximum size will be the same
        as the copied object.

        @param other The object to copy from.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of the allocator.
    */
    template<class OtherAlloc>
    basic_multi_buffer(basic_multi_buffer<
        OtherAlloc> const& other);

    /** Copy Constructor

        This container is constructed with the contents of `other`
        using copy semantics. The maximum size will be the same
        as the copied object.

        @param other The object to copy from.

        @param alloc The allocator to use for the object.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of `alloc`.
    */
    template<class OtherAlloc>
    basic_multi_buffer(
        basic_multi_buffer<OtherAlloc> const& other,
        allocator_type const& alloc);

    /** Move Assignment

        The container is assigned with the contents of `other`
        using move semantics. The maximum size will be the same
        as the moved-from object.

        Buffer sequences previously obtained from `other` using
        @ref data or @ref prepare remain valid after the move.

        @param other The object to move from. After the move,
        the moved-from object will have zero capacity, zero readable
        bytes, and zero writable bytes.
    */
    basic_multi_buffer&
    operator=(basic_multi_buffer&& other);

    /** Copy Assignment

        The container is assigned with the contents of `other`
        using copy semantics. The maximum size will be the same
        as the copied object.

        After the copy, `this` will have zero writable bytes.

        @param other The object to copy from.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of the allocator.
    */
    basic_multi_buffer& operator=(
        basic_multi_buffer const& other);

    /** Copy Assignment

        The container is assigned with the contents of `other`
        using copy semantics. The maximum size will be the same
        as the copied object.

        After the copy, `this` will have zero writable bytes.

        @param other The object to copy from.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of the allocator.
    */
    template<class OtherAlloc>
    basic_multi_buffer& operator=(
        basic_multi_buffer<OtherAlloc> const& other);

    /// Returns a copy of the allocator used.
    allocator_type
    get_allocator() const
    {
        return this->get();
    }

    /** Set the maximum allowed capacity

        This function changes the currently configured upper limit
        on capacity to the specified value.

        @param n The maximum number of bytes ever allowed for capacity.

        @esafe

        No-throw guarantee.
    */
    void
    max_size(std::size_t n) noexcept
    {
        max_ = n;
    }

    /** Guarantee a minimum capacity

        This function adjusts the internal storage (if necessary)
        to guarantee space for at least `n` bytes.

        Buffer sequences previously obtained using @ref data remain
        valid, while buffer sequences previously obtained using
        @ref prepare become invalid.

        @param n The minimum number of byte for the new capacity.
        If this value is greater than the maximum size, then the
        maximum size will be adjusted upwards to this value.

        @throws std::length_error if n is larger than the maximum
        allocation size of the allocator.

        @esafe

        Strong guarantee.
    */
    void
    reserve(std::size_t n);

    /** Reallocate the buffer to fit the readable bytes exactly.

        Buffer sequences previously obtained using @ref data or
        @ref prepare become invalid.

        @esafe

        Strong guarantee.
    */
    void
    shrink_to_fit();

    /** Set the size of the readable and writable bytes to zero.

        This clears the buffer without changing capacity.
        Buffer sequences previously obtained using @ref data or
        @ref prepare become invalid.

        @esafe

        No-throw guarantee.
    */
    void
    clear() noexcept;

    /// Exchange two dynamic buffers
    template<class Alloc>
    friend
    void
    swap(
        basic_multi_buffer<Alloc>& lhs,
        basic_multi_buffer<Alloc>& rhs) noexcept;

    //--------------------------------------------------------------------------

#if BOOST_BEAST_DOXYGEN
    /// The ConstBufferSequence used to represent the readable bytes.
    using const_buffers_type = __implementation_defined__;

    /// The MutableBufferSequence used to represent the readable bytes.
    using mutable_data_type = __implementation_defined__;

    /// The MutableBufferSequence used to represent the writable bytes.
    using mutable_buffers_type = __implementation_defined__;
#else
    using const_buffers_type = readable_bytes<false>;
    using mutable_data_type = readable_bytes<true>;
    class mutable_buffers_type;
#endif

#if 0
    mutable_buffers_type
    buffer() noexcept
    {
        return {in_, dist(in_, out_)};
    }

    const_buffers_type
    buffer() const noexcept
    {
        return {in_, dist(in_, out_)};
    }

    dynamic_storage_buffer<basic_multi_buffer>
    dynamic_buffer() noexcept
    {
        return make_dynamic_buffer(*this);
    }

    dynamic_storage_buffer<basic_multi_buffer>
    dynamic_buffer(std::size_t max_size) noexcept
    {
        return make_dynamic_buffer(*this, max_size);
    }

    dynamic_storage_buffer<basic_multi_buffer>
    operator->() noexcept
    {
        return dynamic_buffer();
    }
#endif

    //--------------------------------------------------------------------------

    /// Returns the number of readable bytes.
    std::size_t
    size() const noexcept
    {
        return in_size_;
    }

    /// Return the maximum number of bytes, both readable and writable, that can ever be held.
    std::size_t
    max_size() const noexcept
    {
        return max_;
    }

    /// Return the maximum number of bytes, both readable and writable, that can be held without requiring an allocation.
    std::size_t
    capacity() const noexcept;

#ifndef BOOST_ASIO_NO_DYNAMIC_BUFFER_V1

    /** Returns a constant buffer sequence representing the readable bytes

        @note The sequence may contain multiple contiguous memory regions.
    */
    const_buffers_type
    data() const noexcept;

    /** Returns a constant buffer sequence representing the readable bytes

        @note The sequence may contain multiple contiguous memory regions.
    */
    const_buffers_type
    cdata() const noexcept
    {
        return data();
    }

    /** Returns a mutable buffer sequence representing the readable bytes.

        @note The sequence may contain multiple contiguous memory regions.
    */
    mutable_data_type
    data() noexcept;

    /** Returns a mutable buffer sequence representing writable bytes.
    
        Returns a mutable buffer sequence representing the writable
        bytes containing exactly `n` bytes of storage. Memory may be
        reallocated as needed.

        All buffer sequences previously obtained using @ref prepare are
        invalidated. Buffer sequences previously obtained using @ref data
        remain valid.

        @param n The desired number of bytes in the returned buffer
        sequence.

        @throws std::length_error if `size() + n` exceeds `max_size()`.

        @esafe

        Strong guarantee.
    */
    mutable_buffers_type
    prepare(std::size_t n);

    /** Append writable bytes to the readable bytes.

        Appends n bytes from the start of the writable bytes to the
        end of the readable bytes. The remainder of the writable bytes
        are discarded. If n is greater than the number of writable
        bytes, all writable bytes are appended to the readable bytes.

        All buffer sequences previously obtained using @ref prepare are
        invalidated. Buffer sequences previously obtained using @ref data
        remain valid.

        @param n The number of bytes to append. If this number
        is greater than the number of writable bytes, all
        writable bytes are appended.

        @esafe

        No-throw guarantee.
    */
    void
    commit(std::size_t n) noexcept;

#endif

    /** Return a constant buffer sequence representing the underlying memory.

        The returned buffer sequence `u` represents the underlying
        memory beginning at offset `pos` and where `buffer_size(u) <= n`.

        @param pos The offset to start from. If this is larger than
        the size of the underlying memory, an empty buffer sequence
        is returned.

        @param n The maximum number of bytes in the returned sequence,
        starting from `pos`.

        @return The constant buffer sequence
    */
    readable_bytes<false>
    data(std::size_t pos, std::size_t n) const noexcept;

    /** Return a mutable buffer sequence representing the underlying memory.

        The returned buffer sequence `u` represents the underlying
        memory beginning at offset `pos` and where `buffer_size(u) <= n`.

        @param pos The offset to start from. If this is larger than
        the size of the underlying memory, an empty buffer sequence
        is returned.

        @param n The maximum number of bytes in the returned sequence,
        starting from `pos`.

        @return The mutable buffer sequence
    */
    readable_bytes<true>
    data(std::size_t pos, std::size_t n) noexcept;

    /** Extend the underlying memory to accommodate additional bytes.

        @param n The number of additional bytes to extend by.

        @throws `length_error` if `size() + n > max_size()`.
    */
    void
    grow(std::size_t n);

    /** Remove bytes from the end of the underlying memory.

        This removes bytes from the end of the underlying memory. If
        the number of bytes to remove is larger than `size()`, then
        all underlying memory is emptied.

        @param n The number of bytes to remove.
    */
    void
    shrink(std::size_t n);

    /** Remove bytes from beginning of the readable bytes.

        Removes n bytes from the beginning of the readable bytes.

        All buffers sequences previously obtained using
        @ref data or @ref prepare are invalidated.

        @param n The number of bytes to remove. If this number
        is greater than the number of readable bytes, all
        readable bytes are removed.

        @esafe

        No-throw guarantee.
    */
    void
    consume(std::size_t n) noexcept;

private:
    template<class OtherAlloc>
    friend class basic_multi_buffer;

    template<class OtherAlloc>
    void copy_from(basic_multi_buffer<OtherAlloc> const&);
    void move_assign(basic_multi_buffer& other, std::false_type);
    void move_assign(basic_multi_buffer& other, std::true_type) noexcept;
    void copy_assign(basic_multi_buffer const& other, std::false_type);
    void copy_assign(basic_multi_buffer const& other, std::true_type);
    void swap(basic_multi_buffer&) noexcept;
    void swap(basic_multi_buffer&, std::true_type) noexcept;
    void swap(basic_multi_buffer&, std::false_type) noexcept;
    void destroy(list_type& list) noexcept;
    void destroy(const_iter it);
    void destroy(element& e);
    element& alloc(std::size_t size);
    void debug_check() const;
};

/// A typical multi buffer
using multi_buffer = basic_multi_buffer<std::allocator<char>>;

} // beast
} // boost

#include <boost/beast/core/impl/multi_buffer.hpp>

#endif