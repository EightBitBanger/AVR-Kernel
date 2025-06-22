#ifndef __STRING_ALLOCATOR_
#define __STRING_ALLOCATOR_

class string {
    
public:
    
    string();
    string(uint32_t size);
    string(const string& str);
    string(const char* cstr);
    ~string();
    
    uint8_t& operator[](unsigned int const i);
    const uint8_t& operator[](unsigned int const i) const;
    
    void set(uint32_t i, uint8_t item);
    uint8_t get(uint32_t i);
    
    bool empty(void);
    uint32_t size(void) const;
    uint32_t max_size(void);
    
    void resize(uint32_t newSize);
    void reserve(uint32_t newCapacity);
    
    // String functions
    
    int find(const char* substr) const;
    int find(const string& substr) const;
    
    void replace(const char* target, const char* replacement);
    void replace(const string& target, const string& replacement);
    
    void insert(uint32_t index, uint8_t item);
    void insert(uint32_t index, const string str);
    void insert(uint32_t index, const char* str);
    
    void remove(uint32_t index);
    void remove(const char* substr);
    void remove(const string& substr);
    
private:
    
    uint32_t mSize;
    uint32_t mCapacity;
    uint32_t mAddress;
    uint32_t mDummyIndex;
    mutable uint8_t mDummy;
    
    void mUpdateBuffer(uint32_t index, uint8_t value);
    uint8_t mReadBuffer(uint32_t index) const;
};

#endif
