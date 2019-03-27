/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsts/typetree.h"
#include "tt_create.h"


typedef struct array_size array_size_t;
struct array_size {
  unsigned long long size;
  array_size_t *next;
};

struct ddsts_context {
  ddsts_type_t *root_type;
  ddsts_type_t *cur_type;
  ddsts_type_t *type_for_declarator;
  array_size_t *array_sizes;
  dds_return_t retcode;
  ddsts_identifier_t dangling_identifier;
};

extern bool ddsts_new_base_type(ddsts_context_t *context, ddsts_flags_t flags, ddsts_type_t **result)
{
  assert(context != NULL);
  ddsts_type_t *base_type;
  dds_return_t rc = ddsts_create_base_type(flags, &base_type);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  *result = base_type;
  return true;
}

static bool new_sequence(ddsts_context_t *context, ddsts_type_t *element_type, unsigned long long max, ddsts_type_t **result)
{
  assert(context != NULL);
  ddsts_type_t *sequence;
  dds_return_t rc = ddsts_create_sequence(element_type, max, &sequence);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  *result = sequence;
  return true;
}

extern bool ddsts_new_sequence(ddsts_context_t *context, ddsts_type_t *base, ddsts_literal_t *size, ddsts_type_t **result)
{
  assert(size->flags == DDSTS_ULONGLONG);
  return new_sequence(context, base, size->value.ullng, result);
}

extern bool ddsts_new_sequence_unbound(ddsts_context_t *context, ddsts_type_t *base, ddsts_type_t **result)
{
  return new_sequence(context, base, 0ULL, result);
}

static bool new_string(ddsts_context_t *context, ddsts_flags_t flags, unsigned long long max, ddsts_type_t **result)
{
  assert(context != NULL);
  ddsts_type_t *string;
  dds_return_t rc = ddsts_create_string(flags, max, &string);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  *result = string;
  return true;
}

extern bool ddsts_new_string(ddsts_context_t *context, ddsts_literal_t *size, ddsts_type_t **result)
{
  assert(size->flags == DDSTS_ULONGLONG);
  return new_string(context, DDSTS_STRING, size->value.ullng, result);
}

extern bool ddsts_new_string_unbound(ddsts_context_t *context, ddsts_type_t **result)
{
  return new_string(context, DDSTS_STRING, 0ULL, result);
}

extern bool ddsts_new_wide_string(ddsts_context_t *context, ddsts_literal_t *size, ddsts_type_t **result)
{
  assert(size->flags == DDSTS_ULONGLONG);
  return new_string(context, DDSTS_WIDE_STRING, size->value.ullng, result);
}

extern bool ddsts_new_wide_string_unbound(ddsts_context_t *context, ddsts_type_t **result)
{
  return new_string(context, DDSTS_WIDE_STRING, 0ULL, result);
}

extern bool ddsts_new_fixed_pt(ddsts_context_t *context, ddsts_literal_t *digits, ddsts_literal_t *fraction_digits, ddsts_type_t **result)
{
  assert(context != NULL);
  assert(digits->flags == DDSTS_ULONGLONG);
  assert(fraction_digits->flags == DDSTS_ULONGLONG);
  ddsts_type_t *fixed_pt;
  dds_return_t rc = ddsts_create_fixed_pt(digits->value.ullng, fraction_digits->value.ullng, &fixed_pt);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  *result = fixed_pt;
  return true;
}

static bool new_map(ddsts_context_t *context, ddsts_type_t *key_type, ddsts_type_t *value_type, unsigned long long max, ddsts_type_t **result)
{
  ddsts_type_t *map;
  dds_return_t rc = ddsts_create_map(key_type, value_type, max, &map);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  *result = map;
  return true;
}

extern bool ddsts_new_map(ddsts_context_t *context, ddsts_type_t *key_type, ddsts_type_t *value_type, ddsts_literal_t *size, ddsts_type_t **result)
{
  assert(size->flags == DDSTS_ULONGLONG);
  return new_map(context, key_type, value_type, size->value.ullng, result);
}

extern bool ddsts_new_map_unbound(ddsts_context_t *context, ddsts_type_t *key_type, ddsts_type_t *value_type, ddsts_type_t **result)
{
  return new_map(context, key_type, value_type, 0UL, result);
}

struct ddsts_scoped_name {
  ddsts_identifier_t name;
  bool top;
  ddsts_scoped_name_t *next;
};

extern bool ddsts_new_scoped_name(ddsts_context_t *context, ddsts_scoped_name_t* prev, bool top, ddsts_identifier_t name, ddsts_scoped_name_t **result)
{
  assert(context != NULL);
  assert(context->dangling_identifier == name);
  ddsts_scoped_name_t *scoped_name = (ddsts_scoped_name_t*)ddsrt_malloc(sizeof(ddsts_scoped_name_t));
  if (scoped_name == NULL) {
    context->retcode = DDS_RETCODE_OUT_OF_RESOURCES;
    return false;
  }
  context->dangling_identifier = NULL;
  scoped_name->name = name;
  scoped_name->top = top;
  scoped_name->next = NULL;
  if (prev == NULL) {
    *result = scoped_name;
  }
  else {
    ddsts_scoped_name_t **ref_scoped_name = &prev->next;
    while (*ref_scoped_name != NULL) {
      ref_scoped_name = &(*ref_scoped_name)->next;
    }
    *ref_scoped_name = scoped_name;
    *result = prev;
  }
  return true;
}

static bool resolve_scoped_name(ddsts_context_t *context, ddsts_scoped_name_t *scoped_name, ddsts_type_t **result)
{
  assert(context != NULL);
  assert(scoped_name != NULL);
  ddsts_type_t *cur_type = scoped_name->top ? context->root_type : context->cur_type;
  for (; cur_type != NULL; cur_type = cur_type->type.parent) {
    ddsts_type_t *found_type = cur_type;
    ddsts_scoped_name_t *cur_scoped_name;
    for (cur_scoped_name = scoped_name; cur_scoped_name != NULL && found_type != NULL; cur_scoped_name = cur_scoped_name->next) {
      ddsts_type_t *child = NULL;
      if (DDSTS_IS_TYPE(found_type, DDSTS_MODULE)) {
        child = found_type->module.members;
      }
      else if (DDSTS_IS_TYPE(found_type, DDSTS_STRUCT)) {
        child = found_type->struct_def.members;
      }
      found_type = NULL;
      for (; child != NULL; child = child->type.next) {
        if (DDSTS_IS_DEFINITION(child) && strcmp(child->type.name, cur_scoped_name->name) == 0) {
          found_type = child;
          break;
        }
      }
    }
    if (found_type != NULL) {
      *result = found_type;
      return true;
    }
  }
  /* Could not resolve scoped name */
  DDS_ERROR("Could not resolve scoped name\n");
  *result = NULL;
  return false;
}

extern void ddsts_free_scoped_name(ddsts_scoped_name_t *scoped_name)
{
  while (scoped_name != NULL) {
    ddsts_scoped_name_t *next = scoped_name->next;
    ddsrt_free((void*)scoped_name->name);
    ddsrt_free((void*)scoped_name);
    scoped_name = next;
  }
}

extern bool ddsts_get_type_from_scoped_name(ddsts_context_t *context, ddsts_scoped_name_t *scoped_name, ddsts_type_t **result)
{
  ddsts_type_t *definition;
  resolve_scoped_name(context, scoped_name, &definition);
  ddsts_free_scoped_name(scoped_name);
  if (definition == NULL) {
    /* Create a Boolean type, just to prevent a NULL pointer */
    return ddsts_new_base_type(context, DDSTS_BOOLEAN, result);
  }
  *result = definition;
  return true;
}

static bool new_module_definition(ddsts_context_t *context, ddsts_identifier_t name, ddsts_type_t *parent, ddsts_type_t **result)
{
  assert(context != NULL);
  assert(context->dangling_identifier == name);
  ddsts_type_t *module;
  dds_return_t rc = ddsts_create_module(name, &module);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  context->dangling_identifier = NULL;
  ddsts_module_add_member(parent, module);
  *result = module;
  return true;
}

extern ddsts_context_t* ddsts_create_context(void)
{
  ddsts_context_t *context = (ddsts_context_t*)ddsrt_malloc(sizeof(ddsts_context_t));
  if (context == NULL) {
    return NULL;
  }
  context->dangling_identifier = NULL;
  ddsts_type_t *module;
  if (!new_module_definition(context, NULL, NULL, &module)) {
    ddsrt_free(context);
    return NULL;
  }
  context->root_type = module;
  context->cur_type = context->root_type;
  context->type_for_declarator = NULL;
  context->array_sizes = NULL;
  context->retcode = DDS_RETCODE_ERROR;
  return context;
}

extern dds_return_t ddsts_context_get_retcode(ddsts_context_t* context)
{
  assert(context != NULL);
  return context->retcode;
}

extern ddsts_type_t* ddsts_context_take_root_type(ddsts_context_t *context)
{
  ddsts_type_t* result = context->root_type;
  context->root_type = NULL;
  return result;
}

static void ddsts_context_free_array_sizes(ddsts_context_t *context)
{
  while (context->array_sizes != NULL) {
    array_size_t *next = context->array_sizes->next;
    ddsrt_free((void*)context->array_sizes);
    context->array_sizes = next;
  }
}

static void ddsts_context_close_member(ddsts_context_t *context)
{
  if (context->type_for_declarator != NULL && context->type_for_declarator->type.parent == NULL) {
    ddsts_free_type(context->type_for_declarator);
  }
  context->type_for_declarator = NULL;
  ddsts_context_free_array_sizes(context);
}

extern void ddsts_free_context(ddsts_context_t *context)
{
  assert(context != NULL);
  ddsts_context_close_member(context);
  ddsts_free_type(context->root_type);
  ddsrt_free(context->dangling_identifier);
  ddsrt_free(context);
}

bool ddsts_context_copy_identifier(ddsts_context_t *context, ddsts_identifier_t source, ddsts_identifier_t *dest)
{
  assert(context != NULL && source != NULL);
  assert(context->dangling_identifier == NULL);
  context->dangling_identifier = *dest = ddsrt_strdup(source);
  if (context->dangling_identifier == NULL) {
    context->retcode = DDS_RETCODE_OUT_OF_RESOURCES;
    return false;
  }
  return true;
}

#if (!defined(NDEBUG))
static bool cur_scope_is_definition_type(ddsts_context_t *context, ddsts_flags_t flags)
{
  assert(context != NULL && context->cur_type != NULL);
  return DDSTS_IS_TYPE(context->cur_type, flags);
}
#endif

extern bool ddsts_module_open(ddsts_context_t *context, ddsts_identifier_t name)
{
  assert(cur_scope_is_definition_type(context, DDSTS_MODULE));
  ddsts_type_t *module;
  if (!new_module_definition(context, name, context->cur_type, &module)) {
    return false;
  }
  context->cur_type = module;
  return true;
}

extern void ddsts_module_close(ddsts_context_t *context)
{
  assert(cur_scope_is_definition_type(context, DDSTS_MODULE));
  assert(context->cur_type->type.parent != NULL);
  context->cur_type = context->cur_type->type.parent;
}

extern bool ddsts_add_struct_forward(ddsts_context_t *context, ddsts_identifier_t name)
{
  assert(cur_scope_is_definition_type(context, DDSTS_MODULE));
  assert(context->dangling_identifier == name);
  ddsts_type_t *forward_dcl;
  dds_return_t rc = ddsts_create_struct_forward_dcl(name, &forward_dcl);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  context->dangling_identifier = NULL;
  ddsts_module_add_member(context->cur_type, forward_dcl);
  return true;
}

extern bool ddsts_add_struct_open(ddsts_context_t *context, ddsts_identifier_t name)
{
  assert(   cur_scope_is_definition_type(context, DDSTS_MODULE)
         || cur_scope_is_definition_type(context, DDSTS_STRUCT));
  assert(context->dangling_identifier == name);
  ddsts_type_t *new_struct;
  dds_return_t rc = ddsts_create_struct(name, &new_struct);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  context->dangling_identifier = NULL;
  if (DDSTS_IS_TYPE(context->cur_type, DDSTS_MODULE)) {
    ddsts_module_add_member(context->cur_type, new_struct);
  }
  else {
    ddsts_struct_add_member(context->cur_type, new_struct);
  }
  context->cur_type = new_struct;
  return true;
}

extern bool ddsts_add_struct_extension_open(ddsts_context_t *context, ddsts_identifier_t name, ddsts_scoped_name_t *scoped_name)
{
  assert(cur_scope_is_definition_type(context, DDSTS_MODULE));
  assert(context->dangling_identifier == name);
  ddsts_type_t *new_struct;
  dds_return_t rc = ddsts_create_struct(name, &new_struct);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  context->dangling_identifier = NULL;
  ddsts_module_add_member(context->cur_type, new_struct);
  /* find super */
  ddsts_type_t *definition;
  if (!resolve_scoped_name(context, scoped_name, &definition)) {
    ddsts_free_scoped_name(scoped_name);
    return false;
  }
  ddsts_free_scoped_name(scoped_name);
  if (definition != NULL && DDSTS_IS_TYPE(definition, DDSTS_STRUCT)) {
    new_struct->struct_def.super = definition;
  }
  context->cur_type = new_struct;
  return true;
}

extern bool ddsts_add_struct_member(ddsts_context_t *context, ddsts_type_t **ref_spec_type)
{
  assert(cur_scope_is_definition_type(context, DDSTS_STRUCT));
  ddsts_context_close_member(context);
  context->type_for_declarator = *ref_spec_type;
  *ref_spec_type = NULL;
  return true;
}

extern void ddsts_struct_close(ddsts_context_t *context, ddsts_type_t **result)
{
  assert(cur_scope_is_definition_type(context, DDSTS_STRUCT));
  ddsts_context_close_member(context);
  *result = context->cur_type;
  context->cur_type = context->cur_type->type.parent;
}

extern void ddsts_struct_empty_close(ddsts_context_t *context, ddsts_type_t **result)
{
  assert(cur_scope_is_definition_type(context, DDSTS_STRUCT));
  *result = context->cur_type;
  context->cur_type = context->cur_type->type.parent;
}

static ddsts_type_t *create_array_type(array_size_t *array_size, ddsts_type_t *type)
{
  if (array_size == NULL) {
    return type;
  }
  ddsts_type_t *array;
  dds_return_t rc = ddsts_create_array(NULL, array_size->size, &array);
  if (rc != DDS_RETCODE_OK) {
    return NULL;
  }
  ddsts_type_t *element_type = create_array_type(array_size->next, type);
  if (element_type == NULL) {
    ddsts_free_type(array);
    return NULL;
  }
  ddsts_array_set_element_type(array, element_type);
  return array;
}

extern bool ddsts_add_declarator(ddsts_context_t *context, ddsts_identifier_t name)
{
  assert(context != NULL);
  assert(context->dangling_identifier == name);
  if (DDSTS_IS_TYPE(context->cur_type, DDSTS_STRUCT)) {
    assert(context->type_for_declarator != NULL);
    ddsts_type_t *decl;
    dds_return_t rc = ddsts_create_declaration(name, NULL, &decl);
    if (rc != DDS_RETCODE_OK) {
      ddsts_context_free_array_sizes(context);
      context->retcode = rc;
      return false;
    }
    context->dangling_identifier = NULL;
    ddsts_type_t* type = create_array_type(context->array_sizes, context->type_for_declarator);
    if (type == NULL) {
      ddsts_context_free_array_sizes(context);
      ddsts_free_type(decl);
      context->retcode = DDS_RETCODE_OUT_OF_RESOURCES;
      return false;
    }
    ddsts_context_free_array_sizes(context);
    ddsts_declaration_set_type(decl, type);
    ddsts_struct_add_member(context->cur_type, decl);
    return true;
  }
  assert(false);
  return false;
}

extern bool ddsts_add_array_size(ddsts_context_t *context, ddsts_literal_t *value)
{
  assert(context != NULL);
  assert(value->flags == DDSTS_ULONGLONG);
  array_size_t **ref_array_size = &context->array_sizes;
  while (*ref_array_size != NULL) {
    ref_array_size = &(*ref_array_size)->next;
  }
  *ref_array_size = (array_size_t*)ddsrt_malloc(sizeof(array_size_t));
  if (*ref_array_size == NULL) {
    context->retcode = DDS_RETCODE_OUT_OF_RESOURCES;
    return false;
  }
  (*ref_array_size)->size = value->value.ullng;
  (*ref_array_size)->next = NULL;
  return true;
}

extern void ddsts_accept(ddsts_context_t *context)
{
  assert(context != NULL);
  context->retcode = DDS_RETCODE_OK;
}
