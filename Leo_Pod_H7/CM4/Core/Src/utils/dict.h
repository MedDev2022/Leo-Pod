/*
 * dict.h
 *
 *  Created on: 5 Dec 2023
 *      Author: Eyal
 */

#ifndef DICT_H_
#define DICT_H_

typedef struct dict_entry_s {
    const char *key;
    int value;
} dict_entry_s;

typedef struct dict_s {
    int len;
    int cap;
    dict_entry_s *entry;
} dict_s, *dict_t;


int dict_find_index(dict_t dict, const char *key);
int dict_find(dict_t dict, const char *key, int def);
void dict_add(dict_t dict, const char *key, int value);
dict_t dict_new(void);
void dict_free(dict_t dict);
void dict_print(dict_t dict);


#endif /* DICT_H_ */
