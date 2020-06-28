
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */


#include <njs_main.h>


static njs_int_t njs_vm_init(njs_vm_t *vm);
static njs_int_t njs_vm_handle_events(njs_vm_t *vm);


const njs_str_t  njs_entry_main =           njs_str("main");
const njs_str_t  njs_entry_module =         njs_str("module");
const njs_str_t  njs_entry_native =         njs_str("native");
const njs_str_t  njs_entry_unknown =        njs_str("unknown");
const njs_str_t  njs_entry_anonymous =      njs_str("anonymous");


void
njs_vm_opt_init(njs_vm_opt_t *options)
{
    njs_memzero(options, sizeof(njs_vm_opt_t));
}


njs_vm_t *
njs_vm_create(njs_vm_opt_t *options)
{
    njs_mp_t   *mp;
    njs_vm_t   *vm;
    njs_int_t  ret;

    mp = njs_mp_fast_create(2 * njs_pagesize(), 128, 512, 16);
    if (njs_slow_path(mp == NULL)) {
        return NULL;
    }

    vm = njs_mp_zalign(mp, sizeof(njs_value_t), sizeof(njs_vm_t));
    if (njs_slow_path(vm == NULL)) {
        return NULL;
    }

    vm->mem_pool = mp;

    ret = njs_regexp_init(vm);
    if (njs_slow_path(ret != NJS_OK)) {
        return NULL;
    }

    njs_lvlhsh_init(&vm->values_hash);

    vm->options = *options;

    if (options->shared != NULL) {
        vm->shared = options->shared;

    } else {
        ret = njs_builtin_objects_create(vm);
        if (njs_slow_path(ret != NJS_OK)) {
            return NULL;
        }
    }

    vm->external = options->external;

    vm->trace.level = NJS_LEVEL_TRACE;
    vm->trace.size = 2048;
    vm->trace.handler = njs_parser_trace_handler;
    vm->trace.data = vm;

    njs_set_undefined(&vm->retval);

    if (options->init) {
        ret = njs_vm_init(vm);
        if (njs_slow_path(ret != NJS_OK)) {
            return NULL;
        }
    }

    vm->symbol_generator = NJS_SYMBOL_KNOWN_MAX;

    return vm;
}


void
njs_vm_destroy(njs_vm_t *vm)
{
    njs_event_t        *event;
    njs_lvlhsh_each_t  lhe;

    if (njs_waiting_events(vm)) {
        njs_lvlhsh_each_init(&lhe, &njs_event_hash_proto);

        for ( ;; ) {
            event = njs_lvlhsh_each(&vm->events_hash, &lhe);

            if (event == NULL) {
                break;
            }

            njs_del_event(vm, event, NJS_EVENT_RELEASE);
        }
    }

    njs_mp_destroy(vm->mem_pool);
}


njs_int_t
njs_vm_compile(njs_vm_t *vm, u_char **start, u_char *end)
{
    njs_int_t           ret;
    njs_str_t           ast;
    njs_chb_t           chain;
    njs_lexer_t         lexer;
    njs_parser_t        *parser, *prev;
    njs_vm_code_t       *code;
    njs_generator_t     generator;
    njs_parser_scope_t  *scope;

    if (vm->parser != NULL && !vm->options.accumulative) {
        return NJS_ERROR;
    }

    if (vm->modules != NULL && vm->options.accumulative) {
        njs_module_reset(vm);
    }

    parser = njs_mp_zalloc(vm->mem_pool, sizeof(njs_parser_t));
    if (njs_slow_path(parser == NULL)) {
        return NJS_ERROR;
    }

    prev = vm->parser;
    vm->parser = parser;

    ret = njs_lexer_init(vm, &lexer, &vm->options.file, *start, end);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    parser->vm = vm;
    parser->lexer = &lexer;

    njs_set_undefined(&vm->retval);

    ret = njs_parser(parser, prev);
    if (njs_slow_path(ret != NJS_OK)) {
        goto fail;
    }

    parser->lexer = NULL;

    scope = parser->scope;

    ret = njs_variables_scope_reference(vm, scope);
    if (njs_slow_path(ret != NJS_OK)) {
        goto fail;
    }

    *start = lexer.start;

    njs_memzero(&generator, sizeof(njs_generator_t));

    code = njs_generate_scope(vm, &generator, scope, &njs_entry_main);
    if (njs_slow_path(code == NULL)) {
        if (!njs_is_error(&vm->retval)) {
            njs_internal_error(vm, "njs_generate_scope() failed");
        }

        goto fail;
    }

    vm->main_index = code - (njs_vm_code_t *) vm->codes->start;
    vm->start = generator.code_start;
    vm->global_scope = generator.local_scope;
    vm->scope_size = generator.scope_size;

    vm->variables_hash = &scope->variables;

    if (vm->options.init && !vm->options.accumulative) {
        ret = njs_vm_init(vm);
        if (njs_slow_path(ret != NJS_OK)) {
            return ret;
        }
    }

    if (vm->options.disassemble) {
        njs_disassembler(vm);
    }

    if (njs_slow_path(vm->options.ast)) {
        njs_chb_init(&chain, vm->mem_pool);
        ret = njs_parser_serialize_ast(parser->node, &chain);
        if (njs_slow_path(ret == NJS_ERROR)) {
            return ret;
        }

        if (njs_slow_path(njs_chb_join(&chain, &ast) != NJS_OK)) {
            return NJS_ERROR;
        }

        njs_print(ast.start, ast.length);

        njs_chb_destroy(&chain);
        njs_mp_free(vm->mem_pool, ast.start);
    }

    return ret;

fail:

    vm->parser = prev;

    return NJS_ERROR;
}


njs_vm_t *
njs_vm_clone(njs_vm_t *vm, njs_external_ptr_t external)
{
    njs_mp_t   *nmp;
    njs_vm_t   *nvm;
    njs_int_t  ret;

    njs_thread_log_debug("CLONE:");

    if (vm->options.accumulative) {
        return NULL;
    }

    nmp = njs_mp_fast_create(2 * njs_pagesize(), 128, 512, 16);
    if (njs_slow_path(nmp == NULL)) {
        return NULL;
    }

    nvm = njs_mp_align(nmp, sizeof(njs_value_t), sizeof(njs_vm_t));
    if (njs_slow_path(nvm == NULL)) {
        goto fail;
    }

    *nvm = *vm;

    nvm->mem_pool = nmp;
    nvm->trace.data = nvm;
    nvm->external = external;

    ret = njs_vm_init(nvm);
    if (njs_slow_path(ret != NJS_OK)) {
        goto fail;
    }

    return nvm;

fail:

    njs_mp_destroy(nmp);

    return NULL;
}


static njs_int_t
njs_vm_init(njs_vm_t *vm)
{
    size_t       size, scope_size;
    u_char       *values;
    njs_int_t    ret;
    njs_value_t  *global;
    njs_frame_t  *frame;

    scope_size = vm->scope_size + NJS_INDEX_GLOBAL_OFFSET;

    size = njs_frame_size(0) + scope_size + NJS_FRAME_SPARE_SIZE;
    size = njs_align_size(size, NJS_FRAME_SPARE_SIZE);

    frame = njs_mp_align(vm->mem_pool, sizeof(njs_value_t), size);
    if (njs_slow_path(frame == NULL)) {
        return NJS_ERROR;
    }

    njs_memzero(frame, njs_frame_size(0));

    vm->top_frame = &frame->native;
    vm->active_frame = frame;

    frame->native.size = size;
    frame->native.free_size = size - (njs_frame_size(0) + scope_size);

    values = (u_char *) frame + njs_frame_size(0);

    frame->native.free = values + scope_size;

    vm->scopes[NJS_SCOPE_GLOBAL] = (njs_value_t *) values;

    memcpy(values + NJS_INDEX_GLOBAL_OFFSET, vm->global_scope, vm->scope_size);

    ret = njs_regexp_init(vm);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    global = (njs_value_t *) (values + NJS_INDEX_GLOBAL_OBJECT_OFFSET);

    ret = njs_builtin_objects_clone(vm, global);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    njs_lvlhsh_init(&vm->modules_hash);

    njs_lvlhsh_init(&vm->events_hash);
    njs_queue_init(&vm->posted_events);
    njs_queue_init(&vm->promise_events);

    return NJS_OK;
}


njs_int_t
njs_vm_call(njs_vm_t *vm, njs_function_t *function, const njs_value_t *args,
    njs_uint_t nargs)
{
    return njs_vm_invoke(vm, function, args, nargs, (njs_index_t) &vm->retval);
}


njs_int_t
njs_vm_invoke(njs_vm_t *vm, njs_function_t *function, const njs_value_t *args,
    njs_uint_t nargs, njs_index_t retval)
{
    njs_int_t  ret;

    ret = njs_function_frame(vm, function, &njs_value_undefined, args, nargs,
                             0);
    if (njs_slow_path(ret != NJS_OK)) {
        return ret;
    }

    return njs_function_frame_invoke(vm, retval);
}


void
njs_vm_scopes_restore(njs_vm_t *vm, njs_native_frame_t *native,
    njs_native_frame_t *previous)
{
    njs_uint_t      n, nesting;
    njs_value_t     *args;
    njs_frame_t     *frame;
    njs_closure_t   **closures;
    njs_function_t  *function;

    vm->top_frame = previous;

    args = previous->arguments;
    function = previous->function;

    if (function != NULL) {
        args += function->args_offset;
    }

    vm->scopes[NJS_SCOPE_CALLEE_ARGUMENTS] = args;

    function = native->function;

    if (function->native) {
        return;
    }

    if (function->closure) {
        /* GC: release function closures. */
    }

    frame = (njs_frame_t *) native;
    frame = frame->previous_active_frame;
    vm->active_frame = frame;

    /* GC: arguments, local, and local block closures. */

    vm->scopes[NJS_SCOPE_ARGUMENTS] = frame->native.arguments;
    vm->scopes[NJS_SCOPE_LOCAL] = frame->local;

    function = frame->native.function;

    nesting = (function != NULL) ? function->u.lambda->nesting : 0;
    closures = njs_frame_closures(frame);

    for (n = 0; n <= nesting; n++) {
        vm->scopes[NJS_SCOPE_CLOSURE + n] = &closures[n]->u.values;
    }

    while (n < NJS_MAX_NESTING) {
        vm->scopes[NJS_SCOPE_CLOSURE + n] = NULL;
        n++;
    }
}


njs_vm_event_t
njs_vm_add_event(njs_vm_t *vm, njs_function_t *function, njs_uint_t once,
    njs_host_event_t host_ev, njs_event_destructor_t destructor)
{
    njs_event_t  *event;

    event = njs_mp_alloc(vm->mem_pool, sizeof(njs_event_t));
    if (njs_slow_path(event == NULL)) {
        return NULL;
    }

    event->host_event = host_ev;
    event->destructor = destructor;
    event->function = function;
    event->once = once;
    event->posted = 0;
    event->nargs = 0;
    event->args = NULL;

    if (njs_add_event(vm, event) != NJS_OK) {
        return NULL;
    }

    return event;
}


void
njs_vm_del_event(njs_vm_t *vm, njs_vm_event_t vm_event)
{
    njs_event_t  *event;

    event = (njs_event_t *) vm_event;

    njs_del_event(vm, event, NJS_EVENT_RELEASE | NJS_EVENT_DELETE);
}


njs_int_t
njs_vm_waiting(njs_vm_t *vm)
{
    return njs_waiting_events(vm);
}


njs_int_t
njs_vm_posted(njs_vm_t *vm)
{
    return njs_posted_events(vm) || njs_promise_events(vm);
}


njs_int_t
njs_vm_post_event(njs_vm_t *vm, njs_vm_event_t vm_event,
    const njs_value_t *args, njs_uint_t nargs)
{
    njs_event_t  *event;

    event = (njs_event_t *) vm_event;

    if (nargs != 0 && !event->posted) {
        event->nargs = nargs;
        event->args = njs_mp_alloc(vm->mem_pool, sizeof(njs_value_t) * nargs);
        if (njs_slow_path(event->args == NULL)) {
            return NJS_ERROR;
        }

        memcpy(event->args, args, sizeof(njs_value_t) * nargs);
    }

    if (!event->posted) {
        event->posted = 1;
        njs_queue_insert_tail(&vm->posted_events, &event->link);
    }

    return NJS_OK;
}


njs_int_t
njs_vm_run(njs_vm_t *vm)
{
    return njs_vm_handle_events(vm);
}


njs_int_t
njs_vm_start(njs_vm_t *vm)
{
    njs_int_t  ret;

    ret = njs_module_load(vm);
    if (njs_slow_path(ret != NJS_OK)) {
        return ret;
    }

    return njs_vmcode_interpreter(vm, vm->start);
}


static njs_int_t
njs_vm_handle_events(njs_vm_t *vm)
{
    njs_int_t         ret;
    njs_event_t       *ev;
    njs_queue_t       *promise_events, *posted_events;
    njs_queue_link_t  *link;

    promise_events = &vm->promise_events;
    posted_events = &vm->posted_events;

    do {
        for ( ;; ) {
            link = njs_queue_first(promise_events);

            if (link == njs_queue_tail(promise_events)) {
                break;
            }

            ev = njs_queue_link_data(link, njs_event_t, link);

            njs_queue_remove(&ev->link);

            ret = njs_vm_call(vm, ev->function, ev->args, ev->nargs);
            if (njs_slow_path(ret == NJS_ERROR)) {
                return ret;
            }
        }

        for ( ;; ) {
            link = njs_queue_first(posted_events);

            if (link == njs_queue_tail(posted_events)) {
                break;
            }

            ev = njs_queue_link_data(link, njs_event_t, link);

            if (ev->once) {
                njs_del_event(vm, ev, NJS_EVENT_RELEASE | NJS_EVENT_DELETE);

            } else {
                ev->posted = 0;
                njs_queue_remove(&ev->link);
            }

            ret = njs_vm_call(vm, ev->function, ev->args, ev->nargs);

            if (ret == NJS_ERROR) {
                return ret;
            }
        }

    } while (!njs_queue_is_empty(promise_events));

    return njs_posted_events(vm) ? NJS_AGAIN : NJS_OK;
}


njs_int_t
njs_vm_add_path(njs_vm_t *vm, const njs_str_t *path)
{
    njs_str_t  *item;

    if (vm->paths == NULL) {
        vm->paths = njs_arr_create(vm->mem_pool, 4, sizeof(njs_str_t));
        if (njs_slow_path(vm->paths == NULL)) {
            return NJS_ERROR;
        }
    }

    item = njs_arr_add(vm->paths);
    if (njs_slow_path(item == NULL)) {
        return NJS_ERROR;
    }

    *item = *path;

    return NJS_OK;
}


njs_value_t *
njs_vm_retval(njs_vm_t *vm)
{
    return &vm->retval;
}


void
njs_vm_retval_set(njs_vm_t *vm, const njs_value_t *value)
{
    vm->retval = *value;
}


njs_int_t
njs_vm_value(njs_vm_t *vm, const njs_str_t *path, njs_value_t *retval)
{
    u_char       *start, *p, *end;
    size_t       size;
    njs_int_t    ret;
    njs_value_t  value, key;

    start = path->start;
    end = start + path->length;

    njs_set_object(&value, &vm->global_object);

    for ( ;; ) {
        p = njs_strlchr(start, end, '.');

        size = ((p != NULL) ? p : end) - start;
        if (njs_slow_path(size == 0)) {
            njs_type_error(vm, "empty path element");
            return NJS_ERROR;
        }

        ret = njs_string_set(vm, &key, start, size);
        if (njs_slow_path(ret != NJS_OK)) {
            return NJS_ERROR;
        }

        ret = njs_value_property(vm, &value, &key, njs_value_arg(retval));
        if (njs_slow_path(ret != NJS_OK)) {
            return ret;
        }

        if (p == NULL) {
            break;
        }

        start = p + 1;
        value = *retval;
    }

    return NJS_OK;
}


njs_int_t
njs_vm_bind(njs_vm_t *vm, const njs_str_t *var_name, const njs_value_t *value,
    njs_bool_t shared)
{
    njs_int_t           ret;
    njs_object_t        *global;
    njs_lvlhsh_t        *hash;
    njs_object_prop_t   *prop;
    njs_lvlhsh_query_t  lhq;

    prop = njs_object_prop_alloc(vm, &njs_value_undefined, value, 1);
    if (njs_slow_path(prop == NULL)) {
        return NJS_ERROR;
    }

    ret = njs_string_new(vm, &prop->name, var_name->start, var_name->length, 0);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    lhq.value = prop;
    lhq.key = *var_name;
    lhq.key_hash = njs_djb_hash(lhq.key.start, lhq.key.length);
    lhq.replace = 1;
    lhq.pool = vm->mem_pool;
    lhq.proto = &njs_object_hash_proto;

    global = &vm->global_object;
    hash = shared ? &global->shared_hash : &global->hash;

    ret = njs_lvlhsh_insert(hash, &lhq);
    if (njs_slow_path(ret != NJS_OK)) {
        njs_internal_error(vm, "lvlhsh insert failed");
        return ret;
    }

    return NJS_OK;
}


void
njs_value_string_get(njs_value_t *value, njs_str_t *dst)
{
    njs_string_get(value, dst);
}


njs_int_t
njs_vm_value_string_set(njs_vm_t *vm, njs_value_t *value, const u_char *start,
    uint32_t size)
{
    return njs_string_set(vm, value, start, size);
}


u_char *
njs_vm_value_string_alloc(njs_vm_t *vm, njs_value_t *value, uint32_t size)
{
    return njs_string_alloc(vm, value, size, 0);
}


njs_function_t *
njs_vm_function(njs_vm_t *vm, const njs_str_t *path)
{
    njs_int_t    ret;
    njs_value_t  retval;

    ret = njs_vm_value(vm, path, &retval);
    if (njs_slow_path(ret != NJS_OK || !njs_is_function(&retval))) {
        return NULL;
    }

    return njs_function(&retval);
}


uint16_t
njs_vm_prop_magic16(njs_object_prop_t *prop)
{
    return prop->value.data.magic16;
}


uint32_t
njs_vm_prop_magic32(njs_object_prop_t *prop)
{
    return prop->value.data.magic32;
}


njs_int_t
njs_vm_prop_name(njs_vm_t *vm, njs_object_prop_t *prop, njs_str_t *dst)
{
    if (njs_slow_path(!njs_is_string(&prop->name))) {
        njs_type_error(vm, "property name is not a string");
        return NJS_ERROR;
    }

    njs_string_get(&prop->name, dst);

    return NJS_OK;
}


njs_noinline void
njs_vm_value_error_set(njs_vm_t *vm, njs_value_t *value, const char *fmt, ...)
{
    va_list  args;
    u_char   buf[NJS_MAX_ERROR_STR], *p;

    p = buf;

    if (fmt != NULL) {
        va_start(args, fmt);
        p = njs_vsprintf(buf, buf + sizeof(buf), fmt, args);
        va_end(args);
    }

    njs_error_new(vm, value, NJS_OBJ_TYPE_ERROR, buf, p - buf);
}


njs_noinline void
njs_vm_memory_error(njs_vm_t *vm)
{
    njs_memory_error_set(vm, &vm->retval);
}


njs_int_t
njs_vm_value_string(njs_vm_t *vm, njs_str_t *dst, njs_value_t *src)
{
    njs_int_t    ret;
    njs_uint_t   exception;

    if (njs_slow_path(src->type == NJS_NUMBER
                      && njs_number(src) == 0
                      && signbit(njs_number(src))))
    {
        njs_string_get(&njs_string_minus_zero, dst);
        return NJS_OK;
    }

    exception = 0;

again:

    ret = njs_vm_value_to_string(vm, dst, src);
    if (njs_fast_path(ret == NJS_OK)) {
        return NJS_OK;
    }

    if (!exception) {
        exception = 1;

        /* value evaluation threw an exception. */

        src = &vm->retval;
        goto again;
    }

    dst->length = 0;
    dst->start = NULL;

    return NJS_ERROR;
}


njs_int_t
njs_vm_retval_string(njs_vm_t *vm, njs_str_t *dst)
{
    if (vm->top_frame == NULL) {
        /* An exception was thrown during compilation. */

        njs_vm_init(vm);
    }

    return njs_vm_value_string(vm, dst, &vm->retval);
}


njs_int_t
njs_vm_retval_dump(njs_vm_t *vm, njs_str_t *dst, njs_uint_t indent)
{
    if (vm->top_frame == NULL) {
        /* An exception was thrown during compilation. */

        njs_vm_init(vm);
    }

    return njs_vm_value_dump(vm, dst, &vm->retval, 0, 1);
}


njs_int_t
njs_vm_object_alloc(njs_vm_t *vm, njs_value_t *retval, ...)
{
    va_list             args;
    njs_int_t           ret;
    njs_value_t         *name, *value;
    njs_object_t        *object;
    njs_object_prop_t   *prop;
    njs_lvlhsh_query_t  lhq;

    object = njs_object_alloc(vm);
    if (njs_slow_path(object == NULL)) {
        return NJS_ERROR;
    }

    ret = NJS_ERROR;

    va_start(args, retval);

    for ( ;; ) {
        name = va_arg(args, njs_value_t *);
        if (name == NULL) {
            break;
        }

        value = va_arg(args, njs_value_t *);
        if (value == NULL) {
            njs_type_error(vm, "missed value for a key");
            goto done;
        }

        if (njs_slow_path(!njs_is_string(name))) {
            njs_type_error(vm, "prop name is not a string");
            goto done;
        }

        lhq.replace = 0;
        lhq.pool = vm->mem_pool;
        lhq.proto = &njs_object_hash_proto;

        njs_string_get(name, &lhq.key);
        lhq.key_hash = njs_djb_hash(lhq.key.start, lhq.key.length);

        prop = njs_object_prop_alloc(vm, name, value, 1);
        if (njs_slow_path(prop == NULL)) {
            goto done;
        }

        lhq.value = prop;

        ret = njs_lvlhsh_insert(&object->hash, &lhq);
        if (njs_slow_path(ret != NJS_OK)) {
            njs_internal_error(vm, NULL);
            goto done;
        }
    }

    ret = NJS_OK;

    njs_set_object(retval, object);

done:

    va_end(args);

    return ret;
}


njs_int_t
njs_vm_array_alloc(njs_vm_t *vm, njs_value_t *retval, uint32_t spare)
{
    njs_array_t  *array;

    array = njs_array_alloc(vm, 1, 0, spare);

    if (njs_slow_path(array == NULL)) {
        return NJS_ERROR;
    }

    njs_set_array(retval, array);

    return NJS_OK;
}


njs_value_t *
njs_vm_array_push(njs_vm_t *vm, njs_value_t *value)
{
    if (njs_slow_path(!njs_is_array(value))) {
        njs_type_error(vm, "njs_vm_array_push() argument is not array");
        return NULL;
    }

    return njs_array_push(vm, njs_array(value));
}


njs_value_t *
njs_vm_object_prop(njs_vm_t *vm, njs_value_t *value, const njs_str_t *prop,
    njs_opaque_value_t *retval)
{
    njs_int_t    ret;
    njs_value_t  key;

    if (njs_slow_path(!njs_is_object(value))) {
        njs_type_error(vm, "njs_vm_object_prop() argument is not object");
        return NULL;
    }

    ret = njs_vm_value_string_set(vm, &key, prop->start, prop->length);
    if (njs_slow_path(ret != NJS_OK)) {
        return NULL;
    }

    ret = njs_value_property(vm, value, &key, njs_value_arg(retval));
    if (njs_slow_path(ret != NJS_OK)) {
        return NULL;
    }

    return njs_value_arg(retval);
}


njs_value_t *
njs_vm_array_prop(njs_vm_t *vm, njs_value_t *value, int64_t index,
    njs_opaque_value_t *retval)
{
    njs_int_t    ret;
    njs_array_t  *array;

    if (njs_slow_path(!njs_is_object(value))) {
        njs_type_error(vm, "njs_vm_array_prop() argument is not object");
        return NULL;
    }

    if (njs_fast_path(njs_is_fast_array(value))) {
        array = njs_array(value);

        if (index >= 0 && index < array->length) {
            return &array->start[index];
        }

        return NULL;
    }

    ret = njs_value_property_i64(vm, value, index, njs_value_arg(retval));
    if (njs_slow_path(ret != NJS_OK)) {
        return NULL;
    }

    return njs_value_arg(retval);
}


njs_value_t *
njs_vm_array_start(njs_vm_t *vm, njs_value_t *value)
{
    if (njs_slow_path(!njs_is_fast_array(value))) {
        njs_type_error(vm, "njs_vm_array_start() argument is not a fast array");
        return NULL;
    }

    return njs_array(value)->start;
}


njs_int_t
njs_vm_array_length(njs_vm_t *vm, njs_value_t *value, int64_t *length)
{
    if (njs_fast_path(njs_is_array(value))) {
        *length = njs_array(value)->length;
    }

    return njs_object_length(vm, value, length);
}


njs_int_t
njs_vm_value_to_string(njs_vm_t *vm, njs_str_t *dst, njs_value_t *src)
{
    u_char       *start;
    size_t       size;
    njs_int_t    ret;
    njs_value_t  value, stack;

    if (njs_slow_path(src == NULL)) {
        return NJS_ERROR;
    }

    if (njs_is_error(src)) {
        if (njs_is_memory_error(vm, src)) {
            njs_string_get(&njs_string_memory_error, dst);
            return NJS_OK;
        }

        ret = njs_error_stack(vm, src, &stack);
        if (njs_slow_path(ret == NJS_ERROR)) {
            return ret;
        }

        if (ret == NJS_OK) {
            src = &stack;
        }
    }

    value = *src;

    ret = njs_value_to_string(vm, &value, &value);

    if (njs_fast_path(ret == NJS_OK)) {
        size = value.short_string.size;

        if (size != NJS_STRING_LONG) {
            start = njs_mp_alloc(vm->mem_pool, size);
            if (njs_slow_path(start == NULL)) {
                njs_memory_error(vm);
                return NJS_ERROR;
            }

            memcpy(start, value.short_string.start, size);

        } else {
            size = value.long_string.size;
            start = value.long_string.data->start;
        }

        dst->length = size;
        dst->start = start;
    }

    return ret;
}


njs_int_t
njs_vm_value_string_copy(njs_vm_t *vm, njs_str_t *retval,
    njs_value_t *value, uintptr_t *next)
{
    uintptr_t    n;
    njs_array_t  *array;

    switch (value->type) {

    case NJS_STRING:
        if (*next != 0) {
            return NJS_DECLINED;
        }

        *next = 1;
        break;

    case NJS_ARRAY:
        array = njs_array(value);

        do {
            n = (*next)++;

            if (n == array->length) {
                return NJS_DECLINED;
            }

            value = &array->start[n];

        } while (!njs_is_valid(value));

        break;

    default:
        return NJS_ERROR;
    }

    return njs_vm_value_to_string(vm, retval, value);
}


void *
njs_lvlhsh_alloc(void *data, size_t size)
{
    return njs_mp_align(data, size, size);
}


void
njs_lvlhsh_free(void *data, void *p, size_t size)
{
    njs_mp_free(data, p);
}
