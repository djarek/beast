//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_STATIC_BUFFER_HPP
#define BOOST_BEAST_STATIC_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/detail/buffers_pair.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

namespace boost {
namespace beast {

/** A dynamic buffer providing a fixed size, circular buffer.

    A dynamic buffer encapsulates memory storage that may be
    automatically resized as required, where the memory is
    divided into two regions: readable bytes followed by
    writable bytes. These memory regions are internal to
    the dynamic buffer, but direct access to the elements
    is provided to permit them to be efficiently used with
    I/O operations.

    Objects of this type meet the requirements of <em>DynamicBuffer</em>
    and have the following additional properties:

    @li A mutable buffer sequence representing the readable
    bytes is returned by @ref data when `this` is non-const.

    @li Buffer sequences representing the readable and writable
    bytes, returned by @ref data and @ref prepare, may have
    length up to two.

    @li All operations execute in constant time.

    @li Ownership of the underlying storage belongs to the
    derived class.

    @note Variables are usually declared using the template class
    @ref static_buffer; however, to reduce the number of template
    instantiations, objects should be passed `static_buffer_base&`.

    @see static_buffer
*/
class static_buffer_base
#ifndef BOOST_BEAST_DOXYGEN
    : private detail::dynamic_buffer_access
#endif
{
    char* begin_;
    std::size_t in_off_ = 0;
    std::size_t in_size_ = 0;
    std::size_t out_size_ = 0;
    std::size_t capacity_;

    friend class dynamic_storage_buffer<static_buffer_base>;

    static_buffer_base(static_buffer_base const& other) = delete;

    static_buffer_base& operator=(static_buffer_base const&) = delete;

public:
    /** Constructor

        This creates a dynamic buffer using the provided storage area.

        @param p A pointer to valid storage of at least `n` bytes.

        @param size The number of valid bytes pointed to by `p`.
    */
    BOOST_BEAST_DECL
    static_buffer_base(void* p, std::size_t size) noexcept;

    /** Clear the readable and writable bytes to zero.

        This function causes the readable and writable bytes
        to become empty. The capacity is not changed.

        Buffer sequences previously obtained using @ref data or
        @ref prepare become invalid.

        @esafe

        No-throw guarantee.
    */
    BOOST_BEAST_DECL
    void
    clear() noexcept;

    dynamic_storage_buffer<static_buffer_base>
    dynamic_buffer() noexcept
    {
        return make_dynamic_buffer(*this);
    }

    dynamic_storage_buffer<static_buffer_base>
    dynamic_buffer(std::size_t max_size) noexcept
    {
        return make_dynamic_buffer(*this, max_size);
    }

    dynamic_storage_buffer<static_buffer_base>
    operator->() noexcept
    {
        return dynamic_buffer();
    }

    //--------------------------------------------------------------------------

#if BOOST_BEAST_DOXYGEN
    /// The ConstBufferSequence used to represent the readable bytes.
    using const_buffers_type = __implementation_defined__;

    /// The MutableBufferSequence used to represent the readable bytes.
    using mutable_data_type = __implementation_defined__;

    /// The MutableBufferSequence used to represent the writable bytes.
    using mutable_buffers_type = __implementation_defined__;
#else
    using const_buffers_type   = detail::buffers_pair<false>;
    using mutable_data_type    = detail::buffers_pair<true>;
    using mutable_buffers_type = detail::buffers_pair<true>;
#endif

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
        return capacity_;
    }

    /// Return the maximum number of bytes, both readable and writable, that can be held without requiring an allocation.
    std::size_t
    capacity() const noexcept
    {
        return capacity_;
    }

#ifndef BOOST_ASIO_NO_DYNAMIC_BUFFER_V1

    /// Returns a constant buffer sequence representing the readable bytes
    BOOST_BEAST_DECL
    const_buffers_type
    data() const noexcept;
    
    /// Returns a constant buffer sequence representing the readable bytes
    const_buffers_type
    cdata() const noexcept
    {
        return data();
    }

    /// Returns a mutable buffer sequence representing the readable bytes
    BOOST_BEAST_DECL
    mutable_data_type
    data() noexcept;

    /** Returns a mutable buffer sequence representing writable bytes.
    
        Returns a mutable buffer sequence representing the writable
        bytes containing exactly `n` bytes of storage. Memory may be
        reallocated as needed.

        All buffers sequences previously obtained using
        @ref data or @ref prepare are invalidated.

        @param n The desired number of bytes in the returned buffer
        sequence.

        @throws std::length_error if `size() + n` exceeds `max_size()`.

        @esafe

        Strong guarantee.
    */
    BOOST_BEAST_DECL
    mutable_buffers_type
    prepare(std::size_t n);

    /** Append writable bytes to the readable bytes.

        Appends n bytes from the start of the writable bytes to the
        end of the readable bytes. The remainder of the writable bytes
        are discarded. If n is greater than the number of writable
        bytes, all writable bytes are appended to the readable bytes.

        All buffers sequences previously obtained using
        @ref data or @ref prepare are invalidated.

        @param n The number of bytes to append. If this number
        is greater than the number of writable bytes, all
        writable bytes are appended.

        @esafe

        No-throw guarantee.
    */
    BOOST_BEAST_DECL
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
    BOOST_BEAST_DECL
    const_buffers_type
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
    BOOST_BEAST_DECL
    mutable_buffers_type
    data(std::size_t pos, std::size_t n) noexcept;

    /** Extend the underlying memory to accommodate additional bytes.

        @param n The number of additional bytes to extend by.

        @throws `length_error` if `size() + n > max_size()`.
    */
    BOOST_BEAST_DECL
    void
    grow(std::size_t n);

    /** Remove bytes from the end of the underlying memory.

        This removes bytes from the end of the underlying memory. If
        the number of bytes to remove is larger than `size()`, then
        all underlying memory is emptied.

        @param n The number of bytes to remove.
    */
    BOOST_BEAST_DECL
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
    BOOST_BEAST_DECL
    void
    consume(std::size_t n) noexcept;
};

//------------------------------------------------------------------------------

/** A dynamic buffer providing a fixed size, circular buffer.

    A dynamic buffer encapsulates memory storage that may be
    automatically resized as required, where the memory is
    divided into two regions: readable bytes followed by
    writable bytes. These memory regions are internal to
    the dynamic buffer, but direct access to the elements
    is provided to permit them to be efficiently used with
    I/O operations.

    Objects of this type meet the requirements of <em>DynamicBuffer</em>
    and have the following additional properties:

    @li A mutable buffer sequence representing the readable
    bytes is returned by @ref data when `this` is non-const.

    @li Buffer sequences representing the readable and writable
    bytes, returned by @ref data and @ref prepare, may have
    length up to two.

    @li All operations execute in constant time.

    @tparam N The number of bytes in the internal buffer.

    @note To reduce the number of template instantiations when passing
    objects of this type in a deduced context, the signature of the
    receiving function should use @ref static_buffer_base instead.

    @see static_buffer_base
*/
template<std::size_t N>
class static_buffer : public static_buffer_base
{
    char buf_[N];

public:
    /// Constructor
    static_buffer() noexcept
        : static_buffer_base(buf_, N)
    {
    }

    /// Constructor
    static_buffer(static_buffer const&) noexcept;

    /// Assignment
    static_buffer& operator=(static_buffer const&) noexcept;

    /// Returns the @ref static_buffer_base portion of this object
    static_buffer_base&
    base() noexcept
    {
        return *this;
    }

    /// Returns the @ref static_buffer_base portion of this object
    static_buffer_base const&
    base() const noexcept
    {
        return *this;
    }

    /// Return the maximum sum of the input and output sequence sizes.
    std::size_t constexpr
    max_size() const noexcept
    {
        return N;
    }

    /// Return the maximum sum of input and output sizes that can be held without an allocation.
    std::size_t constexpr
    capacity() const noexcept
    {
        return N;
    }
};

} // beast
} // boost

#include <boost/beast/core/impl/static_buffer.hpp>
#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/core/impl/static_buffer.ipp>
#endif

#endif
