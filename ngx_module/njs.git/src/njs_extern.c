
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */


#include <njs_main.h>


static njs_int_t njs_external_prop_handler(njs_vm_t *vm,
    njs_object_prop_t *self, njs_value_t *value, njs_value_t *setval,
    njs_value_t *retval);


static njs_int_t
njs_external_add(njs_vm_t *vm, njs_arr_t *protos,
    const njs_external_t *external, njs_uint_t n)
{
    size_t              size;
    ssize_t             length;
    njs_int_t           ret;
    njs_lvlhsh_t        *hash;
    const u_char        *start;
    njs_function_t      *function;
    njs_object_prop_t   *prop;
    njs_lvlhsh_query_t  lhq;
    njs_exotic_slots_t  *slot, *next;

    slot = njs_arr_add(protos);
    njs_memzero(slot, sizeof(njs_exotic_slots_t));

    hash = &slot->external_shared_hash;
    njs_lvlhsh_init(hash);

    lhq.replace = 0;
    lhq.proto = &njs_object_hash_proto;
    lhq.pool = vm->mem_pool;

    while (n != 0) {
        prop = njs_object_prop_alloc(vm, &njs_string_empty,
                                     &njs_value_invalid, 1);
        if (njs_slow_path(prop == NULL)) {
            goto memory_error;
        }

        prop->writable = external->writable;
        prop->configurable = external->configurable;
        prop->enumerable = external->enumerable;

        if (external->flags & 4) {
            njs_set_symbol(&prop->name, external->name.symbol);

            lhq.key_hash = external->name.symbol;

        } else {
            ret = njs_string_set(vm, &prop->name, external->name.string.start,
                                 external->name.string.length);
            if (njs_slow_path(ret != NJS_OK)) {
                return NJS_ERROR;
            }

            lhq.key = external->name.string;
            lhq.key_hash = njs_djb_hash(lhq.key.start, lhq.key.length);
        }

        lhq.value = prop;

        switch (external->flags & 3) {
        case NJS_EXTERN_METHOD:
            function = njs_mp_zalloc(vm->mem_pool, sizeof(njs_function_t));
            if (njs_slow_path(function == NULL)) {
                goto memory_error;
            }

            function->object.shared_hash = vm->shared->arrow_instance_hash;
            function->object.type = NJS_FUNCTION;
            function->object.shared = 1;
            function->object.extensible = 1;
            function->args_offset = 1;
            function->native = 1;
            function->magic8 = external->u.method.magic8;
            function->u.native = external->u.method.native;

            njs_set_function(&prop->value, function);

            break;

        case NJS_EXTERN_PROPERTY:
            if (external->u.property.handler != NULL) {
                prop->type = NJS_PROPERTY_HANDLER;
                prop->value.type = NJS_INVALID;
                prop->value.data.truth = 1;
                prop->value.data.magic16 = 0;
                prop->value.data.magic32 = external->u.property.magic32;
                prop->value.data.u.prop_handler = external->u.property.handler;

            } else {
                start = (u_char *) external->u.property.value;
                size = njs_strlen(start);
                length = njs_utf8_length(start, size);
                if (njs_slow_path(length < 0)) {
                    length = 0;
                }

                ret = njs_string_new(vm, &prop->value, start, size, length);
                if (njs_slow_path(ret != NJS_OK)) {
                    return NJS_ERROR;
                }
            }

            break;

        case NJS_EXTERN_OBJECT:
        default:
            next = njs_arr_item(protos, protos->items);

            ret = njs_external_add(vm, protos, external->u.object.properties,
                                   external->u.object.nproperties);
            if (njs_slow_path(ret != NJS_OK)) {
                return ret;
            }

            prop->type = NJS_PROPERTY_HANDLER;
            prop->value.type = NJS_INVALID;
            prop->value.data.truth = 1;
            prop->value.data.magic16 = next - slot;
            prop->value.data.magic32 = lhq.key_hash;
            prop->value.data.u.prop_handler = njs_external_prop_handler;

            next->writable = external->u.object.writable;
            next->configurable = external->u.object.configurable;
            next->enumerable = external->u.object.enumerable;
            next->prop_handler = external->u.object.prop_handler;
            next->keys = external->u.object.keys;

            break;
        }

        ret = njs_lvlhsh_insert(hash, &lhq);
        if (njs_slow_path(ret != NJS_OK)) {
            njs_internal_error(vm, "lvlhsh insert failed");
            return NJS_ERROR;
        }

        n--;
        external++;
    }

    return NJS_OK;

memory_error:

    njs_memory_error(vm);

    return NJS_ERROR;
}


static njs_int_t
njs_external_prop_handler(njs_vm_t *vm, njs_object_prop_t *self,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    njs_int_t           ret;
    njs_object_prop_t   *prop;
    njs_external_ptr_t  external;
    njs_object_value_t  *ov;
    njs_lvlhsh_query_t  lhq;
    njs_exotic_slots_t  *slots;

    if (njs_slow_path(retval == NULL)) {
        return NJS_DECLINED;
    }

    slots = NULL;

    if (njs_slow_path(setval != NULL)) {
        *retval = *setval;

    } else {
        external = njs_vm_external(vm, value);
        if (njs_slow_path(external == NULL)) {
            njs_value_undefined_set(retval);
            return NJS_OK;
        }

        ov = njs_mp_alloc(vm->mem_pool, sizeof(njs_object_value_t));
        if (njs_slow_path(ov == NULL)) {
            njs_memory_error(vm);
            return NJS_ERROR;
        }

        slots = njs_object(value)->slots + self->value.data.magic16;

        njs_lvlhsh_init(&ov->object.hash);
        ov->object.shared_hash = slots->external_shared_hash;
        ov->object.type = NJS_OBJECT;
        ov->object.shared = 0;
        ov->object.extensible = 1;
        ov->object.error_data = 0;
        ov->object.fast_array = 0;
        ov->object.__proto__ = &vm->prototypes[NJS_OBJ_TYPE_OBJECT].object;
        ov->object.slots = slots;

        njs_set_data(&ov->value, external);
        njs_set_object_value(retval, ov);
    }

    prop = njs_object_prop_alloc(vm, &self->name, retval, 1);
    if (njs_slow_path(prop == NULL)) {
        return NJS_ERROR;
    }

    if (slots != NULL) {
        prop->writable = slots->writable;
        prop->configurable = slots->configurable;
        prop->enumerable = slots->enumerable;
    }

    lhq.value = prop;
    njs_string_get(&self->name, &lhq.key);
    lhq.key_hash = self->value.data.magic32;
    lhq.replace = 1;
    lhq.pool = vm->mem_pool;
    lhq.proto = &njs_object_hash_proto;

    ret = njs_lvlhsh_insert(njs_object_hash(value), &lhq);
    if (njs_slow_path(ret != NJS_OK)) {
        njs_internal_error(vm, "lvlhsh insert/replace failed");
        return NJS_ERROR;
    }

    return NJS_OK;
}


static njs_uint_t
njs_external_protos(const njs_external_t *external, njs_uint_t size)
{
    njs_uint_t  n;

    n = 1;

    while (size != 0) {
        if ((external->flags & 3) == NJS_EXTERN_OBJECT) {
            n += njs_external_protos(external->u.object.properties,
                                     external->u.object.nproperties);
        }

        size--;
        external++;
    }

    return n;
}


njs_external_proto_t
njs_vm_external_prototype(njs_vm_t *vm, const njs_external_t *definition,
    njs_uint_t n)
{
    njs_arr_t   *protos;
    njs_int_t   ret;
    njs_uint_t  size;

    size = njs_external_protos(definition, n) + 1;

    protos = njs_arr_create(vm->mem_pool, size, sizeof(njs_exotic_slots_t));
    if (njs_slow_path(protos == NULL)) {
        njs_memory_error(vm);
        return NULL;
    }

    ret = njs_external_add(vm, protos, definition, n);
    if (njs_slow_path(ret != NJS_OK)) {
        njs_internal_error(vm, "njs_vm_external_add() failed");
        return NULL;
    }

    return protos;
}


njs_int_t
njs_vm_external_create(njs_vm_t *vm, njs_value_t *value,
    njs_external_proto_t proto, njs_external_ptr_t external, njs_bool_t shared)
{
    njs_arr_t           *protos;
    njs_object_value_t  *ov;
    njs_exotic_slots_t  *slots;

    if (njs_slow_path(proto == NULL)) {
        return NJS_ERROR;
    }

    ov = njs_mp_alloc(vm->mem_pool, sizeof(njs_object_value_t));
    if (njs_slow_path(ov == NULL)) {
        njs_memory_error(vm);
        return NJS_ERROR;
    }

    protos = proto;
    slots = protos->start;

    njs_lvlhsh_init(&ov->object.hash);
    ov->object.shared_hash = slots->external_shared_hash;
    ov->object.type = NJS_OBJECT;
    ov->object.shared = shared;
    ov->object.extensible = 1;
    ov->object.error_data = 0;
    ov->object.fast_array = 0;
    ov->object.__proto__ = &vm->prototypes[NJS_OBJ_TYPE_OBJECT].object;
    ov->object.slots = slots;

    njs_set_object_value(value, ov);
    njs_set_data(&ov->value, external);

    return NJS_OK;
}


njs_external_ptr_t
njs_vm_external(njs_vm_t *vm, const njs_value_t *value)
{
    if (njs_fast_path(njs_is_object_value(value))) {
        value = njs_object_value(value);
        if (njs_fast_path(njs_is_data(value))) {
            return njs_value_data(value);
        }
    }

    return NULL;
}
