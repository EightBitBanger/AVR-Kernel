#include <stdint.h>
#include <stddef.h>

typedef struct {
    char *next_token;
    const char *delim;
} cstr_tokenizer_t;

// Initializes the tokenizer with a string and its delimiters
void cstr_tokenizer_init(cstr_tokenizer_t* tokenizer, char* str, const char* delim);

// Retrieves the next token. Returns NULL when finished.
char* cstr_tokenizer_next(cstr_tokenizer_t* tokenizer);
