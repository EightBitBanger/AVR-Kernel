/*

Thread safe C string tokenizer implementation.

*/

#ifndef _STRING_TOKENIZER_H_
#define _STRING_TOKENIZER_H_

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char* next_token;
    const char* delim;
} cstr_tok_t;

// Initializes the tokenizer with a string and its delimiters
void cstr_tok_init(cstr_tok_t* tokenizer, char* str, const char* delim);

// Retrieves the next token. Returns NULL when finished.
char* cstr_tok_next(cstr_tok_t* tokenizer);

#endif
