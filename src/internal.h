#ifndef INTERNAL_H
#define INTERNAL_H

#include "rcheevos.h"

void* rc_alloc(void* pointer, int* offset, int size, void* dummy);

void rc_parse_trigger_internal(rc_trigger_t* self, int* ret, void* buffer, const char** memaddr, lua_State* L, int funcs_ndx);
void rc_reset_trigger_internal(rc_trigger_t* self, int* dirty);

rc_condset_t* rc_parse_condset(int* ret, void* buffer, const char** memaddr, lua_State* L, int funcs_ndx);
int rc_test_condset(rc_condset_t* self, int* dirty, int* reset, rc_peek_t peek, void* ud, lua_State* L);
int rc_reset_condset(rc_condset_t* self);

rc_condition_t* rc_parse_condition(int* ret, void* buffer, const char** memaddr, lua_State* L, int funcs_ndx);
int rc_test_condition(rc_condition_t* self, unsigned add_buffer, rc_peek_t peek, void* ud, lua_State* L);

int rc_parse_operand(rc_operand_t* self, const char** memaddr, int is_trigger, lua_State* L, int funcs_ndx);
unsigned rc_evaluate_operand(rc_operand_t* self, rc_peek_t peek, void* ud, lua_State* L);

rc_term_t* rc_parse_term(int* ret, void* buffer, const char** memaddr, lua_State* L, int funcs_ndx);
unsigned rc_evaluate_term(rc_term_t* self, rc_peek_t peek, void* ud, lua_State* L);

rc_expression_t* rc_parse_expression(int* ret, void* buffer, const char** memaddr, lua_State* L, int funcs_ndx);
unsigned rc_evaluate_expression(rc_expression_t* self, rc_peek_t peek, void* ud, lua_State* L);

void rc_parse_value(rc_value_t* self, int* ret, void* buffer, const char** memaddr, lua_State* L, int funcs_ndx);
unsigned rc_evaluate_value(rc_value_t* value, rc_peek_t peek, void* ud, lua_State* L);

#endif /* INTERNAL_H */
