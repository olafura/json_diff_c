#include "jsmn.h"
#include <string.h>
#include <stdlib.h>
#include <string.h>

void jsmn_init(jsmn_parser *parser) {
    parser->pos = 0;
    parser->toknext = 0;
    parser->toksuper = -1;
}

static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser,
                                   jsmntok_t *tokens, unsigned int num_tokens) {
    if (parser->toknext >= num_tokens) return NULL;
    jsmntok_t *tok = &tokens[parser->toknext++];
    tok->start = tok->end = -1;
    tok->size = 0;
    tok->parent = -1;
    return tok;
}

static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type,
                            int start, int end) {
    token->type = type;
    token->start = start;
    token->end = end;
    token->size = 0;
}

int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
               jsmntok_t *tokens, unsigned int num_tokens) {
    int i;
    jsmntok_t *token;
    for (; parser->pos < len; parser->pos++) {
        char c = js[parser->pos];
        switch (c) {
        case '{':
        case '[':
            token = jsmn_alloc_token(parser, tokens, num_tokens);
            if (!token) return -1;
            token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
            token->start = parser->pos;
            token->parent = parser->toksuper;
            parser->toksuper = parser->toknext - 1;
            break;
        case '}':
        case ']':
            {
                jsmntype_t type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
                for (i = parser->toknext - 1; i >= 0; i--) {
                    token = &tokens[i];
                    if (token->start != -1 && token->end == -1) {
                        if (token->type != type) return -1;
                        parser->toksuper = token->parent;
                        token->end = parser->pos + 1;
                        break;
                    }
                }
                if (i == -1) return -1;
            }
            break;
        case '"':
            parser->pos++;
            {
                int start = parser->pos;
                while (parser->pos < len) {
                    if (js[parser->pos] == '"') break;
                    if (js[parser->pos] == '\\') parser->pos++;
                    parser->pos++;
                }
                if (parser->pos >= len) return -1;
                token = jsmn_alloc_token(parser, tokens, num_tokens);
                if (!token) return -1;
                jsmn_fill_token(token, JSMN_STRING, start, parser->pos);
                token->parent = parser->toksuper;
            }
            break;
        case '\t': case '\r': case '\n': case ' ': case ',':
            break;
        case ':':
            parser->toksuper = parser->toknext - 1;
            break;
        default:
            parser->pos--;
            {
                int start = parser->pos;
                while (parser->pos < len) {
                    c = js[parser->pos];
                    if (c == '\t' || c == '\r' || c == '\n' ||
                        c == ' ' || c == ',' || c == ']' || c == '}') break;
                    parser->pos++;
                }
                token = jsmn_alloc_token(parser, tokens, num_tokens);
                if (!token) return -1;
                jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
                token->parent = parser->toksuper;
                parser->pos--;
            }
            break;
        }
    }
    for (i = parser->toknext - 1; i >= 0; i--) {
        token = &tokens[i];
        if (token->start != -1 && token->end == -1) return -1;
    }
    return parser->toknext;
}