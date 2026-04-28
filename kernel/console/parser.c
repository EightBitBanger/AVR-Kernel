#include <kernel/console/parser.h>
#include <stdio.h>
#include <stdint.h>

void parser_trim_leading_spaces(char *str) {
    if (str == NULL) return;
    int i = 0;
    while (str[i] == ' ' && str[i] != '\0') {i++;}
    
    if (i > 0) {
        int j = 0;
        while (str[i] != '\0') {str[j++] = str[i++];}
        str[j] = '\0';
    }
}

