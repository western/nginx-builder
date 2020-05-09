
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */


#include <njs_main.h>


static njs_int_t njs_object_hash_test(njs_lvlhsh_query_t *lhq, void *data);
static njs_object_prop_t *njs_object_exist_in_proto(const njs_object_t *begin,
    const njs_object_t *end, njs_lvlhsh_query_t *lhq);
static njs_int_t njs_object_enumerate_array(njs_vm_t *vm,
    const njs_array_t *array, njs_array_t *items, njs_object_enum_t kind);
static njs_int_t njs_object_enumerate_typed_array(njs_vm_t *vm,
    const njs_typed_array_t *array, njs_array_t *items, njs_object_enum_t kind);
static njs_int_t njs_object_enumerate_string(njs_vm_t *vm,
    const njs_value_t *value, njs_array_t *items, njs_object_enum_t kind);
static njs_int_t njs_object_enumerate_object(njs_vm_t *vm,
    const njs_object_t *object, njs_array_t *items, njs_object_enum_t kind,
    njs_object_enum_type_t type, njs_bool_t all);
static njs_int_t njs_object_own_enumerate_object(njs_vm_t *vm,
    const njs_object_t *object, const njs_object_t *parent, njs_array_t *items,
    njs_object_enum_t kind, njs_object_enum_type_t type, njs_bool_t all);
static njs_int_t njs_object_define_properties(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t unused);
static njs_int_t njs_object_set_prototype(njs_vm_t *vm, njs_object_t *object,
    const njs_value_t *value);


njs_object_t *
njs_object_alloc(njs_vm_t *vm)
{
    njs_object_t  *object;

    object = njs_mp_alloc(vm->mem_pool, sizeof(njs_object_t));

    if (njs_fast_path(object != NULL)) {
        njs_lvlhsh_init(&object->hash);
        njs_lvlhsh_init(&object->shared_hash);
        object->__proto__ = &vm->prototypes[NJS_OBJ_TYPE_OBJECT].object;
        object->slots = NULL;
        object->type = NJS_OBJECT;
        object->shared = 0;
        object->extensible = 1;
        object->error_data = 0;
        object->fast_array = 0;

        return object;
    }

    njs_memory_error(vm);

    return NULL;
}


njs_object_t *
njs_object_value_copy(njs_vm_t *vm, njs_value_t *value)
{
    size_t        size;
    njs_object_t  *object;

    object = njs_object(value);

    if (!object->shared) {
        return object;
    }

    size = njs_is_object_value(value) ? sizeof(njs_object_value_t)
                                      : sizeof(njs_object_t);
    object = njs_mp_alloc(vm->mem_pool, size);

    if (njs_fast_path(object != NULL)) {
        memcpy(object, njs_object(value), size);
        object->__proto__ = &vm->prototypes[NJS_OBJ_TYPE_OBJECT].object;
        object->shared = 0;
        value->data.u.object = object;
        return object;
    }

    njs_memory_error(vm);

    return NULL;
}


njs_object_t *
njs_object_value_alloc(njs_vm_t *vm, const njs_value_t *value, njs_uint_t type)
{
    njs_uint_t          index;
    njs_object_value_t  *ov;

    ov = njs_mp_alloc(vm->mem_pool, sizeof(njs_object_value_t));

    if (njs_fast_path(ov != NULL)) {
        njs_lvlhsh_init(&ov->object.hash);

        if (type == NJS_STRING) {
            ov->object.shared_hash = vm->shared->string_instance_hash;

        } else {
            njs_lvlhsh_init(&ov->object.shared_hash);
        }

        ov->object.type = njs_object_value_type(type);
        ov->object.shared = 0;
        ov->object.extensible = 1;
        ov->object.error_data = 0;
        ov->object.fast_array = 0;

        index = njs_primitive_prototype_index(type);
        ov->object.__proto__ = &vm->prototypes[index].object;
        ov->object.slots = NULL;

        ov->value = *value;

        return &ov->object;
    }

    njs_memory_error(vm);

    return NULL;
}


njs_int_t
njs_object_hash_create(njs_vm_t *vm, njs_lvlhsh_t *hash,
    const njs_object_prop_t *prop, njs_uint_t n)
{
    njs_int_t           ret;
    njs_lvlhsh_query_t  lhq;

    lhq.replace = 0;
    lhq.proto = &njs_object_hash_proto;
    lhq.pool = vm->mem_pool;

    while (n != 0) {

        njs_object_property_key_set(&lhq, &prop->name, 0);

        lhq.value = (void *) prop;

        ret = njs_lvlhsh_insert(hash, &lhq);
        if (njs_slow_path(ret != NJS_OK)) {
            njs_internal_error(vm, "lvlhsh insert failed");
            return NJS_ERROR;
        }

        prop++;
        n--;
    }

    return NJS_OK;
}


const njs_lvlhsh_proto_t  njs_object_hash_proto
    njs_aligned(64) =
{
    NJS_LVLHSH_DEFAULT,
    njs_object_hash_test,
    njs_lvlhsh_alloc,
    njs_lvlhsh_free,
};


static njs_int_t
njs_object_hash_test(njs_lvlhsh_query_t *lhq, void *data)
{
    size_t             size;
    u_char             *start;
    njs_value_t        *name;
    njs_object_prop_t  *prop;

    prop = data;
    name = &prop->name;

    if (njs_slow_path(njs_is_symbol(name))) {
        return ((njs_symbol_key(name) == lhq->key_hash)
                && lhq->key.start == NULL) ? NJS_OK : NJS_DECLINED;
    }

    /* string. */

    size = name->short_string.size;

    if (size != NJS_STRING_LONG) {
        if (lhq->key.length != size) {
            return NJS_DECLINED;
        }

        start = name->short_string.start;

    } else {
        if (lhq->key.length != name->long_string.size) {
            return NJS_DECLINED;
        }

        start = name->long_string.data->start;
    }

    if (memcmp(start, lhq->key.start, lhq->key.length) == 0) {
        return NJS_OK;
    }

    return NJS_DECLINED;
}


static njs_int_t
njs_object_constructor(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_uint_t    type;
    njs_value_t   *value;
    njs_object_t  *object;

    value = njs_arg(args, nargs, 1);
    type = value->type;

    if (njs_is_null_or_undefined(value)) {

        object = njs_object_alloc(vm);
        if (njs_slow_path(object == NULL)) {
            return NJS_ERROR;
        }

        type = NJS_OBJECT;

    } else {

        if (njs_is_object(value)) {
            object = njs_object(value);

        } else if (njs_is_primitive(value)) {

            /* value->type is the same as prototype offset. */
            object = njs_object_value_alloc(vm, value, type);
            if (njs_slow_path(object == NULL)) {
                return NJS_ERROR;
            }

            type = njs_object_value_type(type);

        } else {
            njs_type_error(vm, "unexpected constructor argument:%s",
                           njs_type_string(type));

            return NJS_ERROR;
        }
    }

    njs_set_type_object(&vm->retval, object, type);

    return NJS_OK;
}


static njs_int_t
njs_object_create(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_value_t   *value, *descs, arguments[3];
    njs_object_t  *object;

    value = njs_arg(args, nargs, 1);

    if (njs_is_object(value) || njs_is_null(value)) {

        object = njs_object_alloc(vm);
        if (njs_slow_path(object == NULL)) {
            return NJS_ERROR;
        }

        if (!njs_is_null(value)) {
            /* GC */
            object->__proto__ = njs_object(value);

        } else {
            object->__proto__ = NULL;
        }

        njs_set_object(&vm->retval, object);

        descs = njs_arg(args, nargs, 2);

        if (njs_slow_path(!njs_is_undefined(descs))) {
            arguments[0] = args[0];
            arguments[1] = vm->retval;
            arguments[2] = *descs;

            return njs_object_define_properties(vm, arguments, 3, unused);
        }

        return NJS_OK;
    }

    njs_type_error(vm, "prototype may only be an object or null: %s",
                   njs_type_string(value->type));

    return NJS_ERROR;
}


static njs_int_t
njs_object_keys(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_value_t  *value;
    njs_array_t  *keys;

    value = njs_arg(args, nargs, 1);

    if (njs_is_null_or_undefined(value)) {
        njs_type_error(vm, "cannot convert %s argument to object",
                       njs_type_string(value->type));

        return NJS_ERROR;
    }

    keys = njs_value_own_enumerate(vm, value, NJS_ENUM_KEYS,
                                   NJS_ENUM_STRING, 0);
    if (keys == NULL) {
        return NJS_ERROR;
    }

    njs_set_array(&vm->retval, keys);

    return NJS_OK;
}


static njs_int_t
njs_object_values(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_array_t  *array;
    njs_value_t  *value;

    value = njs_arg(args, nargs, 1);

    if (njs_is_null_or_undefined(value)) {
        njs_type_error(vm, "cannot convert %s argument to object",
                       njs_type_string(value->type));

        return NJS_ERROR;
    }

    array = njs_value_own_enumerate(vm, value, NJS_ENUM_VALUES,
                                    NJS_ENUM_STRING, 0);
    if (array == NULL) {
        return NJS_ERROR;
    }

    njs_set_array(&vm->retval, array);

    return NJS_OK;
}


static njs_int_t
njs_object_entries(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_array_t  *array;
    njs_value_t  *value;

    value = njs_arg(args, nargs, 1);

    if (njs_is_null_or_undefined(value)) {
        njs_type_error(vm, "cannot convert %s argument to object",
                       njs_type_string(value->type));

        return NJS_ERROR;
    }

    array = njs_value_own_enumerate(vm, value, NJS_ENUM_BOTH,
                                    NJS_ENUM_STRING, 0);
    if (array == NULL) {
        return NJS_ERROR;
    }

    njs_set_array(&vm->retval, array);

    return NJS_OK;
}


static njs_object_prop_t *
njs_object_exist_in_proto(const njs_object_t *object, const njs_object_t *end,
    njs_lvlhsh_query_t *lhq)
{
    njs_int_t          ret;
    njs_object_prop_t  *prop;

    while (object != end) {
        ret = njs_lvlhsh_find(&object->hash, lhq);

        if (njs_fast_path(ret == NJS_OK)) {
            prop = lhq->value;

            if (prop->type == NJS_WHITEOUT) {
                goto next;
            }

            return lhq->value;
        }

        ret = njs_lvlhsh_find(&object->shared_hash, lhq);

        if (njs_fast_path(ret == NJS_OK)) {
            return lhq->value;
        }

next:

        object = object->__proto__;
    }

    return NULL;
}


njs_inline njs_int_t
njs_object_enumerate_value(njs_vm_t *vm, const njs_object_t *object,
    njs_array_t *items, njs_object_enum_t kind, njs_object_enum_type_t type,
    njs_bool_t all)
{
    njs_int_t           ret;
    njs_object_value_t  *obj_val;

    if (type & NJS_ENUM_STRING) {
        switch (object->type) {
        case NJS_ARRAY:
            ret = njs_object_enumerate_array(vm, (njs_array_t *) object, items,
                                             kind);
            break;

        case NJS_OBJECT_STRING:
            obj_val = (njs_object_value_t *) object;

            ret = njs_object_enumerate_string(vm, &obj_val->value, items, kind);
            break;

        default:
            goto object;
        }

        if (njs_slow_path(ret != NJS_OK)) {
            return NJS_ERROR;
        }
    }

object:

    ret = njs_object_enumerate_object(vm, object, items, kind, type, all);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    return NJS_OK;
}


njs_inline njs_int_t
njs_object_own_enumerate_value(njs_vm_t *vm, const njs_object_t *object,
    const njs_object_t *parent, njs_array_t *items, njs_object_enum_t kind,
    njs_object_enum_type_t type, njs_bool_t all)
{
    njs_int_t           ret;
    njs_object_value_t  *obj_val;

    if (type & NJS_ENUM_STRING) {
        switch (object->type) {
        case NJS_ARRAY:
            ret = njs_object_enumerate_array(vm, (njs_array_t *) object, items,
                                             kind);
            break;

        case NJS_TYPED_ARRAY:
            ret = njs_object_enumerate_typed_array(vm,
                                                   (njs_typed_array_t *) object,
                                                   items, kind);
            break;

        case NJS_OBJECT_STRING:
            obj_val = (njs_object_value_t *) object;

            ret = njs_object_enumerate_string(vm, &obj_val->value, items, kind);
            break;

        default:
            goto object;
        }

        if (njs_slow_path(ret != NJS_OK)) {
            return NJS_ERROR;
        }
    }

object:

    ret = njs_object_own_enumerate_object(vm, object, parent, items, kind,
                                          type, all);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    return NJS_OK;
}


njs_array_t *
njs_object_enumerate(njs_vm_t *vm, const njs_object_t *object,
    njs_object_enum_t kind, njs_object_enum_type_t type, njs_bool_t all)
{
    njs_int_t    ret;
    njs_array_t  *items;

    items = njs_array_alloc(vm, 1, 0, NJS_ARRAY_SPARE);
    if (njs_slow_path(items == NULL)) {
        return NULL;
    }

    ret = njs_object_enumerate_value(vm, object, items, kind, type, all);
    if (njs_slow_path(ret != NJS_OK)) {
        return NULL;
    }

    return items;
}


njs_array_t *
njs_object_own_enumerate(njs_vm_t *vm, const njs_object_t *object,
    njs_object_enum_t kind, njs_object_enum_type_t type, njs_bool_t all)
{
    njs_int_t    ret;
    njs_array_t  *items;

    items = njs_array_alloc(vm, 1, 0, NJS_ARRAY_SPARE);
    if (njs_slow_path(items == NULL)) {
        return NULL;
    }

    ret = njs_object_own_enumerate_value(vm, object, object, items, kind, type,
                                         all);
    if (njs_slow_path(ret != NJS_OK)) {
        return NULL;
    }

    return items;
}


njs_inline njs_bool_t
njs_is_enumerable(const njs_value_t *value, njs_object_enum_type_t type)
{
    return (njs_is_string(value) && (type & NJS_ENUM_STRING))
           || (njs_is_symbol(value) && (type & NJS_ENUM_SYMBOL));
}


static njs_int_t
njs_object_enumerate_array(njs_vm_t *vm, const njs_array_t *array,
    njs_array_t *items, njs_object_enum_t kind)
{
    njs_int_t    ret;
    njs_value_t  *p, *start, *end;
    njs_array_t  *entry;

    if (!array->object.fast_array) {
        return NJS_OK;
    }

    start = array->start;

    p = start;
    end = p + array->length;

    switch (kind) {
    case NJS_ENUM_KEYS:
        while (p < end) {
            if (njs_is_valid(p)) {
                ret = njs_array_expand(vm, items, 0, 1);
                if (njs_slow_path(ret != NJS_OK)) {
                    return NJS_ERROR;
                }

                njs_uint32_to_string(&items->start[items->length++], p - start);
            }

            p++;
        }

        break;

    case NJS_ENUM_VALUES:
        while (p < end) {
            if (njs_is_valid(p)) {
                ret = njs_array_add(vm, items, p);
                if (njs_slow_path(ret != NJS_OK)) {
                    return NJS_ERROR;
                }
            }

            p++;
        }

        break;

    case NJS_ENUM_BOTH:
        while (p < end) {
            if (njs_is_valid(p)) {
                entry = njs_array_alloc(vm, 0, 2, 0);
                if (njs_slow_path(entry == NULL)) {
                    return NJS_ERROR;
                }

                njs_uint32_to_string(&entry->start[0], p - start);
                entry->start[1] = *p;

                ret = njs_array_expand(vm, items, 0, 1);
                if (njs_slow_path(ret != NJS_OK)) {
                    return NJS_ERROR;
                }

                njs_set_array(&items->start[items->length++], entry);
            }

            p++;
        }

        break;
    }

    return NJS_OK;
}


static njs_int_t
njs_object_enumerate_typed_array(njs_vm_t *vm, const njs_typed_array_t *array,
    njs_array_t *items, njs_object_enum_t kind)
{
    uint32_t     i, length;
    njs_int_t    ret;
    njs_value_t  *item;
    njs_array_t  *entry;

    length = njs_typed_array_length(array);

    ret = njs_array_expand(vm, items, 0, length);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    item = &items->start[items->length];

    switch (kind) {
    case NJS_ENUM_KEYS:
        for (i = 0; i < length; i++) {
            njs_uint32_to_string(item++, i);
        }

        break;

    case NJS_ENUM_VALUES:
        for (i = 0; i < length; i++) {
            njs_set_number(item++, njs_typed_array_get(array, i));
        }

        break;

    case NJS_ENUM_BOTH:
        for (i = 0; i < length; i++) {
            entry = njs_array_alloc(vm, 0, 2, 0);
            if (njs_slow_path(entry == NULL)) {
                return NJS_ERROR;
            }

            njs_uint32_to_string(&entry->start[0], i);
            njs_set_number(&entry->start[1], njs_typed_array_get(array, i));

            njs_set_array(item++, entry);
        }

        break;
    }

    items->length += length;

    return NJS_OK;
}


static njs_int_t
njs_object_enumerate_string(njs_vm_t *vm, const njs_value_t *value,
    njs_array_t *items, njs_object_enum_t kind)
{
    u_char             *begin;
    uint32_t           i, len, size;
    njs_int_t          ret;
    njs_value_t        *item, *string;
    njs_array_t        *entry;
    const u_char       *src, *end;
    njs_string_prop_t  str_prop;

    len = (uint32_t) njs_string_prop(&str_prop, value);

    ret = njs_array_expand(vm, items, 0, len);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    item = &items->start[items->length];

    switch (kind) {
    case NJS_ENUM_KEYS:
        for (i = 0; i < len; i++) {
            njs_uint32_to_string(item++, i);
        }

        break;

    case NJS_ENUM_VALUES:
        if (str_prop.size == (size_t) len) {
            /* Byte or ASCII string. */

            for (i = 0; i < len; i++) {
                begin = njs_string_short_start(item);
                *begin = str_prop.start[i];

                njs_string_short_set(item, 1, 1);

                item++;
            }

        } else {
            /* UTF-8 string. */

            src = str_prop.start;
            end = src + str_prop.size;

            do {
                begin = (u_char *) src;
                njs_utf8_copy(njs_string_short_start(item), &src, end);
                size = (uint32_t) (src - begin);

                njs_string_short_set(item, size, 1);

                item++;

            } while (src != end);
        }

        break;

    case NJS_ENUM_BOTH:
        if (str_prop.size == (size_t) len) {
            /* Byte or ASCII string. */

            for (i = 0; i < len; i++) {

                entry = njs_array_alloc(vm, 0, 2, 0);
                if (njs_slow_path(entry == NULL)) {
                    return NJS_ERROR;
                }

                njs_uint32_to_string(&entry->start[0], i);

                string = &entry->start[1];

                begin = njs_string_short_start(string);
                *begin = str_prop.start[i];

                njs_string_short_set(string, 1, 1);

                njs_set_array(item, entry);

                item++;
            }

        } else {
            /* UTF-8 string. */

            src = str_prop.start;
            end = src + str_prop.size;
            i = 0;

            do {
                entry = njs_array_alloc(vm, 0, 2, 0);
                if (njs_slow_path(entry == NULL)) {
                    return NJS_ERROR;
                }

                njs_uint32_to_string(&entry->start[0], i++);

                string = &entry->start[1];

                begin = (u_char *) src;
                njs_utf8_copy(njs_string_short_start(string), &src, end);
                size = (uint32_t) (src - begin);

                njs_string_short_set(string, size, 1);

                njs_set_array(item, entry);

                item++;

            } while (src != end);
        }

        break;
    }

    items->length += len;

    return NJS_OK;
}


static njs_int_t
njs_object_enumerate_object(njs_vm_t *vm, const njs_object_t *object,
    njs_array_t *items, njs_object_enum_t kind, njs_object_enum_type_t type,
    njs_bool_t all)
{
    njs_int_t           ret;
    const njs_object_t  *proto;

    ret = njs_object_own_enumerate_object(vm, object, object, items, kind,
                                          type, all);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    proto = object->__proto__;

    while (proto != NULL) {
        ret = njs_object_own_enumerate_value(vm, proto, object, items, kind,
                                             type, all);
        if (njs_slow_path(ret != NJS_OK)) {
            return NJS_ERROR;
        }

        proto = proto->__proto__;
    }

    return NJS_OK;
}


static njs_int_t
njs_object_own_enumerate_object(njs_vm_t *vm, const njs_object_t *object,
    const njs_object_t *parent, njs_array_t *items, njs_object_enum_t kind,
    njs_object_enum_type_t type, njs_bool_t all)
{
    njs_int_t           ret;
    njs_value_t         value;
    njs_array_t         *entry;
    njs_lvlhsh_each_t   lhe;
    njs_object_prop_t   *prop, *ext_prop;
    njs_lvlhsh_query_t  lhq;
    const njs_lvlhsh_t  *hash;

    lhq.proto = &njs_object_hash_proto;

    njs_lvlhsh_each_init(&lhe, &njs_object_hash_proto);
    hash = &object->hash;

    switch (kind) {
    case NJS_ENUM_KEYS:
        for ( ;; ) {
            prop = njs_lvlhsh_each(hash, &lhe);

            if (prop == NULL) {
                break;
            }

            if (!njs_is_enumerable(&prop->name, type)) {
                continue;
            }

            njs_object_property_key_set(&lhq, &prop->name, lhe.key_hash);

            ext_prop = njs_object_exist_in_proto(parent, object, &lhq);

            if (ext_prop == NULL && prop->type != NJS_WHITEOUT
                && (prop->enumerable || all))
            {
                ret = njs_array_add(vm, items, &prop->name);
                if (njs_slow_path(ret != NJS_OK)) {
                    return NJS_ERROR;
                }
            }
        }

        njs_lvlhsh_each_init(&lhe, &njs_object_hash_proto);
        hash = &object->shared_hash;

        for ( ;; ) {
            prop = njs_lvlhsh_each(hash, &lhe);

            if (prop == NULL) {
                break;
            }

            if (!njs_is_enumerable(&prop->name, type)) {
                continue;
            }

            njs_object_property_key_set(&lhq, &prop->name, lhe.key_hash);

            ret = njs_lvlhsh_find(&object->hash, &lhq);

            if (ret != NJS_OK) {
                ext_prop = njs_object_exist_in_proto(parent, object, &lhq);

                if (ext_prop == NULL && (prop->enumerable || all)) {
                    ret = njs_array_add(vm, items, &prop->name);
                    if (njs_slow_path(ret != NJS_OK)) {
                        return NJS_ERROR;
                    }
                }
            }
        }

        break;

    case NJS_ENUM_VALUES:
        for ( ;; ) {
            prop = njs_lvlhsh_each(hash, &lhe);

            if (prop == NULL) {
                break;
            }

            if (!njs_is_enumerable(&prop->name, type)) {
                continue;
            }

            njs_object_property_key_set(&lhq, &prop->name, lhe.key_hash);

            ext_prop = njs_object_exist_in_proto(parent, object, &lhq);

            if (ext_prop == NULL && prop->type != NJS_WHITEOUT
                && (prop->enumerable || all))
            {
                ret = njs_array_add(vm, items, &prop->value);
                if (njs_slow_path(ret != NJS_OK)) {
                    return NJS_ERROR;
                }
            }
        }

        njs_lvlhsh_each_init(&lhe, &njs_object_hash_proto);
        hash = &object->shared_hash;

        for ( ;; ) {
            prop = njs_lvlhsh_each(hash, &lhe);

            if (prop == NULL) {
                break;
            }

            if (!njs_is_enumerable(&prop->name, type)) {
                continue;
            }

            njs_object_property_key_set(&lhq, &prop->name, lhe.key_hash);

            ret = njs_lvlhsh_find(&object->hash, &lhq);

            if (ret != NJS_OK) {
                ext_prop = njs_object_exist_in_proto(parent, object, &lhq);

                if (ext_prop == NULL && (prop->enumerable || all)) {
                    ret = njs_array_add(vm, items, &prop->value);
                    if (njs_slow_path(ret != NJS_OK)) {
                        return NJS_ERROR;
                    }
                }
            }
        }

        break;

    case NJS_ENUM_BOTH:
        for ( ;; ) {
            prop = njs_lvlhsh_each(hash, &lhe);

            if (prop == NULL) {
                break;
            }

            if (!njs_is_enumerable(&prop->name, type)) {
                continue;
            }

            njs_object_property_key_set(&lhq, &prop->name, lhe.key_hash);

            ext_prop = njs_object_exist_in_proto(parent, object, &lhq);

            if (ext_prop == NULL && prop->type != NJS_WHITEOUT
                && (prop->enumerable || all))
            {
                entry = njs_array_alloc(vm, 0, 2, 0);
                if (njs_slow_path(entry == NULL)) {
                    return NJS_ERROR;
                }

                njs_string_copy(&entry->start[0], &prop->name);
                entry->start[1] = prop->value;

                njs_set_array(&value, entry);

                ret = njs_array_add(vm, items, &value);
                if (njs_slow_path(ret != NJS_OK)) {
                    return NJS_ERROR;
                }
            }
        }

        njs_lvlhsh_each_init(&lhe, &njs_object_hash_proto);
        hash = &object->shared_hash;

        for ( ;; ) {
            prop = njs_lvlhsh_each(hash, &lhe);

            if (prop == NULL) {
                break;
            }

            if (!njs_is_enumerable(&prop->name, type)) {
                continue;
            }

            njs_object_property_key_set(&lhq, &prop->name, lhe.key_hash);

            ret = njs_lvlhsh_find(&object->hash, &lhq);

            if (ret != NJS_OK && (prop->enumerable || all)) {
                ext_prop = njs_object_exist_in_proto(parent, object, &lhq);

                if (ext_prop == NULL) {
                    entry = njs_array_alloc(vm, 0, 2, 0);
                    if (njs_slow_path(entry == NULL)) {
                        return NJS_ERROR;
                    }

                    njs_string_copy(&entry->start[0], &prop->name);
                    entry->start[1] = prop->value;

                    njs_set_array(&value, entry);

                    ret = njs_array_add(vm, items, &value);
                    if (njs_slow_path(ret != NJS_OK)) {
                        return NJS_ERROR;
                    }
                }
            }
        }

        break;
    }

    return NJS_OK;
}


njs_inline njs_int_t
njs_traverse_visit(njs_arr_t *list, const njs_value_t *value)
{
    njs_object_t  **p;

    if (njs_is_object(value)) {
        p = njs_arr_add(list);
        if (njs_slow_path(p == NULL)) {
            return NJS_ERROR;
        }

        *p = njs_object(value);
    }

    return NJS_OK;
}


njs_inline njs_int_t
njs_traverse_visited(njs_arr_t *list, const njs_value_t *value)
{
    njs_uint_t    items, n;
    njs_object_t  **start, *obj;

    if (!njs_is_object(value)) {
        /* External. */
        return 0;
    }

    start = list->start;
    items = list->items;
    obj = njs_object(value);

    for (n = 0; n < items; n++) {
        if (start[n] == obj) {
            return 1;
        }
    }

    return 0;
}


njs_int_t
njs_object_traverse(njs_vm_t *vm, njs_object_t *object, void *ctx,
    njs_object_traverse_cb_t cb)
{
    njs_int_t          depth, ret;
    njs_str_t          name;
    njs_arr_t          visited;
    njs_object_t       **start;
    njs_value_t        value, obj;
    njs_object_prop_t  *prop;
    njs_traverse_t     state[NJS_TRAVERSE_MAX_DEPTH];

    depth = 0;

    state[depth].prop = NULL;
    state[depth].parent = NULL;
    state[depth].object = object;
    state[depth].hash = &object->shared_hash;
    njs_lvlhsh_each_init(&state[depth].lhe, &njs_object_hash_proto);

    start = njs_arr_init(vm->mem_pool, &visited, NULL, 8, sizeof(void *));
    if (njs_slow_path(start == NULL)) {
        return NJS_ERROR;
    }

    njs_set_object(&value, object);
    (void) njs_traverse_visit(&visited, &value);

    for ( ;; ) {
        prop = njs_lvlhsh_each(state[depth].hash, &state[depth].lhe);

        if (prop == NULL) {
            if (state[depth].hash == &state[depth].object->shared_hash) {
                state[depth].hash = &state[depth].object->hash;
                njs_lvlhsh_each_init(&state[depth].lhe, &njs_object_hash_proto);
                continue;
            }

            if (depth == 0) {
                goto done;
            }

            depth--;
            continue;
        }

        state[depth].prop = prop;

        ret = cb(vm, &state[depth], ctx);
        if (njs_slow_path(ret != NJS_OK)) {
            return ret;
        }

        value = prop->value;

        if (prop->type == NJS_PROPERTY_HANDLER) {
            njs_set_object(&obj, state[depth].object);
            ret = prop->value.data.u.prop_handler(vm, prop, &obj, NULL, &value);
            if (njs_slow_path(ret == NJS_ERROR)) {
                return ret;

            }
        }

        njs_string_get(&prop->name, &name);

        if (njs_is_object(&value) && !njs_traverse_visited(&visited, &value)) {
            ret = njs_traverse_visit(&visited, &value);
            if (njs_slow_path(ret != NJS_OK)) {
                return NJS_ERROR;
            }

            if (++depth > (NJS_TRAVERSE_MAX_DEPTH - 1)) {
                njs_type_error(vm, "njs_object_traverse() recursion limit:%d",
                               depth);
                return NJS_ERROR;
            }

            state[depth].prop = NULL;
            state[depth].parent = &state[depth - 1];
            state[depth].object = njs_object(&value);
            state[depth].hash = &njs_object(&value)->shared_hash;
            njs_lvlhsh_each_init(&state[depth].lhe, &njs_object_hash_proto);
        }

    }

done:

    njs_arr_destroy(&visited);

    return NJS_OK;
}


static njs_int_t
njs_object_define_property(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_int_t    ret;
    njs_value_t  *value, *name, *desc, lvalue;

    if (!njs_is_object(njs_arg(args, nargs, 1))) {
        njs_type_error(vm, "Object.defineProperty is called on non-object");
        return NJS_ERROR;
    }

    desc = njs_arg(args, nargs, 3);

    if (!njs_is_object(desc)) {
        njs_type_error(vm, "descriptor is not an object");
        return NJS_ERROR;
    }

    value = njs_argument(args, 1);
    name = njs_lvalue_arg(&lvalue, args, nargs, 2);

    ret = njs_object_prop_define(vm, value, name, desc,
                                 NJS_OBJECT_PROP_DESCRIPTOR);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    vm->retval = *value;

    return NJS_OK;
}


static njs_int_t
njs_object_define_properties(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    uint32_t              i, length;
    njs_int_t             ret;
    njs_array_t           *keys;
    njs_value_t           desc, *value, *descs;
    njs_object_prop_t     *prop;
    njs_property_query_t  pq;

    if (!njs_is_object(njs_arg(args, nargs, 1))) {
        njs_type_error(vm, "Object.defineProperties is called on non-object");
        return NJS_ERROR;
    }

    descs = njs_arg(args, nargs, 2);
    ret = njs_value_to_object(vm, descs);
    if (njs_slow_path(ret != NJS_OK)) {
        return ret;
    }

    keys = njs_value_own_enumerate(vm, descs, NJS_ENUM_KEYS,
                                   NJS_ENUM_STRING | NJS_ENUM_SYMBOL, 0);
    if (njs_slow_path(keys == NULL)) {
        return NJS_ERROR;
    }

    length = keys->length;
    value = njs_argument(args, 1);
    njs_property_query_init(&pq, NJS_PROPERTY_QUERY_GET, 0);

    for (i = 0; i < length; i++) {
        ret = njs_property_query(vm, &pq, descs, &keys->start[i]);
        if (njs_slow_path(ret == NJS_ERROR)) {
            goto done;
        }

        prop = pq.lhq.value;

        if (ret == NJS_DECLINED || !prop->enumerable) {
            continue;
        }

        ret = njs_value_property(vm, descs, &keys->start[i], &desc);
        if (njs_slow_path(ret == NJS_ERROR)) {
            goto done;
        }

        ret = njs_object_prop_define(vm, value, &keys->start[i], &desc,
                                     NJS_OBJECT_PROP_DESCRIPTOR);
        if (njs_slow_path(ret != NJS_OK)) {
            goto done;
        }
    }

    ret = NJS_OK;
    vm->retval = *value;

done:

    njs_array_destroy(vm, keys);

    return ret;
}


static njs_int_t
njs_object_get_own_property_descriptor(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t unused)
{
    njs_value_t  lvalue, *value, *property;

    value = njs_arg(args, nargs, 1);

    if (njs_is_null_or_undefined(value)) {
        njs_type_error(vm, "cannot convert %s argument to object",
                       njs_type_string(value->type));
        return NJS_ERROR;
    }

    property = njs_lvalue_arg(&lvalue, args, nargs, 2);

    return njs_object_prop_descriptor(vm, &vm->retval, value, property);
}


static njs_int_t
njs_object_get_own_property_descriptors(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t unused)
{
    njs_int_t           ret;
    uint32_t            i, length;
    njs_array_t         *names;
    njs_value_t         descriptor, *value, *key;
    njs_object_t        *descriptors;
    njs_object_prop_t   *pr;
    njs_lvlhsh_query_t  lhq;

    value = njs_arg(args, nargs, 1);

    if (njs_is_null_or_undefined(value)) {
        njs_type_error(vm, "cannot convert %s argument to object",
                       njs_type_string(value->type));

        return NJS_ERROR;
    }

    names = njs_value_own_enumerate(vm, value, NJS_ENUM_KEYS,
                                    NJS_ENUM_STRING | NJS_ENUM_SYMBOL, 1);
    if (njs_slow_path(names == NULL)) {
        return NJS_ERROR;
    }

    length = names->length;

    descriptors = njs_object_alloc(vm);
    if (njs_slow_path(descriptors == NULL)) {
        ret = NJS_ERROR;
        goto done;
    }

    lhq.replace = 0;
    lhq.pool = vm->mem_pool;
    lhq.proto = &njs_object_hash_proto;

    for (i = 0; i < length; i++) {
        key = &names->start[i];
        ret = njs_object_prop_descriptor(vm, &descriptor, value, key);
        if (njs_slow_path(ret != NJS_OK)) {
            ret = NJS_ERROR;
            goto done;
        }

        pr = njs_object_prop_alloc(vm, key, &descriptor, 1);
        if (njs_slow_path(pr == NULL)) {
            ret = NJS_ERROR;
            goto done;
        }

        njs_object_property_key_set(&lhq, key, 0);
        lhq.value = pr;

        ret = njs_lvlhsh_insert(&descriptors->hash, &lhq);
        if (njs_slow_path(ret != NJS_OK)) {
            njs_internal_error(vm, "lvlhsh insert failed");
            goto done;
        }
    }

    ret = NJS_OK;
    njs_set_object(&vm->retval, descriptors);

done:

    njs_array_destroy(vm, names);

    return ret;
}


static njs_int_t
njs_object_get_own_property(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t type)
{
    njs_array_t  *names;
    njs_value_t  *value;

    value = njs_arg(args, nargs, 1);

    if (njs_is_null_or_undefined(value)) {
        njs_type_error(vm, "cannot convert %s argument to object",
                       njs_type_string(value->type));

        return NJS_ERROR;
    }

    names = njs_value_own_enumerate(vm, value, NJS_ENUM_KEYS,
                                    type, 1);
    if (names == NULL) {
        return NJS_ERROR;
    }

    njs_set_array(&vm->retval, names);

    return NJS_OK;
}


static njs_int_t
njs_object_get_prototype_of(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    uint32_t     index, type;
    njs_value_t  *value;

    value = njs_arg(args, nargs, 1);

    if (njs_is_object(value)) {
        njs_object_prototype_proto(vm, NULL, value, NULL, &vm->retval);
        return NJS_OK;
    }

    if (!njs_is_null_or_undefined(value)) {
        index = njs_primitive_prototype_index(value->type);
        type = njs_is_symbol(value) ? NJS_OBJECT
                                    : njs_object_value_type(value->type);

        njs_set_type_object(&vm->retval, &vm->prototypes[index].object, type);

        return NJS_OK;
    }

    njs_type_error(vm, "cannot convert %s argument to object",
                   njs_type_string(value->type));

    return NJS_ERROR;
}


static njs_int_t
njs_object_set_prototype_of(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_int_t    ret;
    njs_value_t  *value, *proto;

    value = njs_arg(args, nargs, 1);
    if (njs_slow_path(njs_is_null_or_undefined(value))) {
        njs_type_error(vm, "cannot convert %s argument to object",
                       njs_type_string(value->type));
        return NJS_ERROR;
    }

    proto = njs_arg(args, nargs, 2);
    if (njs_slow_path(!njs_is_object(proto) && !njs_is_null(proto))) {
        njs_type_error(vm, "prototype may only be an object or null: %s",
                       njs_type_string(proto->type));
        return NJS_ERROR;
    }

    if (njs_slow_path(!njs_is_object(value))) {
        vm->retval = *value;

        return NJS_OK;
    }

    ret = njs_object_set_prototype(vm, njs_object(value), proto);
    if (njs_fast_path(ret == NJS_OK)) {
        vm->retval = *value;

        return NJS_OK;
    }

    if (ret == NJS_DECLINED) {
        njs_type_error(vm, "Cannot set property \"prototype\", "
                       "object is not extensible");

    } else {
        njs_type_error(vm, "Cyclic __proto__ value");
    }

    return NJS_ERROR;
}


static njs_int_t
njs_object_freeze(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_value_t        *value;
    njs_lvlhsh_t       *hash;
    njs_object_t       *object;
    njs_object_prop_t  *prop;
    njs_lvlhsh_each_t  lhe;

    value = njs_arg(args, nargs, 1);

    if (!njs_is_object(value)) {
        njs_set_undefined(&vm->retval);
        return NJS_OK;
    }

    object = njs_object(value);
    object->extensible = 0;

    njs_lvlhsh_each_init(&lhe, &njs_object_hash_proto);

    hash = &object->hash;

    for ( ;; ) {
        prop = njs_lvlhsh_each(hash, &lhe);

        if (prop == NULL) {
            break;
        }

        if (!njs_is_accessor_descriptor(prop)) {
            prop->writable = 0;
        }

        prop->configurable = 0;
    }

    vm->retval = *value;

    return NJS_OK;
}


static njs_int_t
njs_object_is_frozen(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_value_t        *value;
    njs_lvlhsh_t       *hash;
    njs_object_t       *object;
    njs_object_prop_t  *prop;
    njs_lvlhsh_each_t  lhe;
    const njs_value_t  *retval;

    value = njs_arg(args, nargs, 1);

    if (!njs_is_object(value)) {
        vm->retval = njs_value_true;
        return NJS_OK;
    }

    retval = &njs_value_false;

    object = njs_object(value);
    njs_lvlhsh_each_init(&lhe, &njs_object_hash_proto);

    hash = &object->hash;

    if (object->extensible) {
        goto done;
    }

    for ( ;; ) {
        prop = njs_lvlhsh_each(hash, &lhe);

        if (prop == NULL) {
            break;
        }

        if (prop->configurable) {
            goto done;
        }

        if (njs_is_data_descriptor(prop) && prop->writable) {
            goto done;
        }
    }

    retval = &njs_value_true;

done:

    vm->retval = *retval;

    return NJS_OK;
}


static njs_int_t
njs_object_seal(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_value_t        *value;
    njs_lvlhsh_t       *hash;
    njs_object_t       *object;
    njs_object_prop_t  *prop;
    njs_lvlhsh_each_t  lhe;

    value = njs_arg(args, nargs, 1);

    if (!njs_is_object(value)) {
        vm->retval = *value;
        return NJS_OK;
    }

    object = njs_object(value);
    object->extensible = 0;

    njs_lvlhsh_each_init(&lhe, &njs_object_hash_proto);

    hash = &object->hash;

    for ( ;; ) {
        prop = njs_lvlhsh_each(hash, &lhe);

        if (prop == NULL) {
            break;
        }

        prop->configurable = 0;
    }

    vm->retval = *value;

    return NJS_OK;
}


static njs_int_t
njs_object_is_sealed(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_value_t        *value;
    njs_lvlhsh_t       *hash;
    njs_object_t       *object;
    njs_object_prop_t  *prop;
    njs_lvlhsh_each_t  lhe;
    const njs_value_t  *retval;

    value = njs_arg(args, nargs, 1);

    if (!njs_is_object(value)) {
        vm->retval = njs_value_true;
        return NJS_OK;
    }

    retval = &njs_value_false;

    object = njs_object(value);
    njs_lvlhsh_each_init(&lhe, &njs_object_hash_proto);

    hash = &object->hash;

    if (object->extensible) {
        goto done;
    }

    for ( ;; ) {
        prop = njs_lvlhsh_each(hash, &lhe);

        if (prop == NULL) {
            break;
        }

        if (prop->configurable) {
            goto done;
        }
    }

    retval = &njs_value_true;

done:

    vm->retval = *retval;

    return NJS_OK;
}


static njs_int_t
njs_object_prevent_extensions(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_value_t  *value;

    value = njs_arg(args, nargs, 1);

    if (!njs_is_object(value)) {
        vm->retval = *value;
        return NJS_OK;
    }

    njs_object(&args[1])->extensible = 0;

    vm->retval = *value;

    return NJS_OK;
}


static njs_int_t
njs_object_is_extensible(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_value_t        *value;
    const njs_value_t  *retval;

    value = njs_arg(args, nargs, 1);

    if (!njs_is_object(value)) {
        vm->retval = njs_value_false;
        return NJS_OK;
    }

    retval = njs_object(value)->extensible ? &njs_value_true
                                           : &njs_value_false;

    vm->retval = *retval;

    return NJS_OK;
}


static njs_int_t
njs_object_assign(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    uint32_t              i, j, length;
    njs_int_t             ret;
    njs_array_t           *names;
    njs_value_t           *key, *source, *value, setval;
    njs_object_prop_t     *prop;
    njs_property_query_t  pq;

    value = njs_arg(args, nargs, 1);

    ret = njs_value_to_object(vm, value);
    if (njs_slow_path(ret != NJS_OK)) {
        return ret;
    }

    names = NULL;

    for (i = 2; i < nargs; i++) {
        source = &args[i];

        names = njs_value_own_enumerate(vm, source, NJS_ENUM_KEYS,
                                        NJS_ENUM_STRING | NJS_ENUM_SYMBOL, 1);
        if (njs_slow_path(names == NULL)) {
            return NJS_ERROR;
        }

        length = names->length;

        for (j = 0; j < length; j++) {
            key = &names->start[j];

            njs_property_query_init(&pq, NJS_PROPERTY_QUERY_GET, 1);

            ret = njs_property_query(vm, &pq, source, key);
            if (njs_slow_path(ret != NJS_OK)) {
                goto exception;
            }

            prop = pq.lhq.value;
            if (!prop->enumerable) {
                continue;
            }

            ret = njs_value_property(vm, source, key, &setval);
            if (njs_slow_path(ret != NJS_OK)) {
                goto exception;
            }

            ret = njs_value_property_set(vm, value, key, &setval);
            if (njs_slow_path(ret != NJS_OK)) {
                goto exception;
            }
        }

        njs_array_destroy(vm, names);
    }

    vm->retval = *value;

    return NJS_OK;

exception:

    njs_array_destroy(vm, names);

    return NJS_ERROR;
}


static njs_int_t
njs_object_is(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_set_boolean(&vm->retval, njs_values_same(njs_arg(args, nargs, 1),
                                                 njs_arg(args, nargs, 2)));

    return NJS_OK;
}


/*
 * The __proto__ property of booleans, numbers and strings primitives,
 * of objects created by Boolean(), Number(), and String() constructors,
 * and of Boolean.prototype, Number.prototype, and String.prototype objects.
 */

njs_int_t
njs_primitive_prototype_get_proto(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    njs_uint_t    index;
    njs_object_t  *proto;

    /*
     * The __proto__ getters reside in object prototypes of primitive types
     * and have to return different results for primitive type and for objects.
     */
    if (njs_is_object(value)) {
        proto = njs_object(value)->__proto__;

    } else {
        index = njs_primitive_prototype_index(value->type);
        proto = &vm->prototypes[index].object;
    }

    if (proto != NULL) {
        njs_set_type_object(retval, proto, proto->type);

    } else {
        njs_set_undefined(retval);
    }

    return NJS_OK;
}


/*
 * The "prototype" property of Object(), Array() and other functions is
 * created on demand in the functions' private hash by the "prototype"
 * getter.  The properties are set to appropriate prototype.
 */

njs_int_t
njs_object_prototype_create(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    int32_t            index;
    njs_function_t     *function;
    const njs_value_t  *proto;

    proto = NULL;
    function = njs_function(value);
    index = function - vm->constructors;

    if (index >= 0 && index < NJS_OBJ_TYPE_MAX) {
        proto = njs_property_prototype_create(vm, &function->object.hash,
                                              &vm->prototypes[index].object);
    }

    if (proto == NULL) {
        proto = &njs_value_undefined;
    }

    *retval = *proto;

    return NJS_OK;
}


njs_value_t *
njs_property_prototype_create(njs_vm_t *vm, njs_lvlhsh_t *hash,
    njs_object_t *prototype)
{
    njs_int_t           ret;
    njs_object_prop_t   *prop;
    njs_lvlhsh_query_t  lhq;

    static const njs_value_t  proto_string = njs_string("prototype");

    prop = njs_object_prop_alloc(vm, &proto_string, &njs_value_undefined, 0);
    if (njs_slow_path(prop == NULL)) {
        return NULL;
    }

    /* GC */

    njs_set_type_object(&prop->value, prototype, prototype->type);

    lhq.value = prop;
    lhq.key_hash = NJS_PROTOTYPE_HASH;
    lhq.key = njs_str_value("prototype");
    lhq.replace = 1;
    lhq.pool = vm->mem_pool;
    lhq.proto = &njs_object_hash_proto;

    ret = njs_lvlhsh_insert(hash, &lhq);

    if (njs_fast_path(ret == NJS_OK)) {
        return &prop->value;
    }

    njs_internal_error(vm, "lvlhsh insert failed");

    return NULL;
}


static const njs_object_prop_t  njs_object_constructor_properties[] =
{
    {
        .type = NJS_PROPERTY,
        .name = njs_string("name"),
        .value = njs_string("Object"),
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

    {
        .type = NJS_PROPERTY,
        .name = njs_string("create"),
        .value = njs_native_function(njs_object_create, 2),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("keys"),
        .value = njs_native_function(njs_object_keys, 1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("values"),
        .value = njs_native_function(njs_object_values, 1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("entries"),
        .value = njs_native_function(njs_object_entries, 1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("defineProperty"),
        .value = njs_native_function(njs_object_define_property, 3),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_long_string("defineProperties"),
        .value = njs_native_function(njs_object_define_properties, 2),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_long_string("getOwnPropertyDescriptor"),
        .value = njs_native_function(njs_object_get_own_property_descriptor, 2),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_long_string("getOwnPropertyDescriptors"),
        .value = njs_native_function(njs_object_get_own_property_descriptors,
                                     1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_long_string("getOwnPropertyNames"),
        .value = njs_native_function2(njs_object_get_own_property, 1,
                                      NJS_ENUM_STRING),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_long_string("getOwnPropertySymbols"),
        .value = njs_native_function2(njs_object_get_own_property, 1,
                                      NJS_ENUM_SYMBOL),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("getPrototypeOf"),
        .value = njs_native_function(njs_object_get_prototype_of, 1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("setPrototypeOf"),
        .value = njs_native_function(njs_object_set_prototype_of, 2),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("freeze"),
        .value = njs_native_function(njs_object_freeze, 1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("isFrozen"),
        .value = njs_native_function(njs_object_is_frozen, 1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("seal"),
        .value = njs_native_function(njs_object_seal, 1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("isSealed"),
        .value = njs_native_function(njs_object_is_sealed, 1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_long_string("preventExtensions"),
        .value = njs_native_function(njs_object_prevent_extensions, 1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("isExtensible"),
        .value = njs_native_function(njs_object_is_extensible, 1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("assign"),
        .value = njs_native_function(njs_object_assign, 2),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("is"),
        .value = njs_native_function(njs_object_is, 2),
        .writable = 1,
        .configurable = 1,
    },
};


const njs_object_init_t  njs_object_constructor_init = {
    njs_object_constructor_properties,
    njs_nitems(njs_object_constructor_properties),
};


static njs_int_t
njs_object_set_prototype(njs_vm_t *vm, njs_object_t *object,
    const njs_value_t *value)
{
    const njs_object_t *proto;

    proto = njs_object(value);

    if (njs_slow_path(object->__proto__ == proto)) {
        return NJS_OK;
    }

    if (!object->extensible) {
        return NJS_DECLINED;
    }

    if (njs_slow_path(proto == NULL)) {
        object->__proto__ = NULL;
        return NJS_OK;
    }

    do {
        if (proto == object) {
            return NJS_ERROR;
        }

        proto = proto->__proto__;

    } while (proto != NULL);

    object->__proto__ = njs_object(value);

    return NJS_OK;
}


njs_int_t
njs_object_prototype_proto(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    njs_int_t     ret;
    njs_object_t  *proto, *object;

    if (!njs_is_object(value)) {
        *retval = *value;
        return NJS_OK;
    }

    object = njs_object(value);

    if (setval != NULL) {
        if (njs_is_object(setval) || njs_is_null(setval)) {
            ret = njs_object_set_prototype(vm, object, setval);
            if (njs_slow_path(ret == NJS_ERROR)) {
                njs_type_error(vm, "Cyclic __proto__ value");
                return NJS_ERROR;
            }
        }

        njs_set_undefined(retval);

        return NJS_OK;
    }

    proto = object->__proto__;

    if (njs_fast_path(proto != NULL)) {
        njs_set_type_object(retval, proto, proto->type);

    } else {
        *retval = njs_value_null;
    }

    return NJS_OK;
}


/*
 * The "constructor" property of Object(), Array() and other functions
 * prototypes is created on demand in the prototypes' private hash by the
 * "constructor" getter.  The properties are set to appropriate function.
 */

njs_int_t
njs_object_prototype_create_constructor(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    int32_t                 index;
    njs_value_t             *cons, constructor;
    njs_object_t            *object;
    njs_object_prototype_t  *prototype;

    if (njs_is_object(value)) {
        object = njs_object(value);

        do {
            prototype = (njs_object_prototype_t *) object;
            index = prototype - vm->prototypes;

            if (index >= 0 && index < NJS_OBJ_TYPE_MAX) {
                goto found;
            }

            object = object->__proto__;

        } while (object != NULL);

        njs_thread_log_alert("prototype not found");

        return NJS_ERROR;

    } else {
        index = njs_primitive_prototype_index(value->type);
        prototype = &vm->prototypes[index];
    }

found:

    if (setval == NULL) {
        njs_set_function(&constructor, &vm->constructors[index]);
        setval = &constructor;
    }

    cons = njs_property_constructor_create(vm, &prototype->object.hash, setval);
    if (njs_fast_path(cons != NULL)) {
        *retval = *cons;
        return NJS_OK;
    }

    return NJS_ERROR;
}


njs_value_t *
njs_property_constructor_create(njs_vm_t *vm, njs_lvlhsh_t *hash,
    njs_value_t *constructor)
{
    njs_int_t                 ret;
    njs_object_prop_t         *prop;
    njs_lvlhsh_query_t        lhq;

    static const njs_value_t  constructor_string = njs_string("constructor");

    prop = njs_object_prop_alloc(vm, &constructor_string, constructor, 1);
    if (njs_slow_path(prop == NULL)) {
        return NULL;
    }

    /* GC */

    prop->value = *constructor;
    prop->enumerable = 0;

    lhq.value = prop;
    lhq.key_hash = NJS_CONSTRUCTOR_HASH;
    lhq.key = njs_str_value("constructor");
    lhq.replace = 1;
    lhq.pool = vm->mem_pool;
    lhq.proto = &njs_object_hash_proto;

    ret = njs_lvlhsh_insert(hash, &lhq);

    if (njs_fast_path(ret == NJS_OK)) {
        return &prop->value;
    }

    njs_internal_error(vm, "lvlhsh insert/replace failed");

    return NULL;
}


static njs_int_t
njs_object_prototype_value_of(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    vm->retval = *njs_argument(args, 0);

    if (!njs_is_object(&vm->retval)) {
        return njs_value_to_object(vm, &vm->retval);
    }

    return NJS_OK;
}


static const njs_value_t  njs_object_null_string = njs_string("[object Null]");
static const njs_value_t  njs_object_undefined_string =
                                     njs_long_string("[object Undefined]");
static const njs_value_t  njs_object_boolean_string =
                                     njs_long_string("[object Boolean]");
static const njs_value_t  njs_object_number_string =
                                     njs_long_string("[object Number]");
static const njs_value_t  njs_object_symbol_string =
                                     njs_long_string("[object Symbol]");
static const njs_value_t  njs_object_string_string =
                                     njs_long_string("[object String]");
static const njs_value_t  njs_object_data_string =
                                     njs_string("[object Data]");
static const njs_value_t  njs_object_exernal_string =
                                     njs_long_string("[object External]");
static const njs_value_t  njs_object_object_string =
                                     njs_long_string("[object Object]");
static const njs_value_t  njs_object_array_string =
                                     njs_string("[object Array]");
static const njs_value_t  njs_object_function_string =
                                     njs_long_string("[object Function]");
static const njs_value_t  njs_object_regexp_string =
                                     njs_long_string("[object RegExp]");
static const njs_value_t  njs_object_date_string = njs_string("[object Date]");
static const njs_value_t  njs_object_error_string =
                                     njs_string("[object Error]");
static const njs_value_t  njs_object_arguments_string =
                                     njs_long_string("[object Arguments]");


njs_int_t
njs_object_prototype_to_string(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t unused)
{
    u_char             *p;
    njs_int_t          ret;
    njs_value_t        tag, *value;
    njs_string_prop_t  string;
    const njs_value_t  *name;

    static const njs_value_t  *class_name[NJS_VALUE_TYPE_MAX] = {
        /* Primitives. */
        &njs_object_null_string,
        &njs_object_undefined_string,
        &njs_object_boolean_string,
        &njs_object_number_string,
        &njs_object_symbol_string,
        &njs_object_string_string,

        &njs_object_data_string,
        &njs_object_exernal_string,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,

        /* Objects. */
        &njs_object_object_string,
        &njs_object_array_string,
        &njs_object_boolean_string,
        &njs_object_number_string,
        &njs_object_symbol_string,
        &njs_object_string_string,
        &njs_object_function_string,
        &njs_object_regexp_string,
        &njs_object_date_string,
        &njs_object_object_string,
        &njs_object_object_string,
        &njs_object_object_string,
        &njs_object_object_string,
    };

    value = njs_argument(args, 0);
    name = class_name[value->type];

    if (njs_is_null_or_undefined(value)) {
        vm->retval = *name;

        return NJS_OK;
    }

    if (njs_is_error(value)) {
        name = &njs_object_error_string;
    }

    if (njs_is_object(value)
        && njs_lvlhsh_eq(&njs_object(value)->shared_hash,
                         &vm->shared->arguments_object_instance_hash))
    {
        name = &njs_object_arguments_string;
    }

    ret = njs_object_string_tag(vm, value, &tag);
    if (njs_slow_path(ret == NJS_ERROR)) {
        return ret;
    }

    if (ret == NJS_DECLINED) {
        if (njs_slow_path(name == NULL)) {
            njs_internal_error(vm, "Unknown value type");

            return NJS_ERROR;
        }

        vm->retval = *name;

        return NJS_OK;
    }

    (void) njs_string_prop(&string, &tag);

    p = njs_string_alloc(vm, &vm->retval, string.size + njs_length("[object ]"),
                         string.length + njs_length("[object ]"));
    if (njs_slow_path(p == NULL)) {
        return NJS_ERROR;
    }

    p = njs_cpymem(p, "[object ", 8);
    p = njs_cpymem(p, string.start, string.size);
    *p = ']';

    return NJS_OK;
}


static njs_int_t
njs_object_prototype_has_own_property(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t unused)
{
    njs_int_t             ret;
    njs_value_t           *value, *property;
    njs_property_query_t  pq;

    value = njs_arg(args, nargs, 0);

    if (njs_is_null_or_undefined(value)) {
        njs_type_error(vm, "cannot convert %s argument to object",
                       njs_type_string(value->type));
        return NJS_ERROR;
    }

    property = njs_arg(args, nargs, 1);

    njs_property_query_init(&pq, NJS_PROPERTY_QUERY_GET, 1);

    ret = njs_property_query(vm, &pq, value, property);

    switch (ret) {
    case NJS_OK:
        vm->retval = njs_value_true;
        return NJS_OK;

    case NJS_DECLINED:
        vm->retval = njs_value_false;
        return NJS_OK;

    case NJS_ERROR:
    default:
        return ret;
    }
}


static njs_int_t
njs_object_prototype_prop_is_enumerable(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t unused)
{
    njs_int_t             ret;
    njs_value_t           *value, *property;
    const njs_value_t     *retval;
    njs_object_prop_t     *prop;
    njs_property_query_t  pq;

    value = njs_arg(args, nargs, 0);

    if (njs_is_null_or_undefined(value)) {
        njs_type_error(vm, "cannot convert %s argument to object",
                       njs_type_string(value->type));
        return NJS_ERROR;
    }

    property = njs_arg(args, nargs, 1);

    njs_property_query_init(&pq, NJS_PROPERTY_QUERY_GET, 1);

    ret = njs_property_query(vm, &pq, value, property);

    switch (ret) {
    case NJS_OK:
        prop = pq.lhq.value;
        retval = prop->enumerable ? &njs_value_true : &njs_value_false;
        break;

    case NJS_DECLINED:
        retval = &njs_value_false;
        break;

    case NJS_ERROR:
    default:
        return ret;
    }

    vm->retval = *retval;

    return NJS_OK;
}


static njs_int_t
njs_object_prototype_is_prototype_of(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t unused)
{
    njs_value_t        *prototype, *value;
    njs_object_t       *object, *proto;
    const njs_value_t  *retval;

    if (njs_slow_path(njs_is_null_or_undefined(njs_arg(args, nargs, 0)))) {
        njs_type_error(vm, "cannot convert undefined to object");
        return NJS_ERROR;
    }

    retval = &njs_value_false;
    prototype = &args[0];
    value = njs_arg(args, nargs, 1);

    if (njs_is_object(prototype) && njs_is_object(value)) {
        proto = njs_object(prototype);
        object = njs_object(value);

        do {
            object = object->__proto__;

            if (object == proto) {
                retval = &njs_value_true;
                break;
            }

        } while (object != NULL);
    }

    vm->retval = *retval;

    return NJS_OK;
}


static const njs_object_prop_t  njs_object_prototype_properties[] =
{
    {
        .type = NJS_PROPERTY_HANDLER,
        .name = njs_string("__proto__"),
        .value = njs_prop_handler(njs_object_prototype_proto),
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
        .name = njs_string("valueOf"),
        .value = njs_native_function(njs_object_prototype_value_of, 0),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("toString"),
        .value = njs_native_function(njs_object_prototype_to_string, 0),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("hasOwnProperty"),
        .value = njs_native_function(njs_object_prototype_has_own_property, 1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_long_string("propertyIsEnumerable"),
        .value = njs_native_function(njs_object_prototype_prop_is_enumerable,
                                     1),
        .writable = 1,
        .configurable = 1,
    },

    {
        .type = NJS_PROPERTY,
        .name = njs_string("isPrototypeOf"),
        .value = njs_native_function(njs_object_prototype_is_prototype_of, 1),
        .writable = 1,
        .configurable = 1,
    },
};


const njs_object_init_t  njs_object_prototype_init = {
    njs_object_prototype_properties,
    njs_nitems(njs_object_prototype_properties),
};


njs_int_t
njs_object_length(njs_vm_t *vm, njs_value_t *value, int64_t *length)
{
    njs_int_t    ret;
    njs_value_t  value_length;

    const njs_value_t  string_length = njs_string("length");

    ret = njs_value_property(vm, value, njs_value_arg(&string_length),
                             &value_length);
    if (njs_slow_path(ret == NJS_ERROR)) {
        return ret;
    }

    return njs_value_to_length(vm, &value_length, length);
}


const njs_object_type_init_t  njs_obj_type_init = {
    .constructor = njs_native_ctor(njs_object_constructor, 1, 0),
    .constructor_props = &njs_object_constructor_init,
    .prototype_props = &njs_object_prototype_init,
    .prototype_value = { .object = { .type = NJS_OBJECT } },
};
