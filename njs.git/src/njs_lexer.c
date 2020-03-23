
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */


#include <njs_main.h>


typedef struct njs_lexer_multi_s  njs_lexer_multi_t;

struct njs_lexer_multi_s {
    uint8_t                  symbol;
    uint8_t                  token;
    uint8_t                  count;
    const njs_lexer_multi_t  *next;
};


static njs_int_t njs_lexer_hash_test(njs_lvlhsh_query_t *lhq, void *data);
static njs_int_t njs_lexer_word(njs_lexer_t *lexer, njs_lexer_token_t *token);
static void njs_lexer_string(njs_lexer_t *lexer, njs_lexer_token_t *token,
    u_char quote);
static void njs_lexer_number(njs_lexer_t *lexer, njs_lexer_token_t *token);
static void njs_lexer_multi(njs_lexer_t *lexer, njs_lexer_token_t *token,
    const njs_lexer_multi_t *multi, size_t length);
static void njs_lexer_division(njs_lexer_t *lexer, njs_lexer_token_t *token);

static njs_lexer_token_t *njs_lexer_token_push(njs_vm_t *vm,
    njs_lexer_t *lexer);
static njs_lexer_token_t *njs_lexer_token_pop(njs_lexer_t *lexer);


const njs_lvlhsh_proto_t  njs_lexer_hash_proto
    njs_aligned(64) =
{
    NJS_LVLHSH_DEFAULT,
    njs_lexer_hash_test,
    njs_lvlhsh_alloc,
    njs_lvlhsh_free,
};


static const uint8_t  njs_tokens[256]  njs_aligned(64) = {

                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
    /* \t */    NJS_TOKEN_ILLEGAL,           NJS_TOKEN_SPACE,
    /* \n */    NJS_TOKEN_LINE_END,          NJS_TOKEN_ILLEGAL,
    /* \r */    NJS_TOKEN_ILLEGAL,           NJS_TOKEN_SPACE,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,

    /* 0x10 */  NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,

    /*   ! */   NJS_TOKEN_SPACE,             NJS_TOKEN_LOGICAL_NOT,
    /* " # */   NJS_TOKEN_DOUBLE_QUOTE,      NJS_TOKEN_ILLEGAL,
    /* $ % */   NJS_TOKEN_LETTER,            NJS_TOKEN_REMAINDER,
    /* & ' */   NJS_TOKEN_BITWISE_AND,       NJS_TOKEN_SINGLE_QUOTE,
    /* ( ) */   NJS_TOKEN_OPEN_PARENTHESIS,  NJS_TOKEN_CLOSE_PARENTHESIS,
    /* * + */   NJS_TOKEN_MULTIPLICATION,    NJS_TOKEN_ADDITION,
    /* , - */   NJS_TOKEN_COMMA,             NJS_TOKEN_SUBSTRACTION,
    /* . / */   NJS_TOKEN_DOT,               NJS_TOKEN_DIVISION,

    /* 0 1 */   NJS_TOKEN_DIGIT,             NJS_TOKEN_DIGIT,
    /* 2 3 */   NJS_TOKEN_DIGIT,             NJS_TOKEN_DIGIT,
    /* 4 5 */   NJS_TOKEN_DIGIT,             NJS_TOKEN_DIGIT,
    /* 6 7 */   NJS_TOKEN_DIGIT,             NJS_TOKEN_DIGIT,
    /* 8 9 */   NJS_TOKEN_DIGIT,             NJS_TOKEN_DIGIT,
    /* : ; */   NJS_TOKEN_COLON,             NJS_TOKEN_SEMICOLON,
    /* < = */   NJS_TOKEN_LESS,              NJS_TOKEN_ASSIGNMENT,
    /* > ? */   NJS_TOKEN_GREATER,           NJS_TOKEN_CONDITIONAL,

    /* @ A */   NJS_TOKEN_ILLEGAL,           NJS_TOKEN_LETTER,
    /* B C */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* D E */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* F G */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* H I */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* J K */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* L M */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* N O */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,

    /* P Q */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* R S */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* T U */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* V W */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* X Y */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* Z [ */   NJS_TOKEN_LETTER,            NJS_TOKEN_OPEN_BRACKET,
    /* \ ] */   NJS_TOKEN_ILLEGAL,           NJS_TOKEN_CLOSE_BRACKET,
    /* ^ _ */   NJS_TOKEN_BITWISE_XOR,       NJS_TOKEN_LETTER,

    /* ` a */   NJS_TOKEN_GRAVE,             NJS_TOKEN_LETTER,
    /* b c */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* d e */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* f g */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* h i */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* j k */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* l m */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* n o */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,

    /* p q */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* r s */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* t u */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* v w */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* x y */   NJS_TOKEN_LETTER,            NJS_TOKEN_LETTER,
    /* z { */   NJS_TOKEN_LETTER,            NJS_TOKEN_OPEN_BRACE,
    /* | } */   NJS_TOKEN_BITWISE_OR,        NJS_TOKEN_CLOSE_BRACE,
    /* ~   */   NJS_TOKEN_BITWISE_NOT,       NJS_TOKEN_ILLEGAL,

    /* 0x80 */  NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,

    /* 0x90 */  NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,

    /* 0xA0 */  NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,

    /* 0xB0 */  NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,

    /* TODO: the first byte of valid UTF-8: 0xC2 - 0xF4. */

    /* 0xC0 */  NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,

    /* 0xD0 */  NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,

    /* 0xE0 */  NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,

    /* 0xF0 */  NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
                NJS_TOKEN_ILLEGAL,           NJS_TOKEN_ILLEGAL,
};


static const njs_lexer_multi_t  njs_addition_token[] = {
    { '+', NJS_TOKEN_INCREMENT, 0, NULL },
    { '=', NJS_TOKEN_ADDITION_ASSIGNMENT, 0, NULL },
};


static const njs_lexer_multi_t  njs_substraction_token[] = {
    { '-', NJS_TOKEN_DECREMENT, 0, NULL },
    { '=', NJS_TOKEN_SUBSTRACTION_ASSIGNMENT, 0, NULL },
};


static const njs_lexer_multi_t  njs_exponentiation_token[] = {
    { '=', NJS_TOKEN_EXPONENTIATION_ASSIGNMENT, 0, NULL },
};


static const njs_lexer_multi_t  njs_multiplication_token[] = {
    { '=', NJS_TOKEN_MULTIPLICATION_ASSIGNMENT, 0, NULL },
    { '*', NJS_TOKEN_EXPONENTIATION, 1, njs_exponentiation_token },
};


static const njs_lexer_multi_t  njs_remainder_token[] = {
    { '=', NJS_TOKEN_REMAINDER_ASSIGNMENT, 0, NULL },
};


static const njs_lexer_multi_t  njs_bitwise_and_token[] = {
    { '&', NJS_TOKEN_LOGICAL_AND, 0, NULL },
    { '=', NJS_TOKEN_BITWISE_AND_ASSIGNMENT, 0, NULL },
};


static const njs_lexer_multi_t  njs_bitwise_xor_token[] = {
    { '=', NJS_TOKEN_BITWISE_XOR_ASSIGNMENT, 0, NULL },
};


static const njs_lexer_multi_t  njs_bitwise_or_token[] = {
    { '|', NJS_TOKEN_LOGICAL_OR, 0, NULL },
    { '=', NJS_TOKEN_BITWISE_OR_ASSIGNMENT, 0, NULL },
};


static const njs_lexer_multi_t  njs_strict_not_equal_token[] = {
    { '=', NJS_TOKEN_STRICT_NOT_EQUAL, 0, NULL },
};


static const njs_lexer_multi_t  njs_logical_not_token[] = {
    { '=', NJS_TOKEN_NOT_EQUAL, 1, njs_strict_not_equal_token },
};


static const njs_lexer_multi_t  njs_less_shift_token[] = {
    { '=', NJS_TOKEN_LEFT_SHIFT_ASSIGNMENT, 0, NULL },
};


static const njs_lexer_multi_t  njs_less_token[] = {
    { '=', NJS_TOKEN_LESS_OR_EQUAL, 0, NULL },
    { '<', NJS_TOKEN_LEFT_SHIFT, 1, njs_less_shift_token },
};


static const njs_lexer_multi_t  njs_strict_equal_token[] = {
    { '=', NJS_TOKEN_STRICT_EQUAL, 0, NULL },
};


static const njs_lexer_multi_t  njs_unsigned_right_shift_token[] = {
    { '=', NJS_TOKEN_UNSIGNED_RIGHT_SHIFT_ASSIGNMENT, 0, NULL },
};


static const njs_lexer_multi_t  njs_right_shift_token[] = {
    { '=', NJS_TOKEN_RIGHT_SHIFT_ASSIGNMENT, 0, NULL },
    { '>', NJS_TOKEN_UNSIGNED_RIGHT_SHIFT, 1,
           njs_unsigned_right_shift_token },
};


static const njs_lexer_multi_t  njs_greater_token[] = {
    { '=', NJS_TOKEN_GREATER_OR_EQUAL, 0, NULL },
    { '>', NJS_TOKEN_RIGHT_SHIFT, 2, njs_right_shift_token },
};


static const njs_lexer_multi_t  njs_conditional_token[] = {
    { '?', NJS_TOKEN_COALESCE, 0, NULL },
};


static const njs_lexer_multi_t  njs_assignment_token[] = {
    { '=', NJS_TOKEN_EQUAL, 1, njs_strict_equal_token },
    { '>', NJS_TOKEN_ARROW, 0, NULL },
};


njs_int_t
njs_lexer_init(njs_vm_t *vm, njs_lexer_t *lexer, njs_str_t *file,
    u_char *start, u_char *end)
{
    njs_memzero(lexer, sizeof(njs_lexer_t));

    lexer->file = *file;
    lexer->start = start;
    lexer->end = end;
    lexer->line = 1;
    lexer->keywords_hash = &vm->shared->keywords_hash;
    lexer->mem_pool = vm->mem_pool;

    njs_queue_init(&lexer->preread);

    return NJS_OK;
}


njs_token_type_t
njs_lexer_token(njs_vm_t *vm, njs_lexer_t *lexer)
{
    njs_lexer_token_t  *lt;

    lexer->prev_start = lexer->start;

    if (lexer->token != NULL) {
        lexer->prev_type = lexer->token->type;
        njs_mp_free(vm->mem_pool, lexer->token);
    }

    if (njs_queue_is_empty(&lexer->preread)) {
        lt = njs_lexer_token_push(vm, lexer);
        if (njs_slow_path(lt == NULL)) {
            return NJS_TOKEN_ERROR;
        }
    }

    lexer->token = njs_lexer_token_pop(lexer);

    return lexer->token->type;
}


njs_token_type_t
njs_lexer_peek_token(njs_vm_t *vm, njs_lexer_t *lexer, size_t offset)
{
    size_t             i;
    njs_queue_link_t   *link;
    njs_lexer_token_t  *lt;

    /* GCC and Clang complain about uninitialized lt. */
    lt = NULL;

    link = njs_queue_first(&lexer->preread);

    for (i = 0; i <= offset; i++) {

        if (link != njs_queue_tail(&lexer->preread)) {

            lt = njs_queue_link_data(link, njs_lexer_token_t, link);

            /* NJS_TOKEN_DIVISION stands for regexp literal. */

            if (lt->type == NJS_TOKEN_DIVISION || lt->type == NJS_TOKEN_END) {
                break;
            }

            link = njs_queue_next(link);

        } else {

            lt = njs_lexer_token_push(vm, lexer);

            if (njs_slow_path(lt == NULL)) {
                return NJS_TOKEN_ERROR;
            }
        }
    }

    return lt->type;
}


njs_int_t
njs_lexer_rollback(njs_vm_t *vm, njs_lexer_t *lexer)
{
    njs_lexer_token_t  *lt;

    lt = njs_mp_zalloc(vm->mem_pool, sizeof(njs_lexer_token_t));
    if (njs_slow_path(lt == NULL)) {
        return NJS_ERROR;
    }

    *lt = *lexer->token;

    njs_queue_insert_head(&lexer->preread, &lt->link);

    return NJS_OK;
}


static njs_lexer_token_t *
njs_lexer_token_push(njs_vm_t *vm, njs_lexer_t *lexer)
{
    njs_int_t          ret;
    njs_lexer_token_t  *token;

    token = njs_mp_zalloc(vm->mem_pool, sizeof(njs_lexer_token_t));
    if (njs_slow_path(token == NULL)) {
        return NULL;
    }

    do {
        ret = njs_lexer_next_token(lexer, token);
        if (njs_slow_path(ret != NJS_OK)) {
            return NULL;
        }

    } while (token->type == NJS_TOKEN_COMMENT);

    njs_queue_insert_tail(&lexer->preread, &token->link);

    return token;
}


static njs_lexer_token_t *
njs_lexer_token_pop(njs_lexer_t *lexer)
{
    njs_queue_link_t  *lnk;

    lnk = njs_queue_first(&lexer->preread);
    njs_queue_remove(lnk);

    return njs_queue_link_data(lnk, njs_lexer_token_t, link);
}


njs_int_t
njs_lexer_next_token(njs_lexer_t *lexer, njs_lexer_token_t *token)
{
    u_char  c, *p;

    c = ' ';

    while (lexer->start < lexer->end) {
        c = *lexer->start++;

        if (njs_tokens[c] != NJS_TOKEN_SPACE) {
            break;
        }
    }

    lexer->keyword = 0;
    token->type = njs_tokens[c];

    switch (token->type) {

    case NJS_TOKEN_LETTER:
        return njs_lexer_word(lexer, token);

    case NJS_TOKEN_DOUBLE_QUOTE:
    case NJS_TOKEN_SINGLE_QUOTE:
        njs_lexer_string(lexer, token, c);
        break;

    case NJS_TOKEN_DOT:
        p = lexer->start;

        if (p + 1 < lexer->end
            && njs_tokens[p[0]] == NJS_TOKEN_DOT
            && njs_tokens[p[1]] == NJS_TOKEN_DOT)
        {
            token->text.start = lexer->start - 1;
            token->text.length = (p - token->text.start) + 2;

            token->type = NJS_TOKEN_ELLIPSIS;

            lexer->start += 2;

            return NJS_OK;
        }

        if (p == lexer->end || njs_tokens[*p] != NJS_TOKEN_DIGIT) {
            token->text.start = lexer->start - 1;
            token->text.length = p - token->text.start;

            token->type = NJS_TOKEN_DOT;

            return NJS_OK;
        }

        /* Fall through. */

    case NJS_TOKEN_DIGIT:
        njs_lexer_number(lexer, token);
        break;

    case NJS_TOKEN_DIVISION:
        njs_lexer_division(lexer, token);
        break;

    case NJS_TOKEN_ASSIGNMENT:
        njs_lexer_multi(lexer, token, njs_assignment_token,
                        njs_nitems(njs_assignment_token));
        break;

    case NJS_TOKEN_ADDITION:
        njs_lexer_multi(lexer, token, njs_addition_token,
                        njs_nitems(njs_addition_token));
        break;

    case NJS_TOKEN_SUBSTRACTION:
        njs_lexer_multi(lexer, token, njs_substraction_token,
                        njs_nitems(njs_substraction_token));
        break;

    case NJS_TOKEN_MULTIPLICATION:
        njs_lexer_multi(lexer, token, njs_multiplication_token,
                        njs_nitems(njs_multiplication_token));
        break;

    case NJS_TOKEN_REMAINDER:
        njs_lexer_multi(lexer, token, njs_remainder_token,
                        njs_nitems(njs_remainder_token));
        break;

    case NJS_TOKEN_BITWISE_AND:
        njs_lexer_multi(lexer, token, njs_bitwise_and_token,
                        njs_nitems(njs_bitwise_and_token));
        break;

    case NJS_TOKEN_BITWISE_XOR:
        njs_lexer_multi(lexer, token, njs_bitwise_xor_token,
                        njs_nitems(njs_bitwise_xor_token));
        break;

    case NJS_TOKEN_BITWISE_OR:
        njs_lexer_multi(lexer, token, njs_bitwise_or_token,
                        njs_nitems(njs_bitwise_or_token));
        break;

    case NJS_TOKEN_LOGICAL_NOT:
        njs_lexer_multi(lexer, token, njs_logical_not_token,
                        njs_nitems(njs_logical_not_token));
        break;

    case NJS_TOKEN_LESS:
        njs_lexer_multi(lexer, token, njs_less_token,
                        njs_nitems(njs_less_token));
        break;

    case NJS_TOKEN_GREATER:
        njs_lexer_multi(lexer, token, njs_greater_token,
                        njs_nitems(njs_greater_token));
        break;

    case NJS_TOKEN_CONDITIONAL:
        njs_lexer_multi(lexer, token, njs_conditional_token,
                        njs_nitems(njs_conditional_token));
        break;

    case NJS_TOKEN_SPACE:
        token->type = NJS_TOKEN_END;
        return NJS_OK;

    case NJS_TOKEN_LINE_END:
        lexer->line++;

        /* Fall through. */

    default:
        token->text.start = lexer->start - 1;
        token->text.length = lexer->start - token->text.start;

        break;
    }

    return NJS_OK;
}


static njs_int_t
njs_lexer_hash_test(njs_lvlhsh_query_t *lhq, void *data)
{
    njs_lexer_entry_t  *entry;

    entry = data;

    if (entry->name.length == lhq->key.length
        && memcmp(entry->name.start, lhq->key.start, lhq->key.length) == 0)
    {
        return NJS_OK;
    }

    return NJS_DECLINED;
}


static njs_lexer_entry_t *
njs_lexer_keyword_find(njs_lexer_t *lexer, u_char *key, size_t length,
    uint32_t hash)
{
    njs_int_t           ret;
    njs_lexer_entry_t   *entry;
    njs_lvlhsh_query_t  lhq;

    lhq.key.start = key;
    lhq.key.length = length;

    lhq.key_hash = hash;
    lhq.proto = &njs_lexer_hash_proto;

    ret = njs_lvlhsh_find(lexer->keywords_hash, &lhq);
    if (ret == NJS_OK) {
        return lhq.value;
    }

    entry = njs_mp_alloc(lexer->mem_pool, sizeof(njs_lexer_entry_t));
    if (njs_slow_path(entry == NULL)) {
        return NULL;
    }

    entry->name.start = njs_mp_alloc(lexer->mem_pool, length + 1);
    if (njs_slow_path(entry->name.start == NULL)) {
        return NULL;
    }

    memcpy(entry->name.start, key, length);

    entry->name.start[length] = '\0';
    entry->name.length = length;

    lhq.value = entry;
    lhq.pool = lexer->mem_pool;

    ret = njs_lvlhsh_insert(lexer->keywords_hash, &lhq);
    if (njs_slow_path(ret != NJS_OK)) {
        return NULL;
    }

    return entry;
}


static njs_int_t
njs_lexer_word(njs_lexer_t *lexer, njs_lexer_token_t *token)
{
    u_char                           *p, c;
    uint32_t                         hash_id;
    const njs_lexer_entry_t          *entry;
    const njs_lexer_keyword_entry_t  *key_entry;

    /* TODO: UTF-8 */

    static const uint8_t letter_digit[32]  njs_aligned(32) = {
        0x00, 0x00, 0x00, 0x00, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

                                /* '&%$ #"!  /.-, |*)(  7654 3210 ?>=< ;:98 */
        0x10, 0x00, 0xff, 0x03, /* 0001 0000 0000 0000  1111 1111 0000 0011 */

                                /* GFED CBA@ ONML KJIH  WVUT SRQP _^]\ [ZYX */
        0xfe, 0xff, 0xff, 0x87, /* 1111 1110 1111 1111  1111 1111 1000 0111 */

                                /* gfed cba` onml kjih  wvut srqp  ~}| {zyx */
        0xfe, 0xff, 0xff, 0x07, /* 1111 1110 1111 1111  1111 1111 0000 0111 */

        0x00, 0x00, 0x00, 0x00, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
        0x00, 0x00, 0x00, 0x00, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
        0x00, 0x00, 0x00, 0x00, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
        0x00, 0x00, 0x00, 0x00, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
    };

    token->line = lexer->line;
    token->text.start = lexer->start - 1;

    hash_id = njs_djb_hash_add(NJS_DJB_HASH_INIT, *token->text.start);

    for (p = lexer->start; p < lexer->end; p++) {
        c = *p;

        if ((letter_digit[c / 8] & (1 << (c & 7))) == 0) {
            break;
        }

        hash_id = njs_djb_hash_add(hash_id, c);
    }

    token->text.length = p - token->text.start;
    lexer->start = p;

    key_entry = njs_lexer_keyword(token->text.start, token->text.length);

    if (key_entry == NULL) {
        entry = njs_lexer_keyword_find(lexer, token->text.start,
                                       token->text.length, hash_id);
        if (njs_slow_path(entry == NULL)) {
            return NJS_ERROR;
        }

        token->type = NJS_TOKEN_NAME;

    } else {
        entry = &key_entry->value->entry;
        token->type = key_entry->value->type;

        lexer->keyword = 1;
    }

    token->unique_id = (uintptr_t) entry;

    return NJS_OK;
}


static void
njs_lexer_string(njs_lexer_t *lexer, njs_lexer_token_t *token, u_char quote)
{
    u_char      *p, c;
    njs_bool_t  escape;

    escape = 0;

    p = lexer->start;
    token->text.start = p;

    while (p < lexer->end) {

        c = *p++;

        if (c == '\\') {
            if (p == lexer->end) {
                break;
            }

            p++;

            /* Line continuation. */
            if (p < lexer->end && p[-1] == '\r' && p[0] == '\n') {
                p++;
            }

            escape = 1;

            continue;
        }

        /* Line terminator. */
        if (c == '\r' || c == '\n') {
            break;
        }

        if (c == quote) {
            lexer->start = p;
            token->text.length = (p - 1) - token->text.start;

            token->type = (escape == 0) ? NJS_TOKEN_STRING
                                        : NJS_TOKEN_ESCAPE_STRING;
            return;
        }
    }

    token->text.start--;
    token->text.length = p - token->text.start;

    token->type = NJS_TOKEN_UNTERMINATED_STRING;
}


static void
njs_lexer_number(njs_lexer_t *lexer, njs_lexer_token_t *token)
{
    u_char        c;
    const u_char  *p;

    c = lexer->start[-1];
    p = lexer->start;

    token->text.start = lexer->start - 1;

    if (c == '0' && p != lexer->end) {

        /* Hexadecimal literal values. */

        if (*p == 'x' || *p == 'X') {
            p++;

            if (p == lexer->end) {
                goto illegal_token;
            }

            token->number = njs_number_hex_parse(&p, lexer->end);

            goto done;
        }

        /* Octal literal values. */

        if (*p == 'o' || *p == 'O') {
            p++;

            if (p == lexer->end) {
                goto illegal_token;
            }

            token->number = njs_number_oct_parse(&p, lexer->end);

            if (p < lexer->end && (*p == '8' || *p == '9')) {
                goto illegal_trailer;
            }

            goto done;
        }

        /* Binary literal values. */

        if (*p == 'b' || *p == 'B') {
            p++;

            if (p == lexer->end) {
                goto illegal_token;
            }

            token->number = njs_number_bin_parse(&p, lexer->end);

            if (p < lexer->end && (*p >= '2' && *p <= '9')) {
                goto illegal_trailer;
            }

            goto done;
        }

        /* Legacy Octal literals are deprecated. */

        if (*p >= '0' && *p <= '9') {
            goto illegal_trailer;
        }
    }

    p--;
    token->number = njs_number_dec_parse(&p, lexer->end);

done:

    lexer->start = (u_char *) p;
    token->text.length = p - token->text.start;

    token->type = NJS_TOKEN_NUMBER;

    return;

illegal_trailer:

    p++;

illegal_token:

    token->text.length = p - token->text.start;

    token->type = NJS_TOKEN_ILLEGAL;
}


static void
njs_lexer_multi(njs_lexer_t *lexer, njs_lexer_token_t *token,
    const njs_lexer_multi_t *multi, size_t length)
{
    u_char  c;

    token->text.start = lexer->start - 1;

    while (length != 0 && multi != NULL && lexer->start < lexer->end) {
        c = lexer->start[0];

        if (c == multi->symbol) {
            lexer->start++;

            token->type = multi->token;

            if (multi->count == 0) {
                break;
            }

            length = multi->count;
            multi = multi->next;

        } else {
            length--;
            multi++;
        }
    }

    token->text.length = lexer->start - token->text.start;
}


static void
njs_lexer_division(njs_lexer_t *lexer, njs_lexer_token_t *token)
{
    u_char  c, *p;

    token->text.start = lexer->start - 1;

    if (lexer->start >= lexer->end) {
        goto done;
    }

    c = lexer->start[0];

    if (c == '/') {
        lexer->start++;

        for (p = lexer->start; p < lexer->end; p++) {

            if (*p == '\n' || (p + 1) == lexer->end) {
                lexer->start = p + 1;
                lexer->line++;

                token->type = NJS_TOKEN_LINE_END;

                goto done;
            }
        }

    } else if (c == '*') {
        lexer->start++;

        for (p = lexer->start; p < lexer->end; p++) {

            if (*p == '\n') {
                lexer->line++;
                continue;
            }

            if (*p == '*') {
                if (p + 1 < lexer->end && p[1] == '/') {
                    lexer->start = p + 2;

                    token->type = NJS_TOKEN_COMMENT;

                    goto done;
                }
            }
        }

        token->type = NJS_TOKEN_ILLEGAL;

    } else if (c == '=') {
        lexer->start++;

        token->type = NJS_TOKEN_DIVISION_ASSIGNMENT;
    }

done:

    token->text.length = lexer->start - token->text.start;
}
