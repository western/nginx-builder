
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */


#include <njs_main.h>


struct njs_regexp_group_s {
    njs_str_t  name;
    uint32_t   hash;
    uint32_t   capture;
};


static void *njs_regexp_malloc(size_t size, void *memory_data);
static void njs_regexp_free(void *p, void *memory_data);
static njs_regexp_flags_t njs_regexp_flags(u_char **start, u_char *end,
    njs_bool_t bound);
static njs_int_t njs_regexp_prototype_source(njs_vm_t *vm,
    njs_object_prop_t *prop, njs_value_t *value, njs_value_t *setval,
    njs_value_t *retval);
static int njs_regexp_pattern_compile(njs_vm_t *vm, njs_regex_t *regex,
    u_char *source, int options);
static u_char *njs_regexp_compile_trace_handler(njs_trace_t *trace,
    njs_trace_data_t *td, u_char *start);
static u_char *njs_regexp_match_trace_handler(njs_trace_t *trace,
    njs_trace_data_t *td, u_char *start);
static njs_int_t njs_regexp_exec_result(njs_vm_t *vm, njs_regexp_t *regexp,
    njs_utf8_t utf8, u_char *string, njs_regex_match_data_t *match_data,
    uint32_t last_index);
static njs_int_t njs_regexp_string_create(njs_vm_t *vm, njs_value_t *value,
    u_char *start, uint32_t size, int32_t length);


njs_int_t
njs_regexp_init(njs_vm_t *vm)
{
    vm->regex_context = njs_regex_context_create(njs_regexp_malloc,
                                          njs_regexp_free, vm->mem_pool);
    if (njs_slow_path(vm->regex_context == NULL)) {
        njs_memory_error(vm);
        return NJS_ERROR;
    }

    vm->single_match_data = njs_regex_match_data(NULL, vm->regex_context);
    if (njs_slow_path(vm->single_match_data == NULL)) {
        njs_memory_error(vm);
        return NJS_ERROR;
    }

    vm->regex_context->trace = &vm->trace;

    return NJS_OK;
}


static void *
njs_regexp_malloc(size_t size, void *memory_data)
{
    return njs_mp_alloc(memory_data, size);
}


static void
njs_regexp_free(void *p, void *memory_data)
{
    njs_mp_free(memory_data, p);
}


static njs_regexp_flags_t
njs_regexp_value_flags(njs_vm_t *vm, const njs_value_t *regexp)
{
    njs_regexp_flags_t    flags;
    njs_regexp_pattern_t  *pattern;

    flags = 0;

    pattern = njs_regexp_pattern(regexp);

    if (pattern->global) {
        flags |= NJS_REGEXP_GLOBAL;
    }

    if (pattern->ignore_case) {
        flags |= NJS_REGEXP_IGNORE_CASE;
    }

    if (pattern->multiline) {
        flags |= NJS_REGEXP_MULTILINE;
    }

    return flags;
}


static njs_int_t
njs_regexp_constructor(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    u_char              *start;
    njs_int_t           ret;
    njs_str_t           string;
    njs_value_t         source, flags_string, *pattern, *flags;
    njs_regexp_flags_t  re_flags;

    pattern = njs_arg(args, nargs, 1);

    if (!njs_is_regexp(pattern) && !njs_is_primitive(pattern)) {
        ret = njs_value_to_string(vm, &args[1], &args[1]);
        if (ret != NJS_OK) {
            return ret;
        }
    }

    flags = njs_arg(args, nargs, 2);

    if (!njs_is_primitive(flags)) {
        ret = njs_value_to_string(vm, &args[2], &args[2]);
        if (ret != NJS_OK) {
            return ret;
        }
    }

    re_flags = 0;

    if (njs_is_regexp(pattern)) {
        ret = njs_regexp_prototype_source(vm, NULL, pattern, NULL, &source);
        if (njs_slow_path(ret != NJS_OK)) {
            return ret;
        }

        re_flags = njs_regexp_value_flags(vm, pattern);

        pattern = &source;

    } else {
        if (njs_is_undefined(pattern)) {
            pattern = njs_value_arg(&njs_string_empty);
        }

        ret = njs_primitive_value_to_string(vm, &source, pattern);
        if (njs_slow_path(ret != NJS_OK)) {
            return ret;
        }

        pattern = &source;
    }

    if (njs_is_defined(flags)) {
        ret = njs_primitive_value_to_string(vm, &flags_string, flags);
        if (njs_slow_path(ret != NJS_OK)) {
            return ret;
        }

        njs_string_get(&flags_string, &string);

        start = string.start;

        re_flags = njs_regexp_flags(&start, start + string.length, 1);
        if (njs_slow_path(re_flags < 0)) {
            njs_syntax_error(vm, "Invalid RegExp flags \"%V\"", &string);
            return NJS_ERROR;
        }
    }

    njs_string_get(pattern, &string);

    return njs_regexp_create(vm, &vm->retval, string.start, string.length,
                             re_flags);
}


njs_int_t
njs_regexp_create(njs_vm_t *vm, njs_value_t *value, u_char *start,
    size_t length, njs_regexp_flags_t flags)
{
    njs_regexp_t          *regexp;
    njs_regexp_pattern_t  *pattern;

    if (length != 0) {
        pattern = njs_regexp_pattern_create(vm, start, length, flags);
        if (njs_slow_path(pattern == NULL)) {
            return NJS_ERROR;
        }

    } else {
        pattern = vm->shared->empty_regexp_pattern;
    }

    regexp = njs_regexp_alloc(vm, pattern);

    if (njs_fast_path(regexp != NULL)) {
        njs_set_regexp(value, regexp);

        return NJS_OK;
    }

    return NJS_ERROR;
}


/*
 * 1) PCRE with PCRE_JAVASCRIPT_COMPAT flag rejects regexps with
 * lone closing square brackets as invalid.  Whereas according
 * to ES6: 11.8.5 it is a valid regexp expression.
 *
 * 2) escaping zero byte characters as "\u0000".
 *
 * Escaping it here as a workaround.
 */

njs_inline njs_int_t
njs_regexp_escape(njs_vm_t *vm, njs_str_t *text)
{
    size_t      brackets, zeros;
    u_char      *p, *dst, *start, *end;
    njs_bool_t  in;

    start = text->start;
    end = text->start + text->length;

    in = 0;
    zeros = 0;
    brackets = 0;

    for (p = start; p < end; p++) {

        switch (*p) {
        case '[':
            in = 1;
            break;

        case ']':
            if (!in) {
                brackets++;
            }

            in = 0;
            break;

        case '\\':
            p++;

            if (p == end || *p != '\0') {
                break;
            }

            /* Fall through. */

        case '\0':
            zeros++;
            break;
        }
    }

    if (!brackets && !zeros) {
        return NJS_OK;
    }

    text->length = text->length + brackets + zeros * njs_length("\\u0000");

    text->start = njs_mp_alloc(vm->mem_pool, text->length);
    if (njs_slow_path(text->start == NULL)) {
        njs_memory_error(vm);
        return NJS_ERROR;
    }

    in = 0;
    dst = text->start;

    for (p = start; p < end; p++) {

        switch (*p) {
        case '[':
            in = 1;
            break;

        case ']':
            if (!in) {
                *dst++ = '\\';
            }

            in = 0;
            break;

        case '\\':
            *dst++ = *p++;

            if (p == end) {
                goto done;
            }

            if (*p != '\0') {
                break;
            }

            /* Fall through. */

        case '\0':
            dst = njs_cpymem(dst, "\\u0000", 6);
            continue;
        }

        *dst++ = *p;
    }

done:

    text->length = dst - text->start;

    return NJS_OK;
}


njs_token_type_t
njs_regexp_literal(njs_vm_t *vm, njs_parser_t *parser, njs_value_t *value)
{
    u_char                *p;
    njs_str_t             text;
    njs_lexer_t           *lexer;
    njs_regexp_flags_t    flags;
    njs_regexp_pattern_t  *pattern;

    lexer = parser->lexer;

    for (p = lexer->start; p < lexer->end; p++) {

        switch (*p) {
        case '\n':
        case '\r':
            goto failed;

        case '[':
            while (1) {
                if (++p >= lexer->end) {
                    goto failed;
                }

                if (*p == ']') {
                    break;
                }

                switch (*p) {
                case '\n':
                case '\r':
                    goto failed;

                case '\\':
                    if (++p >= lexer->end || *p == '\n' || *p == '\r') {
                        goto failed;
                    }

                    break;
                }
            }

            break;

        case '\\':
            if (++p >= lexer->end || *p == '\n' || *p == '\r') {
                goto failed;
            }

            break;

        case '/':
            text.start = lexer->start;
            text.length = p - text.start;
            p++;
            lexer->start = p;

            flags = njs_regexp_flags(&p, lexer->end, 0);

            if (njs_slow_path(flags < 0)) {
                njs_parser_syntax_error(vm, parser,
                                        "Invalid RegExp flags \"%*s\"",
                                        p - lexer->start, lexer->start);

                return NJS_TOKEN_ILLEGAL;
            }

            lexer->start = p;

            pattern = njs_regexp_pattern_create(vm, text.start, text.length,
                                                flags);

            if (njs_slow_path(pattern == NULL)) {
                return NJS_TOKEN_ILLEGAL;
            }

            value->data.u.data = pattern;

            return NJS_TOKEN_REGEXP;
        }
    }

failed:

    njs_parser_syntax_error(vm, parser, "Unterminated RegExp \"%*s\"",
                            p - (lexer->start - 1), lexer->start - 1);

    return NJS_TOKEN_ILLEGAL;
}


static njs_regexp_flags_t
njs_regexp_flags(u_char **start, u_char *end, njs_bool_t bound)
{
    u_char              *p;
    njs_regexp_flags_t  flags, flag;

    flags = 0;

    for (p = *start; p < end; p++) {

        switch (*p) {

        case 'g':
            flag = NJS_REGEXP_GLOBAL;
            break;

        case 'i':
            flag = NJS_REGEXP_IGNORE_CASE;
            break;

        case 'm':
            flag = NJS_REGEXP_MULTILINE;
            break;

        case ';':
        case ' ':
        case '\t':
        case '\r':
        case '\n':
        case ',':
        case ')':
        case ']':
        case '}':
        case '.':
            if (!bound) {
                goto done;
            }

            /* Fall through. */

        default:
            goto invalid;
        }

        if (njs_slow_path((flags & flag) != 0)) {
            goto invalid;
        }

        flags |= flag;
    }

done:

    *start = p;

    return flags;

invalid:

    *start = p + 1;

    return NJS_REGEXP_INVALID_FLAG;
}


njs_regexp_pattern_t *
njs_regexp_pattern_create(njs_vm_t *vm, u_char *start, size_t length,
    njs_regexp_flags_t flags)
{
    int                   options, ret;
    u_char                *p, *end;
    size_t                size;
    njs_str_t             text;
    njs_uint_t            n;
    njs_regex_t           *regex;
    njs_regexp_group_t    *group;
    njs_regexp_pattern_t  *pattern;

    size = 1;  /* A trailing "/". */
    size += ((flags & NJS_REGEXP_GLOBAL) != 0);
    size += ((flags & NJS_REGEXP_IGNORE_CASE) != 0);
    size += ((flags & NJS_REGEXP_MULTILINE) != 0);

    text.start = start;
    text.length = length;

    ret = njs_regexp_escape(vm, &text);
    if (njs_slow_path(ret != NJS_OK)) {
        return NULL;
    }

    pattern = njs_mp_zalloc(vm->mem_pool, sizeof(njs_regexp_pattern_t) + 1
                                          + text.length + size + 1);
    if (njs_slow_path(pattern == NULL)) {
        njs_memory_error(vm);
        return NULL;
    }

    pattern->flags = size;

    p = (u_char *) pattern + sizeof(njs_regexp_pattern_t);
    pattern->source = p;

    *p++ = '/';
    p = memcpy(p, text.start, text.length);
    p += text.length;
    end = p;
    *p++ = '\0';

    pattern->global = ((flags & NJS_REGEXP_GLOBAL) != 0);
    if (pattern->global) {
        *p++ = 'g';
    }

#ifdef PCRE_JAVASCRIPT_COMPAT
    /* JavaScript compatibility has been introduced in PCRE-7.7. */
    options = PCRE_JAVASCRIPT_COMPAT;
#else
    options = 0;
#endif

    pattern->ignore_case = ((flags & NJS_REGEXP_IGNORE_CASE) != 0);
    if (pattern->ignore_case) {
        *p++ = 'i';
         options |= PCRE_CASELESS;
    }

    pattern->multiline = ((flags & NJS_REGEXP_MULTILINE) != 0);
    if (pattern->multiline) {
        *p++ = 'm';
         options |= PCRE_MULTILINE;
    }

    *p++ = '\0';

    ret = njs_regexp_pattern_compile(vm, &pattern->regex[0],
                                     &pattern->source[1], options);

    if (njs_fast_path(ret >= 0)) {
        pattern->ncaptures = ret;

    } else if (ret < 0 && ret != NJS_DECLINED) {
        goto fail;
    }

    ret = njs_regexp_pattern_compile(vm, &pattern->regex[1],
                                     &pattern->source[1], options | PCRE_UTF8);
    if (njs_fast_path(ret >= 0)) {

        if (njs_slow_path(njs_regex_is_valid(&pattern->regex[0])
                          && (u_int) ret != pattern->ncaptures))
        {
            njs_internal_error(vm, "regexp pattern compile failed");
            goto fail;
        }

        pattern->ncaptures = ret;

    } else if (ret != NJS_DECLINED) {
        goto fail;
    }

    if (njs_regex_is_valid(&pattern->regex[0])) {
        regex = &pattern->regex[0];

    } else if (njs_regex_is_valid(&pattern->regex[1])) {
        regex = &pattern->regex[1];

    } else {
        goto fail;
    }

    *end = '/';

    pattern->ngroups = njs_regex_named_captures(regex, NULL, 0);

    if (pattern->ngroups != 0) {
        size = sizeof(njs_regexp_group_t) * pattern->ngroups;

        pattern->groups = njs_mp_alloc(vm->mem_pool, size);
        if (njs_slow_path(pattern->groups == NULL)) {
            njs_memory_error(vm);
            return NULL;
        }

        n = 0;

        do {
            group = &pattern->groups[n];

            group->capture = njs_regex_named_captures(regex, &group->name, n);
            group->hash = njs_djb_hash(group->name.start, group->name.length);

            n++;

        } while (n != pattern->ngroups);
    }

    njs_set_undefined(&vm->retval);

    return pattern;

fail:

    njs_mp_free(vm->mem_pool, pattern);
    return NULL;
}


static int
njs_regexp_pattern_compile(njs_vm_t *vm, njs_regex_t *regex, u_char *source,
    int options)
{
    njs_int_t            ret;
    njs_trace_handler_t  handler;

    handler = vm->trace.handler;
    vm->trace.handler = njs_regexp_compile_trace_handler;

    /* Zero length means a zero-terminated string. */
    ret = njs_regex_compile(regex, source, 0, options, vm->regex_context);

    vm->trace.handler = handler;

    if (njs_fast_path(ret == NJS_OK)) {
        return regex->ncaptures;
    }

    return ret;
}


static u_char *
njs_regexp_compile_trace_handler(njs_trace_t *trace, njs_trace_data_t *td,
    u_char *start)
{
    u_char    *p;
    njs_vm_t  *vm;

    vm = trace->data;

    trace = trace->next;
    p = trace->handler(trace, td, start);

    if (vm->parser != NULL && vm->parser->lexer != NULL) {
        njs_syntax_error(vm, "%*s in %uD", p - start, start,
                         vm->parser->lexer->line);

    } else {
        njs_syntax_error(vm, "%*s", p - start, start);
    }

    return p;
}


njs_int_t
njs_regexp_match(njs_vm_t *vm, njs_regex_t *regex, const u_char *subject,
    size_t len, njs_regex_match_data_t *match_data)
{
    njs_int_t            ret;
    njs_trace_handler_t  handler;

    handler = vm->trace.handler;
    vm->trace.handler = njs_regexp_match_trace_handler;

    ret = njs_regex_match(regex, subject, len, match_data, vm->regex_context);

    vm->trace.handler = handler;

    return ret;
}


static u_char *
njs_regexp_match_trace_handler(njs_trace_t *trace, njs_trace_data_t *td,
    u_char *start)
{
    u_char    *p;
    njs_vm_t  *vm;

    vm = trace->data;

    trace = trace->next;
    p = trace->handler(trace, td, start);

    njs_internal_error(vm, (const char *) start);

    return p;
}


njs_regexp_t *
njs_regexp_alloc(njs_vm_t *vm, njs_regexp_pattern_t *pattern)
{
    njs_regexp_t  *regexp;

    regexp = njs_mp_alloc(vm->mem_pool, sizeof(njs_regexp_t));

    if (njs_fast_path(regexp != NULL)) {
        njs_lvlhsh_init(&regexp->object.hash);
        regexp->object.shared_hash = vm->shared->regexp_instance_hash;
        regexp->object.__proto__ = &vm->prototypes[NJS_OBJ_TYPE_REGEXP].object;
        regexp->object.type = NJS_REGEXP;
        regexp->object.shared = 0;
        regexp->object.extensible = 1;
        regexp->object.fast_array = 0;
        regexp->object.error_data = 0;
        njs_set_number(&regexp->last_index, 0);
        regexp->pattern = pattern;
        njs_string_short_set(&regexp->string, 0, 0);
        return regexp;
    }

    njs_memory_error(vm);

    return NULL;
}


static njs_int_t
njs_regexp_prototype_last_index(njs_vm_t *vm, njs_object_prop_t *unused,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    uint32_t           index, last_index;
    njs_regexp_t       *regexp;
    njs_string_prop_t  string;

    regexp = njs_object_proto_lookup(njs_object(value), NJS_REGEXP,
                                     njs_regexp_t);
    if (njs_slow_path(regexp == NULL)) {
        njs_set_undefined(retval);
        return NJS_DECLINED;
    }

    if (setval != NULL) {
        regexp->last_index = *setval;
        *retval  = *setval;

        return NJS_OK;
    }

    if (njs_slow_path(!njs_is_number(&regexp->last_index))) {
        *retval = regexp->last_index;
        return NJS_OK;
    }

    (void) njs_string_prop(&string, &regexp->string);

    last_index = njs_number(&regexp->last_index);

    if (njs_slow_path(string.size < last_index)) {
        *retval = regexp->last_index;
        return NJS_OK;
    }

    index = njs_string_index(&string, last_index);
    njs_set_number(retval, index);

    return NJS_OK;
}


static njs_int_t
njs_regexp_prototype_global(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    njs_regexp_pattern_t  *pattern;

    pattern = njs_regexp_pattern(value);
    *retval = pattern->global ? njs_value_true : njs_value_false;
    njs_release(vm, value);

    return NJS_OK;
}


static njs_int_t
njs_regexp_prototype_ignore_case(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    njs_regexp_pattern_t  *pattern;

    pattern = njs_regexp_pattern(value);
    *retval = pattern->ignore_case ? njs_value_true : njs_value_false;
    njs_release(vm, value);

    return NJS_OK;
}


static njs_int_t
njs_regexp_prototype_multiline(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    njs_regexp_pattern_t  *pattern;

    pattern = njs_regexp_pattern(value);
    *retval = pattern->multiline ? njs_value_true : njs_value_false;
    njs_release(vm, value);

    return NJS_OK;
}


static njs_int_t
njs_regexp_prototype_source(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    u_char                *source;
    int32_t               length;
    uint32_t              size;
    njs_regexp_pattern_t  *pattern;

    pattern = njs_regexp_pattern(value);
    /* Skip starting "/". */
    source = pattern->source + 1;

    size = njs_strlen(source) - pattern->flags;
    length = njs_utf8_length(source, size);

    return njs_regexp_string_create(vm, retval, source, size, length);
}


static njs_int_t
njs_regexp_prototype_to_string(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t unused)
{
    if (njs_is_regexp(njs_arg(args, nargs, 0))) {
        return njs_regexp_to_string(vm, &vm->retval, &args[0]);
    }

    njs_type_error(vm, "\"this\" argument is not a regexp");

    return NJS_ERROR;
}


njs_int_t
njs_regexp_to_string(njs_vm_t *vm, njs_value_t *retval,
    const njs_value_t *value)
{
    u_char                *source;
    int32_t               length;
    uint32_t              size;
    njs_regexp_pattern_t  *pattern;

    pattern = njs_regexp_pattern(value);
    source = pattern->source;

    size = njs_strlen(source);
    length = njs_utf8_length(source, size);

    return njs_regexp_string_create(vm, retval, source, size, length);
}


static njs_int_t
njs_regexp_prototype_test(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    int                     *captures;
    uint64_t                last_index;
    njs_int_t               ret, match;
    njs_uint_t              n;
    njs_regex_t             *regex;
    njs_regexp_t            *regexp;
    njs_value_t             *value, lvalue;
    const njs_value_t       *retval;
    njs_string_prop_t       string;
    njs_regexp_pattern_t    *pattern;
    njs_regex_match_data_t  *match_data;

    if (!njs_is_regexp(njs_arg(args, nargs, 0))) {
        njs_type_error(vm, "\"this\" argument is not a regexp");
        return NJS_ERROR;
    }

    retval = &njs_value_false;

    value = njs_lvalue_arg(&lvalue, args, nargs, 1);

    if (!njs_is_string(value)) {
        ret = njs_value_to_string(vm, value, value);
        if (njs_slow_path(ret != NJS_OK)) {
            return ret;
        }
    }

    (void) njs_string_prop(&string, value);

    n = (string.length != 0);

    regexp = njs_regexp(njs_argument(args, 0));
    pattern = njs_regexp_pattern(&args[0]);

    regex = &pattern->regex[n];
    match_data = vm->single_match_data;

    if (njs_regex_is_valid(regex)) {
        if (njs_regex_backrefs(regex) != 0) {
            match_data = njs_regex_match_data(regex, vm->regex_context);
            if (njs_slow_path(match_data == NULL)) {
                njs_memory_error(vm);
                return NJS_ERROR;
            }
        }

        match = njs_regexp_match(vm, regex, string.start, string.size,
                               match_data);
        if (match >= 0) {
            retval = &njs_value_true;

        } else if (match != NJS_REGEX_NOMATCH) {
            ret = NJS_ERROR;
            goto done;
        }

        if (pattern->global) {
            ret = njs_value_to_length(vm, &regexp->last_index, &last_index);
            if (njs_slow_path(ret != NJS_OK)) {
                return NJS_ERROR;
            }

            if (match >= 0) {
                captures = njs_regex_captures(match_data);
                last_index += captures[1];

            } else {
                last_index = 0;
            }

            njs_set_number(&regexp->last_index, last_index);
        }
    }

    ret = NJS_OK;

    vm->retval = *retval;

done:

    if (match_data != vm->single_match_data) {
        njs_regex_match_data_free(match_data, vm->regex_context);
    }

    return ret;
}


njs_int_t
njs_regexp_prototype_exec(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    uint64_t                last_index;
    njs_int_t               ret;
    njs_utf8_t              utf8;
    njs_value_t             *value, lvalue;
    njs_regexp_t            *regexp;
    njs_string_prop_t       string;
    njs_regexp_utf8_t       type;
    njs_regexp_pattern_t    *pattern;
    njs_regex_match_data_t  *match_data;

    if (!njs_is_regexp(njs_arg(args, nargs, 0))) {
        njs_type_error(vm, "\"this\" argument is not a regexp");
        return NJS_ERROR;
    }

    value = njs_lvalue_arg(&lvalue, args, nargs, 1);

    if (!njs_is_string(value)) {
        ret = njs_value_to_string(vm, value, value);
        if (njs_slow_path(ret != NJS_OK)) {
            return ret;
        }
    }

    regexp = njs_regexp(&args[0]);
    regexp->string = *value;
    pattern = regexp->pattern;

    ret = njs_value_to_length(vm, &regexp->last_index, &last_index);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    if (!pattern->global) {
        last_index = 0;
    }

    (void) njs_string_prop(&string, value);

    if (string.size >= last_index) {
        utf8 = NJS_STRING_BYTE;
        type = NJS_REGEXP_BYTE;

        if (string.length != 0) {
            utf8 = NJS_STRING_ASCII;
            type = NJS_REGEXP_UTF8;

            if (string.length != string.size) {
                utf8 = NJS_STRING_UTF8;
            }
        }

        pattern = regexp->pattern;

        if (njs_regex_is_valid(&pattern->regex[type])) {
            string.start += last_index;
            string.size -= last_index;

            match_data = njs_regex_match_data(&pattern->regex[type],
                                              vm->regex_context);
            if (njs_slow_path(match_data == NULL)) {
                njs_memory_error(vm);
                return NJS_ERROR;
            }

            ret = njs_regexp_match(vm, &pattern->regex[type], string.start,
                                   string.size, match_data);
            if (ret >= 0) {
                return njs_regexp_exec_result(vm, regexp, utf8, string.start,
                                              match_data, last_index);
            }

            if (njs_slow_path(ret != NJS_REGEX_NOMATCH)) {
                njs_regex_match_data_free(match_data, vm->regex_context);

                return NJS_ERROR;
            }
        }
    }

    if (pattern->global) {
        njs_set_number(&regexp->last_index, 0);
    }

    vm->retval = njs_value_null;

    return NJS_OK;
}


static njs_int_t
njs_regexp_exec_result(njs_vm_t *vm, njs_regexp_t *regexp, njs_utf8_t utf8,
    u_char *string, njs_regex_match_data_t *match_data, uint32_t last_index)
{
    int                 *captures;
    u_char              *start;
    int32_t             size, length;
    njs_int_t           ret;
    njs_uint_t          i, n;
    njs_array_t         *array;
    njs_value_t         name;
    njs_object_t        *groups;
    njs_object_prop_t   *prop;
    njs_regexp_group_t  *group;
    njs_lvlhsh_query_t  lhq;

    static const njs_value_t  string_index = njs_string("index");
    static const njs_value_t  string_input = njs_string("input");
    static const njs_value_t  string_groups = njs_string("groups");

    array = njs_array_alloc(vm, 0, regexp->pattern->ncaptures, 0);
    if (njs_slow_path(array == NULL)) {
        goto fail;
    }

    captures = njs_regex_captures(match_data);

    for (i = 0; i < regexp->pattern->ncaptures; i++) {
        n = 2 * i;

        if (captures[n] != -1) {
            start = &string[captures[n]];
            size = captures[n + 1] - captures[n];

            length = njs_string_calc_length(utf8, start, size);

            ret = njs_regexp_string_create(vm, &array->start[i], start, size,
                                           length);
            if (njs_slow_path(ret != NJS_OK)) {
                goto fail;
            }

        } else {
            njs_set_undefined(&array->start[i]);
        }
    }

    prop = njs_object_prop_alloc(vm, &string_index, &njs_value_undefined, 1);
    if (njs_slow_path(prop == NULL)) {
        goto fail;
    }

    /* TODO: Non UTF-8 position */

    njs_set_number(&prop->value, last_index + captures[0]);

    if (regexp->pattern->global) {
        njs_set_number(&regexp->last_index, last_index + captures[1]);
    }

    lhq.key_hash = NJS_INDEX_HASH;
    lhq.key = njs_str_value("index");
    lhq.replace = 0;
    lhq.value = prop;
    lhq.pool = vm->mem_pool;
    lhq.proto = &njs_object_hash_proto;

    ret = njs_lvlhsh_insert(&array->object.hash, &lhq);
    if (njs_slow_path(ret != NJS_OK)) {
        goto insert_fail;
    }

    prop = njs_object_prop_alloc(vm, &string_input, &regexp->string, 1);
    if (njs_slow_path(prop == NULL)) {
        goto fail;
    }

    lhq.key_hash = NJS_INPUT_HASH;
    lhq.key = njs_str_value("input");
    lhq.value = prop;

    ret = njs_lvlhsh_insert(&array->object.hash, &lhq);
    if (njs_slow_path(ret != NJS_OK)) {
        goto insert_fail;
    }

    prop = njs_object_prop_alloc(vm, &string_groups, &njs_value_undefined, 1);
    if (njs_slow_path(prop == NULL)) {
        goto fail;
    }

    lhq.key_hash = NJS_GROUPS_HASH;
    lhq.key = njs_str_value("groups");
    lhq.value = prop;

    ret = njs_lvlhsh_insert(&array->object.hash, &lhq);
    if (njs_slow_path(ret != NJS_OK)) {
        goto insert_fail;
    }

    if (regexp->pattern->ngroups != 0) {
        groups = njs_object_alloc(vm);
        if (njs_slow_path(groups == NULL)) {
            goto fail;
        }

        njs_set_object(&prop->value, groups);

        i = 0;

        do {
            group = &regexp->pattern->groups[i];

            ret = njs_string_set(vm, &name, group->name.start,
                                 group->name.length);
            if (njs_slow_path(ret != NJS_OK)) {
                goto fail;
            }

            prop = njs_object_prop_alloc(vm, &name,
                                         &array->start[group->capture], 1);
            if (njs_slow_path(prop == NULL)) {
                goto fail;
            }

            lhq.key_hash = group->hash;
            lhq.key = group->name;
            lhq.value = prop;

            ret = njs_lvlhsh_insert(&groups->hash, &lhq);
            if (njs_slow_path(ret != NJS_OK)) {
                goto insert_fail;
            }

            i++;

        } while (i < regexp->pattern->ngroups);
    }

    njs_set_array(&vm->retval, array);

    ret = NJS_OK;
    goto done;

insert_fail:

    njs_internal_error(vm, "lvlhsh insert failed");

fail:

    ret = NJS_ERROR;

done:

    njs_regex_match_data_free(match_data, vm->regex_context);

    return ret;
}


static njs_int_t
njs_regexp_string_create(njs_vm_t *vm, njs_value_t *value, u_char *start,
    uint32_t size, int32_t length)
{
    length = (length >= 0) ? length : 0;

    return njs_string_new(vm, value, start, size, length);
}


static const njs_object_prop_t  njs_regexp_constructor_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("RegExp"),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("length"),
        .value = njs_value(NJS_NUMBER, 1, 2.0),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("prototype"),
        .value = njs_prop_handler(njs_object_prototype_create),
    },
};


const njs_object_init_t  njs_regexp_constructor_init = {
    njs_regexp_constructor_properties,
    njs_nitems(njs_regexp_constructor_properties),
};


static const njs_object_prop_t  njs_regexp_prototype_properties[] =
{
    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("constructor"),
        .value = njs_prop_handler(njs_object_prototype_create_constructor),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("global"),
        .value = njs_prop_handler(njs_regexp_prototype_global),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("ignoreCase"),
        .value = njs_prop_handler(njs_regexp_prototype_ignore_case),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("multiline"),
        .value = njs_prop_handler(njs_regexp_prototype_multiline),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("source"),
        .value = njs_prop_handler(njs_regexp_prototype_source),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("toString"),
        .value = njs_native_function(njs_regexp_prototype_to_string, 0),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("test"),
        .value = njs_native_function(njs_regexp_prototype_test, 1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("exec"),
        .value = njs_native_function(njs_regexp_prototype_exec, 1),
        .writable = 1,
        .configurable = 1,
    },
};


const njs_object_prop_t  njs_regexp_instance_properties[] =
{
    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("lastIndex"),
        .value = njs_prop_handler(njs_regexp_prototype_last_index),
        .writable = 1,
    },
};


const njs_object_init_t  njs_regexp_instance_init = {
    njs_regexp_instance_properties,
    njs_nitems(njs_regexp_instance_properties),
};


const njs_object_init_t  njs_regexp_prototype_init = {
    njs_regexp_prototype_properties,
    njs_nitems(njs_regexp_prototype_properties),
};


const njs_object_type_init_t  njs_regexp_type_init = {
   .constructor = njs_native_ctor(njs_regexp_constructor, 2, 0),
   .constructor_props = &njs_regexp_constructor_init,
   .prototype_props = &njs_regexp_prototype_init,
   .prototype_value = { .object = { .type = NJS_REGEXP } },
};
