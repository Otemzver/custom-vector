#pragma once

#include <algorithm>
#include <cassert>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t n)
        : buffer_(Allocate(n))
        , capacity_(n) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept
        : buffer_(other.buffer_)
        , capacity_(other.capacity_) {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Deallocate(buffer_);
            buffer_ = rhs.buffer_;
            capacity_ = rhs.capacity_;
            rhs.buffer_ = nullptr;
            rhs.capacity_ = 0;
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    const T& operator[](size_t index) const noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    size_t Capacity() const noexcept {
        return capacity_;
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

private:
    static T* Allocate(size_t n) {
        return n ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

private:
    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(other.size_) {
        other.size_ = 0;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector tmp(rhs);
                Swap(tmp);
            } else {
                if (size_ >= rhs.size_) {
                    std::copy_n(rhs.data_.GetAddress(), rhs.size_, data_.GetAddress());
                    std::destroy_n(data_ + rhs.size_, size_ - rhs.size_);
                } else {
                    std::copy_n(rhs.data_.GetAddress(), size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_,
                                              rhs.size_ - size_,
                                              data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    [[nodiscard]] size_t Size() const noexcept {
        return size_;
    }

    [[nodiscard]] size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    const T& operator[](size_t index) const noexcept {
        assert(index < size_);
        return data_[index];
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        MoveOrCopyToNewBuffer(new_data.GetAddress());
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_ + new_size, size_ - new_size);
            size_ = new_size;
        } else if (new_size > size_) {
            if (new_size > data_.Capacity()) {
                Reserve(new_size);
            }
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
            size_ = new_size;
        }
    }

    void PopBack() noexcept {
        assert(size_ > 0);
        std::destroy_at(data_ + size_ - 1);
        --size_;
    }

    void PushBack(const T& elem) {
        EmplaceBack(elem);
    }

    void PushBack(T&& elem) {
        EmplaceBack(std::move(elem));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        return *Emplace(cend(), std::forward<Args>(args)...);
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= cbegin() && pos <= cend());

        const size_t index = static_cast<size_t>(pos - cbegin());

        if (size_ == data_.Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            T* const new_begin = new_data.GetAddress();
            T* inserted = nullptr;

            try {
                inserted = new (new_begin + index) T(std::forward<Args>(args)...);

                try {
                    UninitializedMoveOrCopyN(data_.GetAddress(), index, new_begin);
                } catch (...) {
                    std::destroy_at(inserted);
                    throw;
                }

                try {
                    UninitializedMoveOrCopyN(data_.GetAddress() + index,
                                             size_ - index,
                                             new_begin + index + 1);
                } catch (...) {
                    std::destroy_n(new_begin, index);
                    std::destroy_at(inserted);
                    throw;
                }
            } catch (...) {
                throw;
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            ++size_;
            return begin() + index;
        }

        if (index == size_) {
            new (data_ + size_) T(std::forward<Args>(args)...);
            ++size_;
            return end() - 1;
        }

        T tmp(std::forward<Args>(args)...);

        new (data_ + size_) T(std::move(data_[size_ - 1]));
        ++size_;

        std::move_backward(begin() + index, end() - 2, end() - 1);
        data_[index] = std::move(tmp);

        return begin() + index;
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        assert(pos >= cbegin() && pos < cend());

        const size_t index = static_cast<size_t>(pos - cbegin());
        std::move(begin() + index + 1, end(), begin() + index);
        std::destroy_at(end() - 1);
        --size_;
        return begin() + index;
    }

private:
    static constexpr bool kUseMove =
        std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>;

    static void UninitializedMoveOrCopyN(T* src, size_t count, T* dst) {
        if constexpr (kUseMove) {
            std::uninitialized_move_n(src, count, dst);
        } else {
            std::uninitialized_copy_n(src, count, dst);
        }
    }

    void MoveOrCopyToNewBuffer(T* dst) {
        UninitializedMoveOrCopyN(data_.GetAddress(), size_, dst);
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};