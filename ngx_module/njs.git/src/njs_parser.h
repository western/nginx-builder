
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Alexander Borisov
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NJS_PARSER_H_INCLUDED_
#define _NJS_PARSER_H_INCLUDED_


struct njs_parser_scope_s {
    njs_parser_node_t               *top;

    njs_queue_link_t                link;
    njs_queue_t                     nested;

    njs_parser_scope_t              *parent;
    njs_rbtree_t                    variables;
    njs_rbtree_t                    labels;
    njs_rbtree_t                    references;

#define NJS_SCOPE_INDEX_LOCAL       0
#define NJS_SCOPE_INDEX_CLOSURE     1

    njs_arr_t                       *values[2];  /* Array of njs_value_t. */
    njs_index_t                     next_index[2];

    njs_str_t                       cwd;
    njs_str_t                       file;

    njs_scope_t                     type:8;
    uint8_t                         nesting;     /* 4 bits */
    uint8_t                         argument_closures;
    uint8_t                         module;
    uint8_t                         arrow_function;
};


struct njs_parser_node_s {
    njs_token_type_t                token_type:16;
    uint8_t                         ctor:1;
    uint8_t                         temporary;    /* 1 bit  */
    uint8_t                         hoist;        /* 1 bit  */
    uint32_t                        token_line;

    union {
        uint32_t                    length;
        njs_variable_reference_t    reference;
        njs_value_t                 value;
        njs_vmcode_operation_t      operation;
        njs_parser_node_t           *object;
    } u;

    njs_str_t                       name;

    njs_index_t                     index;

    /*
     * The scope points to
     *   in global and function node: global or function scopes;
     *   in variable node: a scope where variable was referenced;
     *   in operation node: a scope to allocate indexes for temporary values.
     */
    njs_parser_scope_t              *scope;

    njs_parser_node_t               *left;
    njs_parser_node_t               *right;
    njs_parser_node_t               *dest;
};


typedef njs_int_t (*njs_parser_state_func_t)(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);


struct njs_parser_s {
    njs_parser_state_func_t         state;
    njs_queue_t                     stack;
    njs_lexer_t                     *lexer;
    njs_vm_t                        *vm;
    njs_parser_node_t               *node;
    njs_parser_node_t               *target;
    njs_parser_scope_t              *scope;
    njs_int_t                       ret;
    njs_bool_t                      strict_semicolon;
    uint32_t                        line;
};


typedef struct {
    njs_parser_state_func_t         state;
    njs_queue_link_t                link;

    njs_parser_node_t               *node;

    njs_bool_t                      optional;
} njs_parser_stack_entry_t;


typedef struct {
    NJS_RBTREE_NODE                 (node);
    uintptr_t                       key;
    njs_parser_node_t               *parser_node;
} njs_parser_rbtree_node_t;


njs_int_t njs_parser_failed_state(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

intptr_t njs_parser_scope_rbtree_compare(njs_rbtree_node_t *node1,
    njs_rbtree_node_t *node2);
njs_int_t njs_parser(njs_parser_t *parser, njs_parser_t *prev);

njs_int_t njs_parser_module_lambda(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
njs_variable_t *njs_variable_resolve(njs_vm_t *vm, njs_parser_node_t *node);
njs_index_t njs_variable_index(njs_vm_t *vm, njs_parser_node_t *node);
njs_bool_t njs_parser_has_side_effect(njs_parser_node_t *node);
njs_token_type_t njs_parser_unexpected_token(njs_vm_t *vm, njs_parser_t *parser,
    njs_str_t *name, njs_token_type_t type);
njs_int_t njs_parser_string_create(njs_vm_t *vm, njs_lexer_token_t *token,
    njs_value_t *value);
u_char *njs_parser_trace_handler(njs_trace_t *trace, njs_trace_data_t *td,
    u_char *start);
void njs_parser_lexer_error(njs_parser_t *parser,
    njs_object_type_t type, const char *fmt, ...);
void njs_parser_node_error(njs_vm_t *vm, njs_parser_node_t *node,
    njs_object_type_t type, const char *fmt, ...);

njs_int_t njs_parser_serialize_ast(njs_parser_node_t *node, njs_chb_t *chain);


#define njs_parser_restricted_identifier(token)                               \
    (token == NJS_TOKEN_ARGUMENTS || token == NJS_TOKEN_EVAL)


#define njs_parser_is_lvalue(node)                                            \
    ((node)->token_type == NJS_TOKEN_NAME                                     \
     || (node)->token_type == NJS_TOKEN_PROPERTY)


#define njs_scope_accumulative(vm, scope)                                     \
    ((vm)->options.accumulative && (scope)->type == NJS_SCOPE_GLOBAL)


#define njs_parser_syntax_error(parser, fmt, ...)                             \
    njs_parser_lexer_error(parser, NJS_OBJ_TYPE_SYNTAX_ERROR, fmt,            \
                           ##__VA_ARGS__)


#define njs_parser_ref_error(parser, fmt, ...)                                \
    njs_parser_lexer_error(parser, NJS_OBJ_TYPE_REF_ERROR, fmt,               \
                           ##__VA_ARGS__)


njs_inline njs_parser_node_t *
njs_parser_node_new(njs_parser_t *parser, njs_token_type_t type)
{
    njs_parser_node_t  *node;

    node = njs_mp_zalloc(parser->vm->mem_pool, sizeof(njs_parser_node_t));

    if (njs_fast_path(node != NULL)) {
        node->token_type = type;
        node->scope = parser->scope;
    }

    return node;
}


njs_inline void
njs_parser_node_free(njs_parser_t *parser, njs_parser_node_t *node)
{
    njs_mp_free(parser->vm->mem_pool, node);
}


njs_inline njs_parser_node_t *
njs_parser_node_string(njs_vm_t *vm, njs_lexer_token_t *token,
    njs_parser_t *parser)
{
    njs_int_t          ret;
    njs_parser_node_t  *node;

    node = njs_parser_node_new(parser, NJS_TOKEN_STRING);
    if (njs_slow_path(node == NULL)) {
        return NULL;
    }

    ret = njs_parser_string_create(vm, token, &node->u.value);
    if (njs_slow_path(ret != NJS_OK)) {
        return NULL;
    }

    node->token_line = token->line;

    return node;
}


njs_inline njs_parser_scope_t *
njs_parser_global_scope(njs_vm_t *vm)
{
    njs_parser_scope_t  *scope;

    scope = vm->parser->scope;

    while (scope->type != NJS_SCOPE_GLOBAL) {
        scope = scope->parent;
    }

    return scope;
}


njs_inline njs_parser_scope_t *
njs_function_scope(njs_parser_scope_t *scope, njs_bool_t any)
{
    while (scope->type != NJS_SCOPE_GLOBAL) {
        if (scope->type == NJS_SCOPE_FUNCTION
            && (any || !scope->arrow_function))
        {
            return scope;
        }

        scope = scope->parent;
    }

    return NULL;
}

#ifndef NJS_PARSER_DEBUG

njs_inline void
njs_parser_next(njs_parser_t *parser, njs_parser_state_func_t state)
{
    parser->state = state;
}

#else

njs_inline int
njs_parser_height(njs_parser_t *parser, njs_queue_link_t *link)
{
    int               height;
    njs_queue_link_t  *lnk;

    height = 0;

   for (lnk = njs_queue_first(&parser->stack);
        lnk != njs_queue_tail(&parser->stack);
        lnk = njs_queue_next(lnk))
   {
       if (link != lnk) {
            height++;
            continue;
       }

       return height;
   }

   return -1;
}

#define njs_parser_next(parser, _state)                                     \
    do {                                                                    \
        const char *name = njs_stringify(_state);                           \
        if (memcmp(name, "entry->state", njs_min(njs_strlen(name), 12))) {  \
            njs_printf("next(%s)\n", name + njs_length("njs_parser_"));     \
        }                                                                   \
                                                                            \
        parser->state = _state;                                             \
    } while(0)

#endif


njs_inline njs_int_t
njs_parser_stack_pop(njs_parser_t *parser)
{
    njs_parser_stack_entry_t  *entry;


    entry = njs_queue_link_data(njs_queue_first(&parser->stack),
                                njs_parser_stack_entry_t, link);

    njs_queue_remove(njs_queue_first(&parser->stack));

#ifdef NJS_PARSER_DEBUG
    njs_printf("  stack_pop(%d)\n",
               njs_parser_height(parser, njs_queue_last(&parser->stack)));
#endif

    njs_parser_next(parser, entry->state);

    parser->target = entry->node;

    njs_mp_free(parser->vm->mem_pool, entry);

    return NJS_OK;
}


#ifndef NJS_PARSER_DEBUG

#define njs_parser_after(_p, _l, _n, _opt, _state)                          \
    _njs_parser_after(_p, _l, _n, _opt, _state)

#else

#define njs_parser_after(__p, _l, _n, _opt, _state)                         \
    (                                                                       \
        njs_printf(" after(%s, link:%d, height:%d)\n",                      \
                   &njs_stringify(_state)[njs_min(njs_strlen(_state), 11)], \
                   njs_parser_height(__p, _l),                              \
                   njs_parser_height(__p, njs_queue_last(&(__p)->stack))),  \
        _njs_parser_after(__p, _l, _n, _opt, _state)                        \
    )

#endif

njs_inline njs_int_t
_njs_parser_after(njs_parser_t *parser, njs_queue_link_t *link, void *node,
    njs_bool_t is_optional, njs_parser_state_func_t state)
{
    njs_parser_stack_entry_t  *entry;

    entry = njs_mp_alloc(parser->vm->mem_pool,
                         sizeof(njs_parser_stack_entry_t));
    if (njs_slow_path(entry == NULL)) {
        return NJS_ERROR;
    }

    entry->state = state;
    entry->node = node;
    entry->optional = is_optional;

    njs_queue_insert_before(link, &entry->link);

    return NJS_OK;
}


njs_inline njs_int_t
njs_parser_failed(njs_parser_t *parser)
{
    njs_parser_next(parser, njs_parser_failed_state);

    parser->target = NULL;

    return NJS_DECLINED;
}


#endif /* _NJS_PARSER_H_INCLUDED_ */
