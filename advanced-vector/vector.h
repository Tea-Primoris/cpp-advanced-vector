#pragma once
#include <algorithm>
#include <cassert>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity)),
          capacity_(capacity) {}

    RawMemory(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept
        : buffer_(std::exchange(other.buffer_, nullptr)),
          capacity_(std::exchange(other.capacity_, 0)) {}

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            buffer_ = std::exchange(rhs.buffer_, nullptr);
            capacity_ = std::exchange(rhs.capacity_, 0);
        }
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T *>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T *;
    using const_iterator = const T *;

    Vector() noexcept = default;

    explicit Vector(const size_t size) : data_(size), size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other) : data_(other.size_), size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_)),
          size_(std::exchange(other.size_, 0)) {}

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                RawMemory<T> new_data(rhs.size_);
                std::uninitialized_copy_n(rhs.data_.GetAddress(), rhs.size_, new_data.GetAddress());
                std::destroy_n(data_.GetAddress(), size_);
                data_ = std::move(new_data);
            }
            else {
                std::copy_n(rhs.data_.GetAddress(), std::min(size_, rhs.size_), data_.GetAddress());
                if (rhs.size_ < size_) {
                    std::ranges::destroy(data_.GetAddress() + rhs.size_, data_.GetAddress() + Capacity());
                }
                else if (rhs.size_ > size_) {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_,
                                              rhs.size_ - size_,
                                              data_.GetAddress() + size_);
                }
            }
            size_ = rhs.size_;
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
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
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    [[nodiscard]]
    size_t Size() const noexcept {
        return size_;
    }

    [[nodiscard]]
    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(const size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_ = std::move(new_data);
    }

    void Swap(Vector& other) {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        else {
            if (new_size < Capacity()) {
                std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            }
            else {
                Reserve(new_size);
                std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            }
        }
        size_ = new_size;
    }

    void PushBack(T&& value) {
        if (Capacity() == size_) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data + size_) T(std::move(value));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);
        }
        else {
            new(data_ + size_) T(std::move(value));
        }
        ++size_;
    }

    void PushBack(const T& value) {
        if (Capacity() == size_) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data + size_) T(value);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);
        }
        else {
            new(data_ + size_) T(value);
        }
        ++size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (Capacity() == size_) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);
        }
        else {
            new(data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_ - 1];
    }

    void PopBack() noexcept {
        if (size_ == 0) {
            return;
        }
        data_[size_ - 1].~T();
        --size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert((pos >= cbegin()) && (pos <= cend()));
        size_t index = std::distance(cbegin(), pos);
        if (Capacity() == size_) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data + index) T(std::forward<Args>(args)...);
            MoveOrCopyToMemory(begin(), index, new_data.GetAddress());
            MoveOrCopyToMemory(begin() + index, size_ - index, new_data.GetAddress() + index + 1);
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);
        }
        else {
            if (pos == cend()) {
                new(data_ + size_) T(std::forward<Args>(args)...);
            }
            else {
                T new_item(std::forward<Args>(args)...);
                new(data_ + size_) T(std::move(data_[size_ - 1]));
                std::move_backward(begin() + index, end() - 1, end());
                data_[index] = std::move(new_item);
            }
        }
        ++size_;
        return begin() + index;
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        size_t index = std::distance(cbegin(), pos);
        std::move(begin() + index + 1, end(), begin() + index);
        PopBack();
        return begin() + index;
    }

    iterator Insert(const_iterator pos, const T& value) {
        assert((pos >= cbegin()) && (pos <= cend()));
        size_t index = std::distance(cbegin(), pos);
        if (Capacity() == size_) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data + index) T(value);
            MoveOrCopyToMemory(begin(), index, new_data.GetAddress());
            MoveOrCopyToMemory(begin() + index, size_ - index, new_data.GetAddress() + index + 1);
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);
        }
        else {
            T new_item(value);
            new(data_ + size_) T(std::move(data_[size_ - 1]));
            std::move_backward(begin() + index, end() - 1, end());
            data_[index] = std::move(new_item);
        }
        ++size_;
        return begin() + index;
    }

    iterator Insert(const_iterator pos, T&& value) {
        assert((pos >= cbegin()) && (pos <= cend()));
        size_t index = std::distance(cbegin(), pos);
        if (Capacity() == size_) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data + index) T(std::move(value));
            MoveOrCopyToMemory(begin(), index, new_data.GetAddress());
            MoveOrCopyToMemory(begin() + index, size_ - index, new_data.GetAddress() + index + 1);
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);
        }
        else {
            T new_item(std::move(value));
            new(data_ + size_) T(std::move(data_[size_ - 1]));
            std::move_backward(begin() + index, end() - 1, end());
            data_[index] = std::move(new_item);
        }
        ++size_;
        return begin() + index;
    }

private:
    RawMemory<T> data_ = RawMemory<T>(0);
    size_t size_ = 0;

    void MoveOrCopyToMemory(iterator from, size_t n, iterator to) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(from, n, to);
        }
        else {
            std::uninitialized_copy_n(from, n, to);
        }
    }
};
