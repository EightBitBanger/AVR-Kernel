#include <avr/io.h>

extern "C" {
#include <kernel/kernel.h>
#include <kernel/string.hpp>
}

string::string() : mSize(0), mCapacity(0), mAddress(0), mDummyIndex(-1), mDummy(0) {}

string::string(uint32_t size) : mSize(size), mCapacity(size), mAddress(new(size + 1)), mDummyIndex(-1), mDummy(0) {
    uint8_t temp[mSize + 1];
    for (uint8_t i = 0; i < mSize + 1; i++) 
        temp[i] = '\0';
    VirtualWrite(mAddress, temp, mSize + 1);
}

string::string(const string& str) : mSize(str.mSize), mCapacity(str.mCapacity), mAddress(new(str.mCapacity + 1)), mDummyIndex(-1), mDummy(0) {
    uint8_t buffer[mSize + 1];
    VirtualRead(str.mAddress, buffer, mSize + 1);
    VirtualWrite(mAddress, buffer, mSize + 1);
}

string::string(const char* cstr) : mSize(strlen(cstr)), mCapacity(mSize), mAddress(new(mSize + 1)), mDummyIndex(-1), mDummy(0) {
    uint8_t temp[mSize + 1];
    for (uint8_t i = 0; i < mSize; i++) 
        temp[i] = cstr[i];
    temp[mSize] = '\0';
    VirtualWrite(mAddress, temp, mSize + 1);
}

string::~string() {
    if (mAddress > 0) 
        delete(mAddress);
}

uint8_t& string::operator[](unsigned int const i) {
    if (mDummyIndex != i) {
        mUpdateBuffer(mDummyIndex, mDummy);
    }
    mDummyIndex = i;
    mDummy = mReadBuffer(i);
    return mDummy;
}

const uint8_t& string::operator[](unsigned int const i) const {
    mDummy = mReadBuffer(i);
    return mDummy;
}

void string::set(uint32_t i, uint8_t item) {
    if (i < mSize) {
        mUpdateBuffer(i, item);
    }
}

uint8_t string::get(uint32_t i) {
    if (i < mSize) {
        return mReadBuffer(i);
    }
    return 0;
}

bool string::empty(void) {
    return mSize == 0;
}

uint32_t string::size(void) const {
    return mSize;
}

uint32_t string::max_size(void) {
    return mCapacity;
}

void string::resize(uint32_t newSize) {
    reserve(newSize);
    mSize = newSize;
}

void string::reserve(uint32_t newCapacity) {
    uint8_t buffer[mCapacity + 1];
    uint32_t newAddress = new(newCapacity + 1);
    VirtualRead(mAddress, buffer, mCapacity + 1);
    VirtualWrite(newAddress, buffer, mCapacity + 1);
    delete(mAddress);
    mAddress = newAddress;
    mCapacity = newCapacity;
}

void string::mUpdateBuffer(uint32_t index, uint8_t value) {
    uint8_t buffer[mSize + 1];
    VirtualRead(mAddress, buffer, mSize + 1);
    buffer[index] = value;
    VirtualWrite(mAddress, buffer, mSize + 1);
}

uint8_t string::mReadBuffer(uint32_t index) const {
    uint8_t buffer[mSize + 1];
    VirtualRead(mAddress, buffer, mSize + 1);
    return buffer[index];
}

// String functionality

int string::find(const char* substr) const {
    uint32_t substrLen = strlen(substr);
    for (uint32_t i = 0; i < mSize - substrLen + 1; ++i) {
        bool match = true;
        for (uint32_t j = 0; j < substrLen; ++j) {
            if (mReadBuffer(i + j) != substr[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return i;
        }
    }
    return -1; // substring not found
}

int string::find(const string& substr) const {
    char temp[substr.size() + 1];
    for (uint32_t i = 0; i < substr.size(); i++) 
        temp[i] = substr[i];
    temp[substr.size()] = '\0';
    return find(temp);
}

void string::replace(const char* target, const char* replacement) {
    uint32_t targetLen = strlen(target);
    uint32_t replacementLen = strlen(replacement);
    int pos = find(target);
    while (pos != -1) {
        if (replacementLen > targetLen) {
            resize(mSize + replacementLen - targetLen);
        }
        for (uint32_t i = 0; i < replacementLen; ++i) {
            set(pos + i, replacement[i]);
        }
        pos = find(target);
    }
}

void string::replace(const string& target, const string& replacement) {
    char tempSrc[target.size() + 1];
    for (uint32_t i = 0; i < target.size(); i++) 
        tempSrc[i] = target[i];
    tempSrc[target.size()] = '\0';
    
    char tempDst[replacement.size() + 1];
    for (uint32_t i = 0; i < replacement.size(); i++) 
        tempDst[i] = replacement[i];
    tempDst[replacement.size()] = '\0';
    
    replace(tempSrc, tempDst);
    return;
}

void string::insert(uint32_t index, uint8_t item) {
    if (index >= mSize) 
        return;
    
    // Resize if necessary
    if (mSize >= mCapacity) 
        reserve(mCapacity + 1); 
    
    mSize++;
    for (uint32_t i = mSize; i > index; --i) {
        set(i, get(i - 1));
    }
    set(index, item);
    return;
}

void string::insert(uint32_t index, const string str) {
    uint32_t strSz = str.size();
    for (uint32_t i = 0; i < strSz; i++) 
        insert(index + i, str[i]);
    return;
}

void string::insert(uint32_t index, const char* str) {
    if (index >= mSize) 
        return; 
    
    uint32_t strLen = strlen(str);
    
    // Resize if necessary
    if (mSize + strLen > mCapacity) 
        reserve(mSize + strLen);
    
    for (uint32_t i = mSize + strLen; i > index + strLen; --i) 
        set(i, get(i - strLen));
    
    for (uint32_t i = 0; i < strLen; ++i) 
        set(index + i, str[i]);
    
    mSize += strLen;
    return;
}

void string::remove(uint32_t index) {
    if (index >= mSize) 
        return;
    
    for (uint32_t i = index; i < mSize - 1; ++i) 
        set(i, get(i + 1));
    
    set(mSize - 1, '\0');
    mSize--;
}

void string::remove(const char* substr) {
    int pos = find(substr);
    uint32_t substrLen = strlen(substr);
    while (pos != -1) {
        for (uint32_t i = pos; i < mSize - substrLen; ++i) {
            set(i, get(i + substrLen));
        }
        mSize -= substrLen;
        pos = find(substr);
    }
    set(mSize, '\0');
}

void string::remove(const string& substr) {
    uint32_t strSz = substr.size();
    uint32_t begin = substr.find(substr);
    for (uint32_t i=0; i < strSz; i++) 
        remove(begin + i);
}

