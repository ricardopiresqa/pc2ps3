#include "pc2ps3/fidelity_trace.hpp"

namespace pc2ps3 {

FidelityTrace::FidelityTrace(FidelitySample* storage, std::size_t capacity)
    : storage_(storage), capacity_(capacity), size_(0), last_tick_(0) {}

bool FidelityTrace::valid() const {
    return storage_ != 0 && capacity_ > 0 && capacity_ <= kMaxCapacity;
}

bool FidelityTrace::append(const FidelitySample& sample) {
    if (!valid() || size_ >= capacity_ ||
        (size_ > 0 && sample.tick <= last_tick_)) {
        return false;
    }

    storage_[size_] = sample;
    last_tick_ = sample.tick;
    ++size_;
    return true;
}

void FidelityTrace::clear() {
    size_ = 0;
    last_tick_ = 0;
}

std::size_t FidelityTrace::size() const {
    return size_;
}

std::size_t FidelityTrace::capacity() const {
    return capacity_;
}

const FidelitySample* FidelityTrace::sample_at(std::size_t index) const {
    if (!valid() || index >= size_) {
        return 0;
    }
    return &storage_[index];
}

} // namespace pc2ps3
