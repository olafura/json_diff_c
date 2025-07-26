#ifndef __JSMN_H_
#define __JSMN_H_
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** JSON type identifier. */
typedef enum {
    JSMN_UNDEFINED = 0,
    JSMN_OBJECT    = 1,
    JSMN_ARRAY     = 2,
    JSMN_STRING    = 3,
    JSMN_PRIMITIVE = 4
} jsmntype_t;

/** JSON token description. */
typedef struct {
    jsmntype_t type; /**< token type */
    int start;       /**< start position in JSON data */
    int end;         /**< end position in JSON data */
    int size;        /**< number of child tokens */
    int parent;      /**< index of parent token */
} jsmntok_t;

/** JSON parser. Contains an array of token blocks available. */
typedef struct {
    unsigned int pos;     /**< offset in the JSON string */
    unsigned int toknext; /**< next token to allocate */
    int toksuper;         /**< superior token node, e.g. parent object or array */
} jsmn_parser;

/**
 * Create JSON parser over an array of tokens.
 */
void jsmn_init(jsmn_parser *parser);

/**
 * Run JSON parser. It parses a JSON data string into a token stream.
 * Returns the number of tokens actually used.
 */
int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
               jsmntok_t *tokens, unsigned int num_tokens);

#ifdef __cplusplus
}
#endif

#endif /* __JSMN_H_ */