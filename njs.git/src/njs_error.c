
/*
 * Copyright (C) Dmitry Volyntsev
 * Copyright (C) NGINX, Inc.
 */


#include <njs_main.h>


static const njs_value_t  njs_error_message_string = njs_string("message");
static const njs_value_t  njs_error_name_string = njs_string("name");
static const njs_value_t  njs_error_stack_string = njs_string("stack");


void
njs_error_new(njs_vm_t *vm, njs_value_t *dst, njs_object_type_t type,
    u_char *start, size_t size)
{
    ssize_t        length;
    njs_int_t     ret;
    njs_value_t   string;
    njs_object_t  *error;

    length = njs_utf8_length(start, size);
    if (njs_slow_path(length < 0)) {
        length = 0;
    }

    ret = njs_string_new(vm, &string, start, size, length);
    if (njs_slow_path(ret != NJS_OK)) {
        return;
    }

    error = njs_error_alloc(vm, type, NULL, &string);
    if (njs_slow_path(error == NULL)) {
        return;
    }

    njs_set_object(dst, error);
}


void
njs_error_fmt_new(njs_vm_t *vm, njs_value_t *dst, njs_object_type_t type,
    const char *fmt, ...)
{
    va_list  args;
    u_char   buf[NJS_MAX_ERROR_STR], *p;

    p = buf;

    if (fmt != NULL) {
        va_start(args, fmt);
        p = njs_vsprintf(buf, buf + sizeof(buf), fmt, args);
        va_end(args);
    }

    njs_error_new(vm, dst, type, buf, p - buf);
}


static njs_int_t
njs_error_stack_new(njs_vm_t *vm, njs_object_t *error, njs_value_t *retval)
{
    njs_int_t           ret;
    njs_str_t           string;
    njs_arr_t           *stack;
    njs_value_t         value;
    njs_native_frame_t  *frame;

    njs_set_object(&value, error);

    ret = njs_error_to_string(vm, retval, &value);
    if (njs_slow_path(ret != NJS_OK)) {
        return ret;
    }

    stack = njs_arr_create(vm->mem_pool, 4, sizeof(njs_backtrace_entry_t));
    if (njs_slow_path(stack == NULL)) {
        return NJS_ERROR;
    }

    frame = vm->top_frame;

    for ( ;; ) {
        if (njs_vm_add_backtrace_entry(vm, stack, frame) != NJS_OK) {
            break;
        }

        frame = frame->previous;

        if (frame == NULL) {
            break;
        }
    }

    njs_string_get(retval, &string);

    ret = njs_vm_backtrace_to_string(vm, stack, &string);

    njs_arr_destroy(stack);

    if (njs_slow_path(ret != NJS_OK)) {
        return ret;
    }

    return njs_string_set(vm, retval, string.start, string.length);
}


njs_int_t
njs_error_stack_attach(njs_vm_t *vm, njs_value_t *value)
{
    njs_int_t           ret;
    njs_object_t        *error;
    njs_object_prop_t   *prop;
    njs_lvlhsh_query_t  lhq;

    if (njs_slow_path(!njs_is_error(value))) {
        return NJS_DECLINED;
    }

    if (vm->debug == NULL || vm->start == NULL) {
        return NJS_OK;
    }

    error = njs_object(value);

    lhq.replace = 0;
    lhq.pool = vm->mem_pool;
    lhq.proto = &njs_object_hash_proto;

    lhq.key = njs_str_value("stack");
    lhq.key_hash = NJS_STACK_HASH;

    prop = njs_object_prop_alloc(vm, &njs_error_stack_string,
                                 &njs_value_undefined, 1);
    if (njs_slow_path(prop == NULL)) {
        return NJS_ERROR;
    }

    prop->enumerable = 0;

    ret = njs_error_stack_new(vm, error, &prop->value);
    if (njs_slow_path(ret == NJS_ERROR)) {
        njs_internal_error(vm, "njs_error_stack_new() failed");
        return NJS_ERROR;
    }

    if (ret == NJS_OK) {
        lhq.value = prop;

        ret = njs_lvlhsh_insert(&error->hash, &lhq);
        if (njs_slow_path(ret == NJS_ERROR)) {
            njs_internal_error(vm, "lvlhsh insert failed");
            return NJS_ERROR;
        }
    }

    return NJS_OK;
}


njs_int_t
njs_error_stack(njs_vm_t *vm, njs_value_t *value, njs_value_t *stack)
{
    njs_int_t  ret;

    ret = njs_value_property(vm, value, njs_value_arg(&njs_error_stack_string),
                             stack);
    if (njs_slow_path(ret != NJS_OK)) {
        return ret;
    }

    if (!njs_is_string(stack)) {
        return NJS_DECLINED;
    }

    return NJS_OK;
}


njs_object_t *
njs_error_alloc(njs_vm_t *vm, njs_object_type_t type, const njs_value_t *name,
    const njs_value_t *message)
{
    njs_int_t           ret;
    njs_object_t        *error;
    njs_object_prop_t   *prop;
    njs_lvlhsh_query_t  lhq;

    error = njs_mp_alloc(vm->mem_pool, sizeof(njs_object_t));
    if (njs_slow_path(error == NULL)) {
        goto memory_error;
    }

    njs_lvlhsh_init(&error->hash);
    njs_lvlhsh_init(&error->shared_hash);
    error->type = NJS_OBJECT;
    error->shared = 0;
    error->extensible = 1;
    error->fast_array = 0;
    error->error_data = 1;
    error->__proto__ = &vm->prototypes[type].object;

    lhq.replace = 0;
    lhq.pool = vm->mem_pool;
    lhq.proto = &njs_object_hash_proto;

    if (name != NULL) {
        lhq.key = njs_str_value("name");
        lhq.key_hash = NJS_NAME_HASH;

        prop = njs_object_prop_alloc(vm, &njs_error_name_string, name, 1);
        if (njs_slow_path(prop == NULL)) {
            goto memory_error;
        }

        lhq.value = prop;

        ret = njs_lvlhsh_insert(&error->hash, &lhq);
        if (njs_slow_path(ret != NJS_OK)) {
            njs_internal_error(vm, "lvlhsh insert failed");
            return NULL;
        }
    }

    if (message!= NULL) {
        lhq.key = njs_str_value("message");
        lhq.key_hash = NJS_MESSAGE_HASH;

        prop = njs_object_prop_alloc(vm, &njs_error_message_string, message, 1);
        if (njs_slow_path(prop == NULL)) {
            goto memory_error;
        }

        prop->enumerable = 0;

        lhq.value = prop;

        ret = njs_lvlhsh_insert(&error->hash, &lhq);
        if (njs_slow_path(ret != NJS_OK)) {
            njs_internal_error(vm, "lvlhsh insert failed");
            return NULL;
        }
    }

    return error;

memory_error:

    njs_memory_error(vm);

    return NULL;
}


static njs_int_t
njs_error_constructor(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t type)
{
    njs_int_t     ret;
    njs_value_t   *value;
    njs_object_t  *error;

    value = njs_arg(args, nargs, 1);

    if (njs_slow_path(!njs_is_string(value))) {
        if (!njs_is_undefined(value)) {
            ret = njs_value_to_string(vm, value, value);
            if (njs_slow_path(ret != NJS_OK)) {
                return ret;
            }
        }
    }

    error = njs_error_alloc(vm, type, NULL,
                            njs_is_defined(value) ? value : NULL);
    if (njs_slow_path(error == NULL)) {
        return NJS_ERROR;
    }

    njs_set_object(&vm->retval, error);

    return NJS_OK;
}


static const njs_object_prop_t  njs_error_constructor_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("Error"),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("length"),
        .value = njs_value(NJS_NUMBER, 1, 1.0),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("prototype"),
        .value = njs_prop_handler(njs_object_prototype_create),
    },
};


const njs_object_init_t  njs_error_constructor_init = {
    njs_error_constructor_properties,
    njs_nitems(njs_error_constructor_properties),
};


static const njs_object_prop_t  njs_eval_error_constructor_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("EvalError"),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("length"),
        .value = njs_value(NJS_NUMBER, 1, 1.0),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("prototype"),
        .value = njs_prop_handler(njs_object_prototype_create),
    },
};


const njs_object_init_t  njs_eval_error_constructor_init = {
    njs_eval_error_constructor_properties,
    njs_nitems(njs_eval_error_constructor_properties),
};


static const njs_object_prop_t  njs_internal_error_constructor_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("InternalError"),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("length"),
        .value = njs_value(NJS_NUMBER, 1, 1.0),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("prototype"),
        .value = njs_prop_handler(njs_object_prototype_create),
    },
};


const njs_object_init_t  njs_internal_error_constructor_init = {
    njs_internal_error_constructor_properties,
    njs_nitems(njs_internal_error_constructor_properties),
};


static const njs_object_prop_t  njs_range_error_constructor_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("RangeError"),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("length"),
        .value = njs_value(NJS_NUMBER, 1, 1.0),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("prototype"),
        .value = njs_prop_handler(njs_object_prototype_create),
    },
};


const njs_object_init_t  njs_range_error_constructor_init = {
    njs_range_error_constructor_properties,
    njs_nitems(njs_range_error_constructor_properties),
};


static const njs_object_prop_t  njs_reference_error_constructor_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("ReferenceError"),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("length"),
        .value = njs_value(NJS_NUMBER, 1, 1.0),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("prototype"),
        .value = njs_prop_handler(njs_object_prototype_create),
    },
};


const njs_object_init_t  njs_reference_error_constructor_init = {
    njs_reference_error_constructor_properties,
    njs_nitems(njs_reference_error_constructor_properties),
};


static const njs_object_prop_t  njs_syntax_error_constructor_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("SyntaxError"),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("length"),
        .value = njs_value(NJS_NUMBER, 1, 1.0),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("prototype"),
        .value = njs_prop_handler(njs_object_prototype_create),
    },
};


const njs_object_init_t  njs_syntax_error_constructor_init = {
    njs_syntax_error_constructor_properties,
    njs_nitems(njs_syntax_error_constructor_properties),
};


static const njs_object_prop_t  njs_type_error_constructor_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("TypeError"),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("length"),
        .value = njs_value(NJS_NUMBER, 1, 1.0),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("prototype"),
        .value = njs_prop_handler(njs_object_prototype_create),
    },
};


const njs_object_init_t  njs_type_error_constructor_init = {
    njs_type_error_constructor_properties,
    njs_nitems(njs_type_error_constructor_properties),
};


static const njs_object_prop_t  njs_uri_error_constructor_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("URIError"),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("length"),
        .value = njs_value(NJS_NUMBER, 1, 1.0),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("prototype"),
        .value = njs_prop_handler(njs_object_prototype_create),
    },
};


const njs_object_init_t  njs_uri_error_constructor_init = {
    njs_uri_error_constructor_properties,
    njs_nitems(njs_uri_error_constructor_properties),
};


void
njs_memory_error_set(njs_vm_t *vm, njs_value_t *value)
{
    njs_object_t            *object;
    njs_object_prototype_t  *prototypes;

    prototypes = vm->prototypes;
    object = &vm->memory_error_object;

    njs_lvlhsh_init(&object->hash);
    njs_lvlhsh_init(&object->shared_hash);
    object->__proto__ = &prototypes[NJS_OBJ_TYPE_INTERNAL_ERROR].object;
    object->type = NJS_OBJECT;
    object->shared = 1;

    /*
     * Marking it nonextensible to differentiate
     * it from ordinary internal errors.
     */
    object->extensible = 0;
    object->fast_array = 0;
    object->error_data = 1;

    njs_set_object(value, object);
}


void
njs_memory_error(njs_vm_t *vm)
{
    njs_memory_error_set(vm, &vm->retval);
}


static njs_int_t
njs_memory_error_constructor(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t unused)
{
    njs_memory_error_set(vm, &vm->retval);

    return NJS_OK;
}


static njs_int_t
njs_memory_error_prototype_create(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    int32_t            index;
    njs_function_t     *function;
    const njs_value_t  *proto;

    /* MemoryError has no its own prototype. */

    index = NJS_OBJ_TYPE_INTERNAL_ERROR;

    function = njs_function(value);
    proto = njs_property_prototype_create(vm, &function->object.hash,
                                          &vm->prototypes[index].object);
    if (proto == NULL) {
        proto = &njs_value_undefined;
    }

    *retval = *proto;

    return NJS_OK;
}


static const njs_object_prop_t  njs_memory_error_constructor_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("MemoryError"),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("length"),
        .value = njs_value(NJS_NUMBER, 1, 1.0),
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("prototype"),
        .value = njs_prop_handler(njs_memory_error_prototype_create),
    },
};


const njs_object_init_t  njs_memory_error_constructor_init = {
    njs_memory_error_constructor_properties,
    njs_nitems(njs_memory_error_constructor_properties),
};


static njs_int_t
njs_error_prototype_value_of(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    vm->retval = args[0];

    return NJS_OK;
}


static njs_int_t
njs_error_to_string2(njs_vm_t *vm, njs_value_t *retval,
    const njs_value_t *error, njs_bool_t want_stack)
{
    size_t              length;
    u_char              *p;
    njs_int_t           ret;
    njs_value_t         value1, value2;
    njs_value_t         *name_value, *message_value;
    njs_string_prop_t   name, message;
    njs_lvlhsh_query_t  lhq;

    static const njs_value_t  default_name = njs_string("Error");

    if (want_stack) {
        ret = njs_error_stack(vm, njs_value_arg(error), retval);
        if (njs_slow_path(ret == NJS_ERROR)) {
            return ret;
        }

        if (ret == NJS_OK) {
            return NJS_OK;
        }
    }

    njs_object_property_init(&lhq, &njs_string_name, NJS_NAME_HASH);

    ret = njs_object_property(vm, error, &lhq, &value1);

    if (njs_slow_path(ret == NJS_ERROR)) {
        return ret;
    }

    name_value = (ret == NJS_OK) ? &value1 : njs_value_arg(&default_name);

    if (njs_slow_path(!njs_is_string(name_value))) {
        ret = njs_value_to_string(vm, &value1, name_value);
        if (njs_slow_path(ret != NJS_OK)) {
            return ret;
        }

        name_value = &value1;
    }

    (void) njs_string_prop(&name, name_value);

    lhq.key_hash = NJS_MESSAGE_HASH;
    lhq.key = njs_str_value("message");

    ret = njs_object_property(vm, error, &lhq, &value2);

    if (njs_slow_path(ret == NJS_ERROR)) {
        return ret;
    }

    message_value = (ret == NJS_OK) ? &value2
                                    : njs_value_arg(&njs_string_empty);

    if (njs_slow_path(!njs_is_string(message_value))) {
        ret = njs_value_to_string(vm, &value2, message_value);
        if (njs_slow_path(ret != NJS_OK)) {
            return ret;
        }

        message_value = &value2;
    }

    (void) njs_string_prop(&message, message_value);

    if (name.size == 0) {
        *retval = *message_value;
        return NJS_OK;
    }

    if (message.size == 0) {
        *retval = *name_value;
        return NJS_OK;
    }

    if (name.length != 0 && message.length != 0) {
        length = name.length + message.length + 2;

    } else {
        length = 0;
    }

    p = njs_string_alloc(vm, retval, name.size + message.size + 2, length);

    if (njs_fast_path(p != NULL)) {
        p = njs_cpymem(p, name.start, name.size);
        *p++ = ':';
        *p++ = ' ';
        memcpy(p, message.start, message.size);

        return NJS_OK;
    }

    njs_memory_error(vm);

    return NJS_ERROR;
}


static njs_int_t
njs_error_prototype_to_string(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    if (nargs < 1 || !njs_is_object(&args[0])) {
        njs_type_error(vm, "\"this\" argument is not an object");
        return NJS_ERROR;
    }

    return njs_error_to_string2(vm, &vm->retval, &args[0], 0);
}


njs_int_t
njs_error_to_string(njs_vm_t *vm, njs_value_t *retval, const njs_value_t *error)
{
    return njs_error_to_string2(vm, retval, error, 1);
}


static const njs_object_prop_t  njs_error_prototype_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("Error"),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("constructor"),
        .value = njs_prop_handler(njs_object_prototype_create_constructor),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("message"),
        .value = njs_string(""),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("valueOf"),
        .value = njs_native_function(njs_error_prototype_value_of, 0),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("toString"),
        .value = njs_native_function(njs_error_prototype_to_string, 0),
        .writable = 1,
        .configurable = 1,
    },
};


const njs_object_init_t  njs_error_prototype_init = {
    njs_error_prototype_properties,
    njs_nitems(njs_error_prototype_properties),
};


const njs_object_type_init_t  njs_error_type_init = {
    .constructor = njs_native_ctor(njs_error_constructor, 1,
                                   NJS_OBJ_TYPE_ERROR),
    .constructor_props = &njs_error_constructor_init,
    .prototype_props = &njs_error_prototype_init,
    .prototype_value = { .object = { .type = NJS_OBJECT } },
};


static const njs_object_prop_t  njs_eval_error_prototype_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("EvalError"),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("message"),
        .value = njs_string(""),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("constructor"),
        .value = njs_prop_handler(njs_object_prototype_create_constructor),
        .writable = 1,
        .configurable = 1,
    },
};


const njs_object_init_t  njs_eval_error_prototype_init = {
    njs_eval_error_prototype_properties,
    njs_nitems(njs_eval_error_prototype_properties),
};


const njs_object_type_init_t  njs_eval_error_type_init = {
    .constructor = njs_native_ctor(njs_error_constructor, 1,
                                   NJS_OBJ_TYPE_EVAL_ERROR),
    .constructor_props = &njs_eval_error_constructor_init,
    .prototype_props = &njs_eval_error_prototype_init,
    .prototype_value = { .object = { .type = NJS_OBJECT } },
};


static njs_int_t
njs_internal_error_prototype_to_string(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t unused)
{
    if (nargs >= 1 && njs_is_object(&args[0])) {

        /* MemoryError is a nonextensible internal error. */
        if (!njs_object(&args[0])->extensible) {
            static const njs_value_t name = njs_string("MemoryError");

            vm->retval = name;

            return NJS_OK;
        }
    }

    return njs_error_prototype_to_string(vm, args, nargs, unused);
}


static const njs_object_prop_t  njs_internal_error_prototype_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("InternalError"),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("message"),
        .value = njs_string(""),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("toString"),
        .value = njs_native_function(njs_internal_error_prototype_to_string, 0),
        .writable = 1,
        .configurable = 1,
    },
};


const njs_object_init_t  njs_internal_error_prototype_init = {
    njs_internal_error_prototype_properties,
    njs_nitems(njs_internal_error_prototype_properties),
};


const njs_object_type_init_t  njs_internal_error_type_init = {
    .constructor = njs_native_ctor(njs_error_constructor, 1,
                                   NJS_OBJ_TYPE_INTERNAL_ERROR),
    .constructor_props = &njs_internal_error_constructor_init,
    .prototype_props = &njs_internal_error_prototype_init,
    .prototype_value = { .object = { .type = NJS_OBJECT } },
};


const njs_object_type_init_t  njs_memory_error_type_init = {
    .constructor = njs_native_ctor(njs_memory_error_constructor, 1, 0),
    .constructor_props = &njs_memory_error_constructor_init,
    .prototype_props = &njs_internal_error_prototype_init,
    .prototype_value = { .object = { .type = NJS_OBJECT } },
};


static const njs_object_prop_t  njs_range_error_prototype_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("RangeError"),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("message"),
        .value = njs_string(""),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("constructor"),
        .value = njs_prop_handler(njs_object_prototype_create_constructor),
        .writable = 1,
        .configurable = 1,
    },
};


const njs_object_init_t  njs_range_error_prototype_init = {
    njs_range_error_prototype_properties,
    njs_nitems(njs_range_error_prototype_properties),
};


const njs_object_type_init_t  njs_range_error_type_init = {
    .constructor = njs_native_ctor(njs_error_constructor, 1,
                                   NJS_OBJ_TYPE_RANGE_ERROR),
    .constructor_props = &njs_range_error_constructor_init,
    .prototype_props = &njs_range_error_prototype_init,
    .prototype_value = { .object = { .type = NJS_OBJECT } },
};


static const njs_object_prop_t  njs_reference_error_prototype_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("ReferenceError"),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("message"),
        .value = njs_string(""),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("constructor"),
        .value = njs_prop_handler(njs_object_prototype_create_constructor),
        .writable = 1,
        .configurable = 1,
    },
};


const njs_object_init_t  njs_reference_error_prototype_init = {
    njs_reference_error_prototype_properties,
    njs_nitems(njs_reference_error_prototype_properties),
};


const njs_object_type_init_t  njs_reference_error_type_init = {
    .constructor = njs_native_ctor(njs_error_constructor, 1,
                                   NJS_OBJ_TYPE_REF_ERROR),
    .constructor_props = &njs_reference_error_constructor_init,
    .prototype_props = &njs_reference_error_prototype_init,
    .prototype_value = { .object = { .type = NJS_OBJECT } },
};


static const njs_object_prop_t  njs_syntax_error_prototype_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("SyntaxError"),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("message"),
        .value = njs_string(""),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("constructor"),
        .value = njs_prop_handler(njs_object_prototype_create_constructor),
        .writable = 1,
        .configurable = 1,
    },
};


const njs_object_init_t  njs_syntax_error_prototype_init = {
    njs_syntax_error_prototype_properties,
    njs_nitems(njs_syntax_error_prototype_properties),
};


const njs_object_type_init_t  njs_syntax_error_type_init = {
    .constructor = njs_native_ctor(njs_error_constructor, 1,
                                   NJS_OBJ_TYPE_SYNTAX_ERROR),
    .constructor_props = &njs_syntax_error_constructor_init,
    .prototype_props = &njs_syntax_error_prototype_init,
    .prototype_value = { .object = { .type = NJS_OBJECT } },
};


static const njs_object_prop_t  njs_type_error_prototype_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("TypeError"),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("message"),
        .value = njs_string(""),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("constructor"),
        .value = njs_prop_handler(njs_object_prototype_create_constructor),
        .writable = 1,
        .configurable = 1,
    },
};


const njs_object_init_t  njs_type_error_prototype_init = {
    njs_type_error_prototype_properties,
    njs_nitems(njs_type_error_prototype_properties),
};


const njs_object_type_init_t  njs_type_error_type_init = {
    .constructor = njs_native_ctor(njs_error_constructor, 1,
                                   NJS_OBJ_TYPE_TYPE_ERROR),
    .constructor_props = &njs_type_error_constructor_init,
    .prototype_props = &njs_type_error_prototype_init,
    .prototype_value = { .object = { .type = NJS_OBJECT } },
};


static const njs_object_prop_t  njs_uri_error_prototype_properties[] =
{
    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("constructor"),
        .value = njs_prop_handler(njs_object_prototype_create_constructor),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("message"),
        .value = njs_string(""),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("URIError"),
        .writable = 1,
        .configurable = 1,
    },
};


const njs_object_init_t  njs_uri_error_prototype_init = {
    njs_uri_error_prototype_properties,
    njs_nitems(njs_uri_error_prototype_properties),
};


const njs_object_type_init_t  njs_uri_error_type_init = {
    .constructor = njs_native_ctor(njs_error_constructor, 1,
                                   NJS_OBJ_TYPE_URI_ERROR),
    .constructor_props = &njs_uri_error_constructor_init,
    .prototype_props = &njs_uri_error_prototype_init,
    .prototype_value = { .object = { .type = NJS_OBJECT } },
};
