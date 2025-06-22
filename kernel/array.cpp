#include <avr/io.h>

extern "C" {
#include <kernel/kernel.h>
#include <kernel/array.hpp>
}

array::array() : mSize(0), mCapacity(0), mAddress(0), mDummyIndex(-1), mDummy(0) {}

array::array(uint32_t size) : mSize(size), mCapacity(size), mAddress(new(size)), mDummyIndex(-1), mDummy(0) {}

array::array(const array& arr) : mSize(arr.mSize), mCapacity(arr.mCapacity), mAddress(new(arr.mCapacity)), mDummyIndex(-1), mDummy(0) {
    uint8_t buffer[mSize];
    VirtualRead(arr.mAddress, buffer, mSize);
    VirtualWrite(mAddress, buffer, mSize);
}

array::~array() {
    if (mAddress > 0) 
        delete(mAddress);
}

uint8_t& array::operator[](unsigned int const i) {
    if (mDummyIndex != i) {
        updateBuffer(mDummyIndex, mDummy);
    }
    mDummyIndex = i;
    mDummy = readBuffer(i);
    return mDummy;
}

const uint8_t& array::operator[](unsigned int const i) const {
    mDummy = readBuffer(i);
    return mDummy;
}

void array::set(uint32_t i, uint8_t item) {
    if (i < mSize) {
        updateBuffer(i, item);
    }
}

uint8_t array::get(uint32_t i) {
    if (i < mSize) {
        return readBuffer(i);
    }
    return 0;
}

bool array::empty(void) {
    return mSize == 0;
}

uint32_t array::size(void) {
    return mSize;
}

uint32_t array::max_size(void) {
    return mCapacity;
}

void array::resize(uint32_t newSize) {
    reserve(newSize);
    mSize = newSize;
}

void array::reserve(uint32_t newCapacity) {
    uint8_t buffer[mCapacity];
    uint32_t newAddress = new(newCapacity);
    VirtualRead(mAddress, buffer, mCapacity);
    VirtualWrite(newAddress, buffer, mCapacity);
    delete(mAddress);
    mAddress = newAddress;
    mCapacity = newCapacity;
}

void array::push_back(uint8_t item) {
    if (mSize >= mCapacity) {
        if (mCapacity == 0) 
            mCapacity = 1;
        reserve(mCapacity * 2);
    }
    updateBuffer(mSize, item);
    mSize++;
}

void array::updateBuffer(uint32_t index, uint8_t value) {
    uint8_t buffer[mSize];
    VirtualRead(mAddress, buffer, mSize);
    buffer[index] = value;
    VirtualWrite(mAddress, buffer, mSize);
}

uint8_t array::readBuffer(uint32_t index) const {
    uint8_t buffer[mSize];
    VirtualRead(mAddress, buffer, mSize);
    return buffer[index];
}
