#ifndef __ARRAY_ALLOCATOR_
#define __ARRAY_ALLOCATOR_


class array {
    
public:
    
    array();
    array(uint32_t size);
    array(const array& arr);
    ~array();
    
    uint8_t& operator[](unsigned int const i);
    const uint8_t& operator[](unsigned int const i) const;
    
    void set(uint32_t i, uint8_t item);
    uint8_t get(uint32_t i);
    
    bool empty(void);
    uint32_t size(void);
    uint32_t max_size(void);
    void resize(uint32_t newSize);
    void reserve(uint32_t newCapacity);
    void push_back(uint8_t item);
    
private:
    uint32_t mSize;
    uint32_t mCapacity;
    uint32_t mAddress;
    uint32_t mDummyIndex;
    mutable uint8_t mDummy;
    
    void updateBuffer(uint32_t index, uint8_t value);
    uint8_t readBuffer(uint32_t index) const;
};

#endif
