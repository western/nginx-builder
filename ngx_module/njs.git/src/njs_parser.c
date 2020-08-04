
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Alexander Borisov
 * Copyright (C) NGINX, Inc.
 */


#include <njs_main.h>


static njs_int_t njs_parser_scope_begin(njs_parser_t *parser, njs_scope_t type);
static void njs_parser_scope_end(njs_parser_t *parser);

static njs_int_t njs_parser_check_error_state(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_primary_expression_test(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_regexp_literal(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_template_literal(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_template_literal_string(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_template_literal_expression(
    njs_parser_t *parser, njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_cover_parenthesized_expression(
    njs_parser_t *parser, njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_binding_identifier_pattern(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_cover_parenthesized_expression_after(
    njs_parser_t *parser, njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_cover_parenthesized_expression_end(
    njs_parser_t *parser, njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_array_literal(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_array_element_list(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_array_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_array_spread_element(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_object_literal(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_object_literal_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_property_definition_list(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_property_definition_list_after(
    njs_parser_t *parser, njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_property_definition(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_property_definition_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_computed_property_name_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_initializer(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_initializer_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_initializer_assign(njs_parser_t *parser,
    njs_token_type_t type);

static njs_int_t njs_parser_member_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_member_expression_next(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_member_expression_bracket(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_member_expression_new_next(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_super_property(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_member_expression_import(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_member_expression_new(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_member_expression_new_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_member_expression_new_args(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_parser_node_t *njs_parser_create_call(njs_parser_t *parser,
    njs_parser_node_t *node, uint8_t ctor);
static njs_int_t njs_parser_call_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_call_expression_args(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_call_expression_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_arguments(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_parenthesis_or_comma(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_argument_list(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_argument_list_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_optional_expression_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_optional_chain(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_optional_chain_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_new_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_new_expression_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_left_hand_side_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_left_hand_side_expression_after(
    njs_parser_t *parser, njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_left_hand_side_expression_node(
    njs_parser_t *parser, njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_left_hand_side_expression_optional(
    njs_parser_t *parser, njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_update_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_update_expression_post(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_update_expression_unary(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_unary_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_unary_expression_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_unary_expression_next(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_exponentiation_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_exponentiation_expression_match(
    njs_parser_t *parser, njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_multiplicative_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_multiplicative_expression_match(
    njs_parser_t *parser, njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_additive_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_additive_expression_match(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_shift_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_shift_expression_match(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_relational_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_relational_expression_match(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_equality_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_equality_expression_match(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_bitwise_AND_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_bitwise_AND_expression_and(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_bitwise_XOR_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_bitwise_XOR_expression_xor(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_bitwise_OR_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_bitwise_OR_expression_or(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_logical_AND_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_logical_AND_expression_and(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_logical_OR_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_logical_OR_expression_or(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_coalesce_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_short_circuit_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_conditional_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_conditional_question_mark(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_conditional_colon(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_conditional_colon_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_assignment_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_match_arrow_expression(njs_parser_t *parser,
    njs_lexer_token_t *token);
static njs_int_t njs_parser_assignment_expression_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_assignment_operator(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_assignment_operator_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_expression_comma(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_statement_wo_node(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_declaration(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_hoistable_declaration(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_block_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_block_statement_open_brace(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_block_statement_close_brace(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_statement_list(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_statement_list_next(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_statement_list_item(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_variable_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_variable_declaration_list(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_variable_declaration_list_next(
    njs_parser_t *parser, njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_variable_declaration(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_binding_pattern(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_object_binding_pattern(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_array_binding_pattern(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_expression_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_expression_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_if_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_if_close_parenthesis(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_else_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_else_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_iteration_statement_do(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_iteration_statement_do_while(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_do_while_semicolon(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_iteration_statement_while(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_while_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_while_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_iteration_statement_for(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_iteration_statement_for_map(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_for_var_binding_or_var_list(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_for_var_in_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_for_var_in_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_for_var_in_of_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_for_in_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_for_in_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_for_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_for_expression_end(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_for_end(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_switch_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_switch_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_switch_block(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_switch_case(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_switch_case_wo_def(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_switch_case_def(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current,
    njs_bool_t with_default);
static njs_int_t njs_parser_switch_case_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_switch_case_block(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_switch_case_after_wo_def(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_switch_case_block_wo_def(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_continue_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_break_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_break_continue(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_token_type_t type);

static njs_int_t njs_parser_return_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_return_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_with_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_labelled_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_labelled_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_throw_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_throw_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_try_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_catch_or_finally(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_catch_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_catch_parenthesis(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_catch_finally(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_debugger_statement(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_function_declaration(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_function_declaration_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_function_parse(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_function_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_function_expression_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_unique_formal_parameters(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_formal_parameters(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_formal_parameters_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_arrow_function(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_arrow_function_args_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_arrow_function_arrow(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_arrow_function_body_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_method_definition(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_get_set(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_get_set_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_get_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_set_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_function_lambda(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_function_lambda_args_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_function_lambda_body_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_export(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_export_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);

static njs_int_t njs_parser_import(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_import_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_module_lambda_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current);
static njs_int_t njs_parser_export_sink(njs_parser_t *parser);

static njs_parser_node_t *njs_parser_return_set(njs_parser_t *parser,
    njs_parser_node_t *expr);
static njs_parser_node_t *njs_parser_variable_node(njs_parser_t *parser,
    uintptr_t unique_id, njs_variable_type_t type);

static njs_parser_node_t *njs_parser_reference(njs_parser_t *parser,
    njs_lexer_token_t *token);

static njs_parser_node_t *njs_parser_argument(njs_parser_t *parser,
    njs_parser_node_t *expr, njs_index_t index);

static njs_int_t njs_parser_object_property(njs_parser_t *parser,
    njs_parser_node_t *parent, njs_parser_node_t *property,
    njs_parser_node_t *value, njs_bool_t proto_init);
static njs_int_t njs_parser_property_accessor(njs_parser_t *parser,
    njs_parser_node_t *parent, njs_parser_node_t *property,
    njs_parser_node_t *value, njs_token_type_t accessor);
static njs_int_t njs_parser_array_item(njs_parser_t *parser,
    njs_parser_node_t *array, njs_parser_node_t *value);
static njs_int_t njs_parser_template_string(njs_parser_t *parser,
    njs_lexer_token_t *token);
static njs_token_type_t njs_parser_escape_string_create(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_value_t *value);
static njs_int_t njs_parser_escape_string_calc_length(njs_parser_t *parser,
    njs_lexer_token_t *token, size_t *out_size, size_t *out_length);

static void njs_parser_serialize_tree(njs_chb_t *chain,
    njs_parser_node_t *node, njs_int_t *ret, size_t indent);
static njs_int_t njs_parser_serialize_node(njs_chb_t *chain,
    njs_parser_node_t *node);


#define njs_parser_chain_top(parser)                                          \
    ((parser)->scope->top)


#define njs_parser_chain_top_set(parser, node)                                \
    (parser)->scope->top = node


njs_inline njs_int_t
njs_parser_not_supported(njs_parser_t *parser, njs_lexer_token_t *token)
{
    if (token->type != NJS_TOKEN_END) {
        njs_parser_syntax_error(parser, "Token \"%V\" not supported "
                                "in this version", &token->text);
    } else {
        njs_parser_syntax_error(parser, "Not supported in this version");
    }

    return NJS_DONE;
}


njs_inline njs_int_t
njs_parser_reject(njs_parser_t *parser)
{
    njs_parser_stack_entry_t  *entry;

    while (!njs_queue_is_empty(&parser->stack)) {
        entry = njs_queue_link_data(njs_queue_first(&parser->stack),
                                    njs_parser_stack_entry_t, link);

        njs_queue_remove(njs_queue_first(&parser->stack));

        if (!entry->optional) {
            njs_parser_next(parser, entry->state);
            parser->target = entry->node;

            return NJS_DECLINED;
        }
    }

    return njs_parser_failed(parser);
}


njs_int_t
njs_parser(njs_parser_t *parser, njs_parser_t *prev)
{
    njs_int_t          ret;
    njs_lexer_token_t  *token;

    ret = njs_parser_scope_begin(parser, NJS_SCOPE_GLOBAL);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    if (prev != NULL) {
        /*
         * Copy the global scope variables from the previous
         * iteration of the accumulative mode.
         */
        ret = njs_variables_copy(parser->vm, &parser->scope->variables,
                                 &prev->scope->variables);
        if (ret != NJS_OK) {
            return ret;
        }
    }

    njs_queue_init(&parser->stack);

    parser->target = NULL;
    njs_parser_next(parser, njs_parser_statement_list);

    ret = njs_parser_after(parser, njs_queue_first(&parser->stack),
                           NULL, 0, njs_parser_check_error_state);
    if (ret != NJS_OK) {
        return ret;
    }

    do {
        token = njs_lexer_token(parser->lexer, 0);
        if (njs_slow_path(token == NULL)) {
            return NJS_ERROR;
        }

        parser->ret = parser->state(parser, token,
                                    njs_queue_first(&parser->stack));

    } while (parser->ret != NJS_DONE && parser->ret != NJS_ERROR);

    if (parser->ret != NJS_DONE) {
        return NJS_ERROR;
    }

    if (njs_is_error(&parser->vm->retval)) {
        return NJS_ERROR;
    }

    if (parser->node == NULL) {
        /* Empty string, just semicolons or variables declarations. */

        parser->node = njs_parser_node_new(parser, 0);
        if (njs_slow_path(parser->node == NULL)) {
            return NJS_ERROR;
        }
    }

    parser->node->token_type = NJS_TOKEN_END;
    parser->node->token_line = parser->lexer->line;

    njs_parser_chain_top_set(parser, parser->node);

    return NJS_OK;
}


static njs_int_t
njs_parser_check_error_state(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    return njs_parser_failed(parser);
}


njs_int_t
njs_parser_failed_state(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_END) {
        njs_parser_syntax_error(parser, "Unexpected token \"%V\"",
                                &token->text);
    } else {
        njs_parser_syntax_error(parser, "Unexpected end of input");
    }

    return NJS_DONE;
}


static njs_int_t
njs_parser_scope_begin(njs_parser_t *parser, njs_scope_t type)
{
    njs_arr_t           *values;
    njs_uint_t          nesting;
    njs_lexer_t         *lexer;
    njs_parser_scope_t  *scope, *parent;

    nesting = 0;

    if (type == NJS_SCOPE_FUNCTION) {

        for (scope = parser->scope; scope != NULL; scope = scope->parent) {

            if (scope->type == NJS_SCOPE_FUNCTION) {
                nesting = scope->nesting + 1;

                if (nesting < NJS_MAX_NESTING) {
                    break;
                }

                njs_parser_syntax_error(parser, "The maximum function nesting "
                                        "level is \"%d\"", NJS_MAX_NESTING);

                return NJS_ERROR;
            }
        }
    }

    scope = njs_mp_zalloc(parser->vm->mem_pool, sizeof(njs_parser_scope_t));
    if (njs_slow_path(scope == NULL)) {
        return NJS_ERROR;
    }

    scope->type = type;

    if (type == NJS_SCOPE_FUNCTION) {
        scope->next_index[0] = type;
        scope->next_index[1] = NJS_SCOPE_CLOSURE + nesting
                               + sizeof(njs_value_t);

    } else {
        if (type == NJS_SCOPE_GLOBAL) {
            type += NJS_INDEX_GLOBAL_OFFSET;
        }

        scope->next_index[0] = type;
        scope->next_index[1] = 0;
    }

    scope->nesting = nesting;
    scope->argument_closures = 0;

    njs_queue_init(&scope->nested);
    njs_rbtree_init(&scope->variables, njs_parser_scope_rbtree_compare);
    njs_rbtree_init(&scope->labels, njs_parser_scope_rbtree_compare);
    njs_rbtree_init(&scope->references, njs_parser_scope_rbtree_compare);

    values = NULL;

    if (scope->type < NJS_SCOPE_BLOCK) {
        values = njs_arr_create(parser->vm->mem_pool, 4, sizeof(njs_value_t));
        if (njs_slow_path(values == NULL)) {
            return NJS_ERROR;
        }
    }

    scope->values[0] = values;
    scope->values[1] = NULL;

    lexer = parser->lexer;

    if (lexer->file.length != 0) {
        njs_file_basename(&lexer->file, &scope->file);
        njs_file_dirname(&lexer->file, &scope->cwd);
    }

    parent = parser->scope;
    scope->parent = parent;
    parser->scope = scope;

    if (parent != NULL) {
        njs_queue_insert_tail(&parent->nested, &scope->link);

        if (nesting == 0) {
            /* Inherit function nesting in blocks. */
            scope->nesting = parent->nesting;
        }
    }

    return NJS_OK;
}


static void
njs_parser_scope_end(njs_parser_t *parser)
{
    njs_parser_scope_t  *scope, *parent;

    scope = parser->scope;

    parent = scope->parent;
    parser->scope = parent;
}


intptr_t
njs_parser_scope_rbtree_compare(njs_rbtree_node_t *node1,
    njs_rbtree_node_t *node2)
{
    njs_variable_node_t  *lex_node1, *lex_node2;

    lex_node1 = (njs_variable_node_t *) node1;
    lex_node2 = (njs_variable_node_t *) node2;

    if (lex_node1->key < lex_node2->key) {
        return -1;
    }

    if (lex_node1->key > lex_node2->key) {
        return 1;
    }

    return 0;
}


static njs_int_t
njs_parser_generator_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    return njs_parser_not_supported(parser, token);
}


static njs_int_t
njs_parser_class_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    return njs_parser_not_supported(parser, token);
}


static njs_int_t
njs_parser_async_generator_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    return njs_parser_not_supported(parser, token);
}


static njs_int_t
njs_parser_async_function_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    return njs_parser_not_supported(parser, token);
}


static njs_int_t
njs_parser_generator_declaration(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    return njs_parser_not_supported(parser, token);
}


static njs_int_t
njs_parser_class_declaration(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    return njs_parser_not_supported(parser, token);
}


static njs_int_t
njs_parser_lexical_declaration(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    return njs_parser_not_supported(parser, token);
}


static njs_int_t
njs_parser_function_or_generator(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t  *node, *cur;

    if (token->type != NJS_TOKEN_FUNCTION) {
        return NJS_DECLINED;
    }

    cur = parser->node;

    if (token->type == NJS_TOKEN_MULTIPLICATION) {
        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_generator_declaration);

    } else {
        node = njs_parser_node_new(parser, NJS_TOKEN_FUNCTION);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->token_line = token->line;
        parser->node = node;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_function_declaration);
    }

    return njs_parser_after(parser, current, cur, 1,
                            njs_parser_statement_after);
}


static njs_int_t
njs_parser_async_function_or_generator(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_ASYNC) {
        return NJS_DECLINED;
    }

    token = njs_lexer_peek_token(parser->lexer, token, 1);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type != NJS_TOKEN_FUNCTION) {
        return NJS_DECLINED;
    }

    if (token->type == NJS_TOKEN_MULTIPLICATION) {
        njs_parser_next(parser, njs_parser_generator_declaration);
    } else {
        njs_parser_next(parser, njs_parser_async_function_expression);
    }

    return NJS_OK;
}


njs_inline njs_int_t
njs_parser_expect_semicolon(njs_parser_t *parser, njs_lexer_token_t *token)
{
    if (token->type != NJS_TOKEN_SEMICOLON) {
        if (parser->strict_semicolon
            || (token->type != NJS_TOKEN_END
                && token->type != NJS_TOKEN_CLOSE_BRACE
                && parser->lexer->prev_type != NJS_TOKEN_LINE_END))
        {
            return NJS_DECLINED;
        }

        return NJS_OK;
    }

    njs_lexer_consume_token(parser->lexer, 1);

    return NJS_OK;
}


static njs_int_t
njs_parser_semicolon(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (njs_parser_expect_semicolon(parser, token) != NJS_OK) {
        return njs_parser_failed(parser);
    }

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_close_bracked(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_CLOSE_BRACKET) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_close_parenthesis(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (parser->ret != NJS_OK) {
        return njs_parser_failed(parser);
    }

    if (token->type != NJS_TOKEN_CLOSE_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_expression_parenthesis(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_OPEN_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_expression);

    return njs_parser_after(parser, current, NULL, 0,
                            njs_parser_close_parenthesis);
}


static njs_int_t
njs_parser_set_line_state(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    parser->node->token_line = (uint32_t) (uintptr_t) parser->target;
    parser->target = NULL;

    return njs_parser_stack_pop(parser);
}


/*
 * 12.2 Primary Expression.
 */
static njs_int_t
njs_parser_primary_expression_test(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t          ret;
    njs_lexer_token_t  *next;
    njs_parser_node_t  *node;

    switch (token->type) {
    /* IdentifierReference */
    case NJS_TOKEN_THIS:
    case NJS_TOKEN_NULL:
    case NJS_TOKEN_NAME:
    case NJS_TOKEN_YIELD:
    case NJS_TOKEN_AWAIT:
        goto reference;

    case NJS_TOKEN_TRUE:
        node = njs_parser_node_new(parser, token->type);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->u.value = njs_value_true;
        node->token_line = token->line;

        parser->node = node;
        goto done;

    case NJS_TOKEN_FALSE:
        node = njs_parser_node_new(parser, token->type);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->u.value = njs_value_false;
        node->token_line = token->line;

        parser->node = node;
        goto done;

    case NJS_TOKEN_NUMBER:
        node = njs_parser_node_new(parser, NJS_TOKEN_NUMBER);
        if (node == NULL) {
            return NJS_ERROR;
        }

        njs_set_number(&node->u.value, token->number);
        node->token_line = token->line;

        parser->node = node;
        goto done;

    case NJS_TOKEN_STRING:
        node = njs_parser_node_new(parser, NJS_TOKEN_STRING);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->token_line = token->line;

        ret = njs_parser_string_create(parser->vm, token, &node->u.value);
        if (ret != NJS_OK) {
            return NJS_ERROR;
        }

        parser->node = node;
        goto done;

    case NJS_TOKEN_ESCAPE_STRING:
        /* Internal optimization. This is not in the specification. */

        node = njs_parser_node_new(parser, NJS_TOKEN_STRING);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->token_line = token->line;

        ret = njs_parser_escape_string_create(parser, token, &node->u.value);
        if (ret != NJS_TOKEN_STRING) {
            return NJS_ERROR;
        }

        parser->node = node;
        goto done;

    case NJS_TOKEN_UNTERMINATED_STRING:
        /* Internal optimization. This is not in the specification. */

        njs_parser_syntax_error(parser, "Unterminated string \"%V\"",
                                &token->text);
        return NJS_ERROR;

    /* ArrayLiteral */
    case NJS_TOKEN_OPEN_BRACKET:
        node = njs_parser_node_new(parser, NJS_TOKEN_ARRAY);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->token_line = token->line;
        parser->node = node;

        njs_parser_next(parser, njs_parser_array_literal);
        break;

    /* ObjectLiteral */
    case NJS_TOKEN_OPEN_BRACE:
        node = njs_parser_node_new(parser, NJS_TOKEN_OBJECT);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->token_line = token->line;
        parser->node = node;

        njs_parser_next(parser, njs_parser_object_literal);
        break;

    /* FunctionExpression */
    case NJS_TOKEN_FUNCTION:
        token = njs_lexer_peek_token(parser->lexer, token, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        /* GeneratorExpression */
        if (token->type == NJS_TOKEN_MULTIPLICATION) {
            njs_parser_next(parser, njs_parser_generator_expression);

        } else {
            node = njs_parser_node_new(parser, NJS_TOKEN_FUNCTION_EXPRESSION);
            if (node == NULL) {
                return NJS_ERROR;
            }

            node->token_line = token->line;
            parser->node = node;

            njs_parser_next(parser, njs_parser_function_expression);
        }

        break;

    /* ClassExpression */
    case NJS_TOKEN_CLASS:
        njs_parser_next(parser, njs_parser_class_expression);
        return NJS_OK;

    /* AsyncFunctionExpression */
    case NJS_TOKEN_ASYNC:
        next = njs_lexer_peek_token(parser->lexer, token, 1);
        if (next == NULL) {
            return NJS_ERROR;
        }

        if (next->type != NJS_TOKEN_FUNCTION) {
            goto reference;
        }

        next = njs_lexer_peek_token(parser->lexer, next, 0);
        if (njs_slow_path(next == NULL)) {
            return NJS_ERROR;
        }

        /* GeneratorExpression */
        if (next->type == NJS_TOKEN_MULTIPLICATION) {
            njs_lexer_consume_token(parser->lexer, 1);
            njs_parser_next(parser, njs_parser_async_generator_expression);

        } else {
            njs_parser_next(parser, njs_parser_async_function_expression);
        }

        return NJS_OK;

    /* RegularExpressionLiteral */
    case NJS_TOKEN_DIVISION:
    case NJS_TOKEN_DIVISION_ASSIGNMENT:
        node = njs_parser_node_new(parser, NJS_TOKEN_REGEXP);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->token_line = token->line;
        parser->node = node;

        ret = njs_parser_regexp_literal(parser, token, current);
        if (ret != NJS_OK) {
            return NJS_ERROR;
        }

        goto done;

    /* TemplateLiteral */
    case NJS_TOKEN_GRAVE:
        node = njs_parser_node_new(parser, NJS_TOKEN_TEMPLATE_LITERAL);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->token_line = token->line;
        parser->node = node;

        njs_parser_next(parser, njs_parser_template_literal);
        return NJS_OK;

    /* CoverParenthesizedExpressionAndArrowParameterList */
    case NJS_TOKEN_OPEN_PARENTHESIS:
        njs_lexer_consume_token(parser->lexer, 1);

        /* TODO: By specification. */
        (void) njs_parser_cover_parenthesized_expression;

        parser->node = NULL;

        njs_parser_next(parser, njs_parser_expression);

        return njs_parser_after(parser, current, NULL, 0,
                                njs_parser_close_parenthesis);

    default:
        if (njs_lexer_token_is_identifier_reference(token)) {
            goto reference;
        }

        return njs_parser_reject(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    return NJS_OK;

reference:

    node = njs_parser_reference(parser, token);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    parser->node = node;

done:

    njs_lexer_consume_token(parser->lexer, 1);

    return NJS_DONE;
}


static njs_int_t
njs_parser_regexp_literal(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    u_char                *p;
    njs_str_t             text;
    njs_lexer_t           *lexer;
    njs_value_t           *value;
    njs_regexp_flags_t    flags;
    njs_regexp_pattern_t  *pattern;

    value = &parser->node->u.value;
    lexer = parser->lexer;

    if (token->type == NJS_TOKEN_DIVISION_ASSIGNMENT) {
        lexer->start--;
    }

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

            flags = njs_regexp_flags(&p, lexer->end);

            if (njs_slow_path(flags < 0)) {
                njs_parser_syntax_error(parser, "Invalid RegExp flags \"%*s\"",
                                        p - lexer->start, lexer->start);

                return NJS_ERROR;
            }

            lexer->start = p;

            pattern = njs_regexp_pattern_create(parser->vm, text.start,
                                                text.length, flags);

            if (njs_slow_path(pattern == NULL)) {
                return NJS_ERROR;
            }

            value->data.u.data = pattern;

            return NJS_OK;
        }
    }

failed:

    njs_parser_syntax_error(parser, "Unterminated RegExp \"%*s\"",
                            p - (lexer->start - 1), lexer->start - 1);
    return NJS_ERROR;
}


static njs_int_t
njs_parser_template_literal(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_index_t        index;
    njs_parser_node_t  *node, *array, *template, *temp;

    temp = njs_parser_node_new(parser, 0);
    if (temp == NULL) {
        return NJS_ERROR;
    }

    array = njs_parser_node_new(parser, NJS_TOKEN_ARRAY);
    if (array == NULL) {
        return NJS_ERROR;
    }

    array->token_line = token->line;

    template = parser->node;
    index = NJS_SCOPE_CALLEE_ARGUMENTS;

    if (template->token_type != NJS_TOKEN_TEMPLATE_LITERAL) {
        node = njs_parser_argument(parser, array, index);
        if (node == NULL) {
            return NJS_ERROR;
        }

        template->right = node;
        temp->right = node;

        index += sizeof(njs_value_t);

    } else {
        template->left = array;
        temp->right = template;
    }

    temp->left = template;
    temp->index = index;

    parser->target = temp;

    token->text.start++;
    token->text.length = 0;

    njs_parser_next(parser, njs_parser_template_literal_string);

    return NJS_OK;
}


static njs_int_t
njs_parser_template_literal_string(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t          ret, ret_item;
    njs_parser_node_t  *template;

    template = parser->target->left;

    ret = njs_parser_template_string(parser, token);
    if (ret == NJS_ERROR) {
        njs_parser_syntax_error(parser, "Unterminated template literal");
        return NJS_DONE;
    }

    if (template->token_type != NJS_TOKEN_TEMPLATE_LITERAL) {
        ret_item = njs_parser_array_item(parser, template->right->left,
                                         parser->node);

    } else {
        ret_item = njs_parser_array_item(parser, template->left, parser->node);
    }

    if (ret_item != NJS_OK) {
        return NJS_ERROR;
    }

    if (ret == NJS_DONE) {
        parser->node = template;

        njs_parser_node_free(parser, parser->target);
        njs_lexer_consume_token(parser->lexer, 1);

        return njs_parser_stack_pop(parser);
    }

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_expression);

    njs_lexer_consume_token(parser->lexer, 1);

    return njs_parser_after(parser, current, parser->target, 0,
                            njs_parser_template_literal_expression);
}


static njs_int_t
njs_parser_template_literal_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t          ret;
    njs_parser_node_t  *template, *node, *parent;

    if (parser->ret != NJS_OK) {
        return njs_parser_failed(parser);
    }

    if (token->type != NJS_TOKEN_CLOSE_BRACE) {
        njs_parser_syntax_error(parser, "Missing \"}\" in template expression");
        return NJS_DONE;
    }

    parent = parser->target->right;
    template = parser->target->left;

    if (template->token_type != NJS_TOKEN_TEMPLATE_LITERAL) {
        node = njs_parser_argument(parser, parser->node, parser->target->index);
        if (node == NULL) {
            return NJS_ERROR;
        }

        parent->right = node;
        parent = node;

        parser->target->index += sizeof(njs_value_t);

    } else {
        ret = njs_parser_array_item(parser, template->left, parser->node);
        if (ret != NJS_OK) {
            return NJS_ERROR;
        }
    }

    parser->target->right = parent;

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_template_literal_string);

    token->text.length = 0;
    token->text.start += 1;

    return NJS_OK;
}


static njs_int_t
njs_parser_cover_parenthesized_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    switch (token->type) {
    case NJS_TOKEN_CLOSE_PARENTHESIS:
        (void) njs_parser_stack_pop(parser);
        break;

    case NJS_TOKEN_ELLIPSIS:
        njs_parser_next(parser, njs_parser_binding_identifier_pattern);
        break;

    default:
        parser->node = NULL;

        njs_parser_next(parser, njs_parser_expression);

        return njs_parser_after(parser, current, NULL, 0,
                               njs_parser_cover_parenthesized_expression_after);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    return NJS_OK;
}


static njs_int_t
njs_parser_binding_identifier_pattern(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    /*
     * BindingIdentifier )
     * BindingPattern )
     */

    switch (token->type) {

    /* BindingIdentifier */
    case NJS_TOKEN_NAME:
        njs_parser_next(parser, njs_parser_cover_parenthesized_expression_end);
        break;

    case NJS_TOKEN_YIELD:
        njs_parser_next(parser, njs_parser_cover_parenthesized_expression_end);
        break;

    case NJS_TOKEN_AWAIT:
        njs_parser_next(parser, njs_parser_cover_parenthesized_expression_end);
        break;

    /* BindingPattern */
    case NJS_TOKEN_OPEN_BRACKET:
        njs_parser_next(parser, njs_parser_array_binding_pattern);

        njs_lexer_consume_token(parser->lexer, 1);
        return njs_parser_after(parser, current, NULL, 0,
                                njs_parser_cover_parenthesized_expression_end);

    case NJS_TOKEN_OPEN_BRACE:
        njs_parser_next(parser, njs_parser_object_binding_pattern);

        njs_lexer_consume_token(parser->lexer, 1);
        return njs_parser_after(parser, current, NULL, 0,
                                njs_parser_cover_parenthesized_expression_end);

    default:
        return NJS_ERROR;
    }

    njs_lexer_consume_token(parser->lexer, 1);

    return NJS_OK;
}


static njs_int_t
njs_parser_cover_parenthesized_expression_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    /*
     * )
     * ,)
     * , ... BindingIdentifier )
     * , ... BindingPattern )
     */

    if (token->type == NJS_TOKEN_CLOSE_PARENTHESIS) {
        goto shift_stack;
    }

    if (token->type != NJS_TOKEN_COMMA) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (njs_slow_path(token == NULL)) {
        return NJS_ERROR;
    }

    if (token->type == NJS_TOKEN_CLOSE_PARENTHESIS) {
        goto shift_stack;
    }

    if(token->type != NJS_TOKEN_ELLIPSIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    njs_parser_next(parser, njs_parser_binding_identifier_pattern);

    return NJS_OK;

shift_stack:

    njs_lexer_consume_token(parser->lexer, 1);

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_cover_parenthesized_expression_end(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_CLOSE_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    return njs_parser_stack_pop(parser);
}


/*
 * 12.2.5 Array Initializer.
 */
static njs_int_t
njs_parser_array_literal(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    parser->target = parser->node;
    parser->node = NULL;

    njs_parser_next(parser, njs_parser_array_element_list);

    return NJS_OK;
}


static njs_int_t
njs_parser_array_element_list(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *array;

    array = parser->target;

    switch (token->type) {
    case NJS_TOKEN_CLOSE_BRACKET:
        njs_lexer_consume_token(parser->lexer, 1);

        parser->node = array;

        return njs_parser_stack_pop(parser);

    case NJS_TOKEN_COMMA:
        njs_lexer_consume_token(parser->lexer, 1);

        array->ctor = 1;
        array->u.length++;

        return NJS_OK;

    case NJS_TOKEN_ELLIPSIS:
        njs_lexer_consume_token(parser->lexer, 1);

        njs_parser_next(parser, njs_parser_assignment_expression);

        return njs_parser_after(parser, current, array, 0,
                                njs_parser_array_spread_element);
    default:
        break;
    }

    njs_parser_next(parser, njs_parser_assignment_expression);

    return njs_parser_after(parser, current, array, 0, njs_parser_array_after);
}


static njs_int_t
njs_parser_array_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t  ret;

    if (parser->ret != NJS_OK) {
        return njs_parser_failed(parser);
    }

    ret = njs_parser_array_item(parser, parser->target, parser->node);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    switch (token->type) {
    case NJS_TOKEN_COMMA:
        njs_lexer_consume_token(parser->lexer, 1);

        /* Fall through. */

    case NJS_TOKEN_CLOSE_BRACKET:
        njs_parser_next(parser, njs_parser_array_element_list);
        break;

    default:
        return njs_parser_failed(parser);
    }

    return NJS_OK;
}


static njs_int_t
njs_parser_array_spread_element(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (parser->ret != NJS_OK || token->type != NJS_TOKEN_CLOSE_BRACKET) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


/*
 * 12.2.6 Object Initializer.
 */
static njs_int_t
njs_parser_object_literal(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t *node;

    node = njs_parser_node_new(parser, 0);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->left = parser->node;

    parser->node = NULL;
    parser->target = node;

    njs_parser_next(parser, njs_parser_property_definition_list);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_object_literal_after);
}


static njs_int_t
njs_parser_object_literal_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (token->type == NJS_TOKEN_COMMA) {
        njs_lexer_consume_token(parser->lexer, 1);

        token = njs_lexer_token(parser->lexer, 1);
        if (token == NULL) {
            return NJS_ERROR;
        }
    }

    if (token->type != NJS_TOKEN_CLOSE_BRACE) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    parser->node = parser->target->left;

    njs_parser_node_free(parser, parser->target);
    parser->target = NULL;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_property_definition_list(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_property_definition);

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_property_definition_list_after);
}


static njs_int_t
njs_parser_property_definition_list_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_COMMA) {
        return njs_parser_stack_pop(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    njs_parser_next(parser, njs_parser_property_definition);

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_property_definition_list_after);
}


njs_inline njs_parser_node_t *
njs_parser_property_name_node(njs_parser_t *parser, njs_lexer_token_t *token)
{
    njs_int_t          ret;
    njs_parser_node_t  *property;

    if (token->type == NJS_TOKEN_NUMBER) {
        property = njs_parser_node_new(parser, NJS_TOKEN_NUMBER);
        if (property != NULL) {
             njs_set_number(&property->u.value, token->number);
        }

    } else if (token->type == NJS_TOKEN_ESCAPE_STRING) {
        property = njs_parser_node_new(parser, NJS_TOKEN_STRING);

        if (property != NULL) {
            ret = njs_parser_escape_string_create(parser, token,
                                                  &property->u.value);
            if (ret != NJS_TOKEN_STRING) {
                return NULL;
            }
        }

    } else {
        property = njs_parser_node_string(parser->vm, token, parser);
    }

    if (property != NULL) {
        property->token_line = token->line;
    }

    return property;
}


njs_inline njs_int_t
njs_parser_property_name(njs_parser_t *parser, njs_queue_link_t *current,
    unsigned consume)
{
    njs_lexer_token_t  *token;
    njs_parser_node_t  *property;

    if (consume > 1) {
        token = njs_lexer_token(parser->lexer, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        property = njs_parser_property_name_node(parser, token);
        if (property == NULL) {
            return NJS_ERROR;
        }

        parser->target->right = property;
        parser->target->index = (token->type == NJS_TOKEN_NAME);
    }

    njs_lexer_consume_token(parser->lexer, consume);

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_assignment_expression);

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_property_definition_after);
}


static njs_int_t
njs_parser_property_definition_ident(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_parser_node_t *temp)
{
    temp->right = njs_parser_node_string(parser->vm, token, parser);
    if (temp->right == NULL) {
        return NJS_ERROR;
    }

    temp->right->index = NJS_TOKEN_OPEN_BRACKET;

    parser->node = njs_parser_reference(parser, token);
    if (parser->node == NULL) {
        return NJS_ERROR;
    }

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    /* CoverInitializedName */
    if (token->type == NJS_TOKEN_ASSIGNMENT) {
        return njs_parser_not_supported(parser, token);
    }

    njs_parser_next(parser, njs_parser_property_definition_after);

    return NJS_OK;
}


static njs_int_t
njs_parser_property_definition(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_str_t          *name;
    njs_token_type_t   accessor;
    njs_lexer_token_t  *next;
    njs_parser_node_t  *temp, *property;

    temp = parser->target;

    switch (token->type) {
    case NJS_TOKEN_CLOSE_BRACE:
        return njs_parser_stack_pop(parser);

    /* PropertyName */
    case NJS_TOKEN_STRING:
    case NJS_TOKEN_ESCAPE_STRING:
    case NJS_TOKEN_NUMBER:
        next = njs_lexer_peek_token(parser->lexer, token, 0);
        if (next == NULL) {
            return NJS_ERROR;
        }

        if (next->type == NJS_TOKEN_COLON) {
            return njs_parser_property_name(parser, current, 2);

        } else if (next->type == NJS_TOKEN_OPEN_PARENTHESIS) {
            goto method_definition;
        }

        njs_lexer_consume_token(parser->lexer, 1);

        return njs_parser_failed(parser);

    /* ComputedPropertyName */
    case NJS_TOKEN_OPEN_BRACKET:
        njs_lexer_consume_token(parser->lexer, 1);

        njs_parser_next(parser, njs_parser_assignment_expression);

        return njs_parser_after(parser, current, temp, 1,
                                njs_parser_computed_property_name_after);

    case NJS_TOKEN_ELLIPSIS:
        return njs_parser_not_supported(parser, token);

    default:
        if (!njs_lexer_token_is_identifier_name(token)) {
            return njs_parser_reject(parser);
        }

        next = njs_lexer_peek_token(parser->lexer, token, 0);
        if (next == NULL) {
            return NJS_ERROR;
        }

        /* PropertyName */
        if (next->type == NJS_TOKEN_COLON) {
            return njs_parser_property_name(parser, current, 2);

        /* MethodDefinition */
        } else if (next->type == NJS_TOKEN_OPEN_PARENTHESIS) {
            goto method_definition;

        } else if (njs_lexer_token_is_reserved(token)) {
            njs_lexer_consume_token(parser->lexer, 1);

            return njs_parser_failed(parser);
        }

        name = &token->text;

        if (name->length == 3 && (memcmp(name->start, "get", 3) == 0
                                  || memcmp(name->start, "set", 3) == 0))
        {
            accessor = (name->start[0] == 'g') ? NJS_TOKEN_PROPERTY_GETTER
                                               : NJS_TOKEN_PROPERTY_SETTER;

            temp->right = (njs_parser_node_t *) (uintptr_t) accessor;

            njs_parser_next(parser, njs_parser_get_set);

            return NJS_OK;
        }

        return njs_parser_property_definition_ident(parser, token, temp);
    }

method_definition:

    property = njs_parser_property_name_node(parser, token);
    if (property == NULL) {
        return NJS_ERROR;
    }

    temp->right = property;
    temp->right->index = NJS_TOKEN_OPEN_BRACKET;

    njs_parser_next(parser, njs_parser_method_definition);

    return njs_parser_after(parser, current, temp, 1,
                            njs_parser_property_definition_after);
}


static njs_int_t
njs_parser_property_definition_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t          ret;
    njs_str_t          name;
    njs_bool_t         proto_init;
    njs_parser_node_t  *property, *temp;

    static const njs_str_t  proto_string = njs_str("__proto__");

    temp = parser->target;
    property = temp->right;

    proto_init = 0;

    if (property->index != NJS_TOKEN_OPEN_BRACKET
        && njs_is_string(&property->u.value))
    {
        njs_string_get(&property->u.value, &name);

        if (njs_slow_path(njs_strstr_eq(&name, &proto_string))) {
            if (temp->token_type == NJS_TOKEN_PROTO_INIT) {
                njs_parser_syntax_error(parser,
                                   "Duplicate __proto__ fields are not allowed "
                                   "in object literals");
                return NJS_ERROR;
            }

            temp->token_type = NJS_TOKEN_PROTO_INIT;
            proto_init = 1;
        }
    }

    if (property->index != 0) {
        property->index = 0;
    }

    ret = njs_parser_object_property(parser, temp->left, property,
                                     parser->node, proto_init);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    temp->right = NULL;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_computed_property_name_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t  *expr;

    if (token->type != NJS_TOKEN_CLOSE_BRACKET) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    /*
     * For further identification.
     * In njs_parser_property_definition_after() index will be reset to zero.
     */
    parser->node->index = NJS_TOKEN_OPEN_BRACKET;

    parser->target->right = parser->node;

    if (token->type == NJS_TOKEN_COLON) {
        return njs_parser_property_name(parser, current, 1);

    /* MethodDefinition */
    } else if (token->type == NJS_TOKEN_OPEN_PARENTHESIS) {
        expr = njs_parser_node_new(parser, NJS_TOKEN_FUNCTION_EXPRESSION);
        if (expr == NULL) {
            return NJS_ERROR;
        }

        expr->token_line = token->line;

        parser->node = expr;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_function_lambda);

        return njs_parser_after(parser, current, parser->target, 1,
                                njs_parser_property_definition_after);
    }

    return njs_parser_failed(parser);
}


static njs_int_t
njs_parser_initializer(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *node;

    if (token->type != NJS_TOKEN_ASSIGNMENT) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    node = parser->node;

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_assignment_expression);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_initializer_after);
}


static njs_int_t
njs_parser_initializer_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *stmt;

    stmt = njs_parser_node_new(parser, NJS_TOKEN_STATEMENT);
    if (stmt == NULL) {
        return NJS_ERROR;
    }

    stmt->left = NULL;
    stmt->right = parser->target;

    parser->target->right = parser->node;
    parser->node = stmt;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_initializer_assign(njs_parser_t *parser, njs_token_type_t type)
{
    njs_parser_node_t  *assign;

    assign = njs_parser_node_new(parser, type);
    if (assign == NULL) {
        return NJS_ERROR;
    }

    assign->u.operation = NJS_VMCODE_MOVE;
    assign->left = parser->node;

    /* assign->right in njs_parser_initializer_after. */

    parser->node = assign;

    return NJS_OK;
}


/*
 * 12.3 Left-Hand-Side Expressions.
 */
static njs_int_t
njs_parser_property(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *node, *prop_node;

    /*
     * [ Expression ]
     * . IdentifierName
     * TemplateLiteral
     */

    switch (token->type) {
    case NJS_TOKEN_OPEN_BRACKET:
        node = njs_parser_node_new(parser, NJS_TOKEN_PROPERTY);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->u.operation = NJS_VMCODE_PROPERTY_GET;
        node->left = parser->node;
        node->token_line = token->line;

        parser->node = NULL;

        njs_lexer_consume_token(parser->lexer, 1);

        njs_parser_next(parser, njs_parser_expression);

        return njs_parser_after(parser, current, node, 1,
                                njs_parser_member_expression_bracket);

    case NJS_TOKEN_DOT:
        token = njs_lexer_peek_token(parser->lexer, token, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        if (njs_lexer_token_is_identifier_name(token)) {
            node = njs_parser_node_new(parser, NJS_TOKEN_PROPERTY);
            if (node == NULL) {
                return NJS_ERROR;
            }

            node->u.operation = NJS_VMCODE_PROPERTY_GET;
            node->token_line = token->line;

            prop_node = njs_parser_node_string(parser->vm, token, parser);
            if (prop_node == NULL) {
                return NJS_ERROR;
            }

            prop_node->token_line = token->line;

            node->left = parser->node;
            node->right = prop_node;

            parser->node = node;

            njs_lexer_consume_token(parser->lexer, 2);
            return NJS_AGAIN;
        }

        njs_lexer_consume_token(parser->lexer, 1);

        return NJS_DECLINED;

    case NJS_TOKEN_GRAVE:
        node = njs_parser_create_call(parser, parser->node, 0);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->token_line = token->line;

        parser->node = node;

        njs_parser_next(parser, njs_parser_template_literal);

        break;

    default:
        return NJS_DONE;
    }

    return NJS_OK;
}


static njs_int_t
njs_parser_member_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t  ret;

    /*
     * PrimaryExpression
     * MemberExpression [ Expression ]
     * MemberExpression . IdentifierName
     * MemberExpression TemplateLiteral
     * SuperProperty
     * MetaProperty
     * new MemberExpression Arguments
     */

    switch (token->type) {
    /* SuperProperty */
    case NJS_TOKEN_SUPER:
        /* TODO: By specification. */
        (void) njs_parser_super_property;
        return njs_parser_not_supported(parser, token);

    /* MetaProperty */
    case NJS_TOKEN_IMPORT:
        /* TODO: By specification. */
        (void) njs_parser_member_expression_import;
        return njs_parser_not_supported(parser, token);

    case NJS_TOKEN_NEW:
        njs_lexer_consume_token(parser->lexer, 1);

        njs_parser_next(parser, njs_parser_member_expression_new);
        break;

    default:
        ret = njs_parser_primary_expression_test(parser, token, current);

        if (ret != NJS_OK) {
            if (ret == NJS_DONE) {
                njs_parser_next(parser, njs_parser_member_expression_next);
                return NJS_OK;
            }

            if (njs_is_error(&parser->vm->retval)) {
                return NJS_DONE;
            }

            return ret;
        }

        break;
    }

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_member_expression_next);
}


static njs_int_t
njs_parser_member_expression_next(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t  ret;

    /*
     * [ Expression ]
     * . IdentifierName
     * TemplateLiteral
     */

    ret = njs_parser_property(parser, token, current);

    switch (ret) {
    case NJS_AGAIN:
        return NJS_OK;

    case NJS_DONE:
        return njs_parser_stack_pop(parser);

    case NJS_DECLINED:
        return njs_parser_failed(parser);

    default:
        break;
    }

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_member_expression_next);
}


static njs_int_t
njs_parser_member_expression_bracket(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_CLOSE_BRACKET) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    parser->target->right = parser->node;
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_member_expression_new_next(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t  ret;

    /*
     * PrimaryExpression
     * SuperProperty
     * MetaProperty
     * Arguments
     */

    switch (token->type) {
    /* SuperProperty */
    case NJS_TOKEN_SUPER:
        (void) njs_parser_super_property;
        return njs_parser_not_supported(parser, token);

    /* MetaProperty */
    case NJS_TOKEN_IMPORT:
        (void) njs_parser_member_expression_import;
        return njs_parser_not_supported(parser, token);

    default:
        ret = njs_parser_primary_expression_test(parser, token, current);

        if (ret != NJS_OK) {
            if (ret == NJS_DONE) {
                njs_parser_next(parser, njs_parser_member_expression_next);
                return NJS_OK;
            }

            return ret;
        }

        return njs_parser_after(parser, current, NULL, 1,
                                njs_parser_member_expression_next);
    }
}


static njs_int_t
njs_parser_super_property(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    /*
     * [ Expression ]
     * . IdentifierName
     */

    if (token->type == NJS_TOKEN_OPEN_BRACKET) {
        parser->node = NULL;

        njs_parser_next(parser, njs_parser_expression);

        return njs_parser_after(parser, current, NULL, 1,
                                njs_parser_close_bracked);
    }

    if (token->type != NJS_TOKEN_DOT) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (!njs_lexer_token_is_identifier_name(token)) {
        return njs_parser_failed(parser);
    }

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_member_expression_import(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    /*
     * import . meta
     */

    if (token->type != NJS_TOKEN_DOT) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type != NJS_TOKEN_META) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_member_expression_new(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    /*
     * new MemberExpression Arguments
     * new . target
     */

    if (token->type != NJS_TOKEN_DOT) {
        njs_parser_next(parser, njs_parser_member_expression_new_next);

        return njs_parser_after(parser, current, NULL, 1,
                                njs_parser_member_expression_new_after);
    }

    /* njs_lexer_consume_token(parser->lexer, 1); */

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type != NJS_TOKEN_TARGET) {
        return njs_parser_failed(parser);
    }

    /* njs_lexer_consume_token(parser->lexer, 1); */

    /* return njs_parser_stack_pop(parser); */
    return njs_parser_not_supported(parser, token);
}


static njs_int_t
njs_parser_member_expression_new_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t  *func;

    if (token->type != NJS_TOKEN_OPEN_PARENTHESIS) {
        parser->node = njs_parser_create_call(parser, parser->node, 1);
        if (parser->node == NULL) {
            return NJS_ERROR;
        }

        parser->node->token_line = token->line;

        return njs_parser_stack_pop(parser);
    }

    func = njs_parser_create_call(parser, parser->node, 1);
    if (func == NULL) {
        return NJS_ERROR;
    }

    func->token_line = token->line;
    parser->node = func;

    njs_lexer_consume_token(parser->lexer, 1);
    njs_parser_next(parser, njs_parser_arguments);

    return njs_parser_after(parser, current, func, 1,
                            njs_parser_member_expression_new_args);
}


static njs_int_t
njs_parser_member_expression_new_args(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


static njs_parser_node_t *
njs_parser_create_call(njs_parser_t *parser, njs_parser_node_t *node,
    uint8_t ctor)
{
    njs_parser_node_t  *func;

    switch (node->token_type) {
    case NJS_TOKEN_NAME:
        func = node;
        func->token_type = NJS_TOKEN_FUNCTION_CALL;

        break;

    case NJS_TOKEN_PROPERTY:
        func = njs_parser_node_new(parser, NJS_TOKEN_METHOD_CALL);
        if (func == NULL) {
            return NULL;
        }

        func->left = node;
        break;

    default:
        /*
         * NJS_TOKEN_METHOD_CALL,
         * NJS_TOKEN_FUNCTION_CALL,
         * NJS_TOKEN_FUNCTION_EXPRESSION,
         * NJS_TOKEN_OPEN_PARENTHESIS,
         * NJS_TOKEN_EVAL.
         */
        func = njs_parser_node_new(parser, NJS_TOKEN_FUNCTION_CALL);
        if (func == NULL) {
            return NULL;
        }

        func->left = node;
        break;
    }

    func->ctor = ctor;

    return func;
}


static njs_int_t
njs_parser_call_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t  ret;

    /*
     * CoverCallExpressionAndAsyncArrowHead
     * SuperCall
     * ImportCall
     * CallExpression Arguments
     * CallExpression [ Expression ]
     * CallExpression . IdentifierName
     * CallExpression TemplateLiteral
     */

    switch (token->type) {
    /* MemberExpression or SuperCall */
    case NJS_TOKEN_SUPER:
        return njs_parser_not_supported(parser, token);

    case NJS_TOKEN_IMPORT:
        return njs_parser_not_supported(parser, token);

    default:
        break;
    }

    njs_parser_next(parser, njs_parser_member_expression);

    ret = njs_parser_after(parser, current, NULL, 1,
                           njs_parser_call_expression_args);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_call_expression_after);
}


static njs_int_t
njs_parser_call_expression_args(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *func;

    if (token->type != NJS_TOKEN_OPEN_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    func = njs_parser_create_call(parser, parser->node, 0);
    if (func == NULL) {
        return NJS_ERROR;
    }

    func->token_line = token->line;
    parser->node = func;

    njs_lexer_consume_token(parser->lexer, 1);
    njs_parser_next(parser, njs_parser_arguments);

    return njs_parser_after(parser, current, func, 1,
                            njs_parser_left_hand_side_expression_node);
}


static njs_int_t
njs_parser_call_expression_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t          ret;
    njs_parser_node_t  *func;

    /*
     * Arguments
     * [ Expression ]
     * . IdentifierName
     * TemplateLiteral
     */

    switch (token->type) {
    case NJS_TOKEN_OPEN_PARENTHESIS:
        func = njs_parser_create_call(parser, parser->node, 0);
        if (func == NULL) {
            return NJS_ERROR;
        }

        func->token_line = token->line;
        parser->node = func;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_arguments);

        ret = njs_parser_after(parser, current, func, 1,
                               njs_parser_left_hand_side_expression_node);
        if (ret != NJS_OK) {
            return NJS_ERROR;
        }

        break;

    default:
        ret = njs_parser_property(parser, token, current);

        switch (ret) {
        case NJS_AGAIN:
            return NJS_OK;

        case NJS_DONE:
            return njs_parser_stack_pop(parser);

        case NJS_DECLINED:
            return njs_parser_failed(parser);

        default:
            break;
        }

        break;
    }

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_call_expression_after);
}


static njs_int_t
njs_parser_arguments(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    /*
     * )
     * ArgumentList )
     * ArgumentList , )
     */

    if (token->type == NJS_TOKEN_CLOSE_PARENTHESIS) {
        njs_lexer_consume_token(parser->lexer, 1);
        return njs_parser_stack_pop(parser);
    }

    njs_parser_next(parser, njs_parser_argument_list);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_parenthesis_or_comma);
}


static njs_int_t
njs_parser_parenthesis_or_comma(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (token->type == NJS_TOKEN_CLOSE_PARENTHESIS) {
        njs_lexer_consume_token(parser->lexer, 1);
        return njs_parser_stack_pop(parser);
    }

    if (token->type != NJS_TOKEN_COMMA) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (njs_slow_path(token == NULL)) {
        return NJS_ERROR;
    }

    if (token->type != NJS_TOKEN_CLOSE_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_argument_list(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    /*
     * AssignmentExpression
     * ... AssignmentExpression
     * ArgumentList , AssignmentExpression
     * ArgumentList , ... AssignmentExpression
     */

    if (token->type == NJS_TOKEN_ELLIPSIS) {
        njs_lexer_consume_token(parser->lexer, 1);
    }

    njs_parser_next(parser, njs_parser_assignment_expression);

    return njs_parser_after(parser, current, parser->node, 1,
                            njs_parser_argument_list_after);
}


static njs_int_t
njs_parser_argument_list_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *node;

    node = njs_parser_node_new(parser, NJS_TOKEN_ARGUMENT);
    if (node == NULL) {
        return NJS_ERROR;
    }

    if (parser->target->index == 0) {
        node->index = NJS_SCOPE_CALLEE_ARGUMENTS;

    } else {
        node->index = parser->target->index + sizeof(njs_value_t);
    }

    node->token_line = token->line;
    node->left = parser->node;

    parser->node->dest = node;
    parser->target->right = node;
    parser->node = node;

    if (token->type == NJS_TOKEN_COMMA) {
        njs_lexer_consume_token(parser->lexer, 1);

        token = njs_lexer_token(parser->lexer, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        if (token->type == NJS_TOKEN_CLOSE_PARENTHESIS) {
            return njs_parser_stack_pop(parser);
        }

        return njs_parser_argument_list(parser, token, current);
    }

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_optional_expression_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_CONDITIONAL) {
        return njs_parser_stack_pop(parser);
    }

    token = njs_lexer_peek_token(parser->lexer, token, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type != NJS_TOKEN_DOT) {
        return njs_parser_stack_pop(parser);
    }

    njs_parser_next(parser, njs_parser_optional_chain);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_optional_expression_after);
}


static njs_int_t
njs_parser_optional_chain(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t          ret;
    njs_parser_node_t  *func;

    /*
     * ? . Arguments
     * ? . [ Expression ]
     * ? . IdentifierName
     * ? . TemplateLiteral
     * OptionalChain Arguments
     * OptionalChain [ Expression ]
     * OptionalChain . IdentifierName
     * OptionalChain TemplateLiteral
     */

    if (token->type != NJS_TOKEN_CONDITIONAL) {
        return njs_parser_failed(parser);
    }

    token = njs_lexer_peek_token(parser->lexer, token, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type != NJS_TOKEN_DOT) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    switch (token->type) {
    case NJS_TOKEN_OPEN_PARENTHESIS:
        func = njs_parser_create_call(parser, parser->node, 0);
        if (func == NULL) {
            return NJS_ERROR;
        }

        func->token_line = token->line;
        parser->node = func;

        njs_lexer_consume_token(parser->lexer, 2);
        njs_parser_next(parser, njs_parser_arguments);

        ret = njs_parser_after(parser, current, func, 1,
                               njs_parser_left_hand_side_expression_node);
        if (ret != NJS_OK) {
            return NJS_ERROR;
        }

        break;

    default:
        ret = njs_parser_property(parser, token, current);

        switch (ret) {
        case NJS_DONE:
        case NJS_DECLINED:
            return njs_parser_failed(parser);

        default:
            break;
        }

        break;
    }

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_optional_chain_after);
}


static njs_int_t
njs_parser_optional_chain_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t          ret;
    njs_parser_node_t  *func;

    /*
     * OptionalChain Arguments
     * OptionalChain [ Expression ]
     * OptionalChain . IdentifierName
     * OptionalChain TemplateLiteral
     */

    switch (token->type) {
    case NJS_TOKEN_OPEN_PARENTHESIS:
        func = njs_parser_create_call(parser, parser->node, 0);
        if (func == NULL) {
            return NJS_ERROR;
        }

        func->token_line = token->line;
        parser->node = func;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_arguments);

        ret = njs_parser_after(parser, current, func, 1,
                               njs_parser_left_hand_side_expression_node);
        if (ret != NJS_OK) {
            return NJS_ERROR;
        }

        break;

    default:
        ret = njs_parser_property(parser, token, current);

        switch (ret) {
        case NJS_AGAIN:
            return NJS_OK;

        case NJS_DONE:
            return njs_parser_stack_pop(parser);

        case NJS_DECLINED:
            return njs_parser_failed(parser);

        default:
            break;
        }

        break;
    }

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_optional_chain_after);
}


static njs_int_t
njs_parser_new_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_NEW) {
        parser->node = NULL;

        njs_parser_next(parser, njs_parser_member_expression_new);

        return NJS_OK;
    }

    njs_lexer_consume_token(parser->lexer, 1);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_new_expression_after);
}


static njs_int_t
njs_parser_new_expression_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *func;

    if (token->type == NJS_TOKEN_OPEN_PARENTHESIS) {
        njs_parser_next(parser, njs_parser_member_expression_new_after);
        return NJS_OK;
    }

    func = njs_parser_create_call(parser, parser->node, 1);
    if (func == NULL) {
        return NJS_ERROR;
    }

    func->token_line = token->line;
    parser->node = func;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_left_hand_side_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    /*
     * NewExpression = new MemberExpression
     * CallExpression = MemberExpression Arguments
     * OptionalExpression = MemberExpression OptionalChain
     */

    switch (token->type) {
    /* NewExpression or MemberExpression */
    case NJS_TOKEN_NEW:
        token = njs_lexer_peek_token(parser->lexer, token, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        if (token->type == NJS_TOKEN_NEW) {
            njs_lexer_consume_token(parser->lexer, 1);

            njs_parser_next(parser, njs_parser_new_expression);

            return njs_parser_after(parser, current, NULL, 1,
                                    njs_parser_left_hand_side_expression_after);
        }

        break;

    /* CallExpression or MemberExpression */
    case NJS_TOKEN_SUPER:
    case NJS_TOKEN_IMPORT:
        token = njs_lexer_peek_token(parser->lexer, token, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        if (token->type == NJS_TOKEN_OPEN_PARENTHESIS) {
            njs_parser_next(parser, njs_parser_call_expression);
            return NJS_OK;
        }

        break;

    default:
        break;
    }

    njs_parser_next(parser, njs_parser_member_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_left_hand_side_expression_after);
}


static njs_int_t
njs_parser_left_hand_side_expression_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t          ret;
    njs_parser_node_t  *func;

    switch (token->type) {
    /* CallExpression */
    case NJS_TOKEN_OPEN_PARENTHESIS:
        func = njs_parser_create_call(parser, parser->node, 0);
        if (func == NULL) {
            return NJS_ERROR;
        }

        func->token_line = token->line;
        parser->node = func;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_arguments);

        ret = njs_parser_after(parser, current, func, 1,
                               njs_parser_left_hand_side_expression_node);
        if (ret != NJS_OK) {
            return NJS_ERROR;
        }

        return njs_parser_after(parser, current, NULL, 1,
                                njs_parser_left_hand_side_expression_optional);

    /* OptionalExpression */
    case NJS_TOKEN_CONDITIONAL:
        njs_parser_next(parser, njs_parser_optional_expression_after);
        break;

    default:
        return njs_parser_stack_pop(parser);
    }

    return NJS_OK;
}


static njs_int_t
njs_parser_left_hand_side_expression_node(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_left_hand_side_expression_optional(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    /* OptionalExpression */
    if (token->type == NJS_TOKEN_CONDITIONAL) {
        njs_parser_next(parser, njs_parser_optional_expression_after);
        return NJS_OK;
    }

    njs_parser_next(parser, njs_parser_optional_chain_after);

    return NJS_OK;
}


static njs_int_t
njs_parser_expression_node(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current, njs_token_type_t type,
    njs_vmcode_operation_t operation, njs_parser_state_func_t after)
{
    njs_parser_node_t  *node;

    if (parser->target != NULL) {
        parser->target->right = parser->node;
        parser->target->right->dest = parser->target;
        parser->node = parser->target;
    }

    if (token->type != type) {
        return njs_parser_stack_pop(parser);
    }

    node = njs_parser_node_new(parser, type);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    node->u.operation = operation;
    node->left = parser->node;
    node->left->dest = node;

    njs_lexer_consume_token(parser->lexer, 1);

    return njs_parser_after(parser, current, node, 1, after);
}


/*
 * 12.4 Update Expressions.
 */
static njs_int_t
njs_parser_update_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t       *node;
    njs_vmcode_operation_t  operation;

    switch (token->type) {
    case NJS_TOKEN_INCREMENT:
        operation = NJS_VMCODE_INCREMENT;
        break;

    case NJS_TOKEN_DECREMENT:
        operation = NJS_VMCODE_DECREMENT;
        break;

    default:
        njs_parser_next(parser, njs_parser_left_hand_side_expression);

        return njs_parser_after(parser, current, NULL, 1,
                                njs_parser_update_expression_post);
    }

    node = njs_parser_node_new(parser, token->type);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    node->u.operation = operation;

    njs_lexer_consume_token(parser->lexer, 1);
    njs_parser_next(parser, njs_parser_left_hand_side_expression);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_update_expression_unary);
}


static njs_int_t
njs_parser_update_expression_post(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_token_type_t        type;
    njs_parser_node_t       *node;
    njs_vmcode_operation_t  operation;

    /* [no LineTerminator here] */

    switch (token->type) {
    case NJS_TOKEN_INCREMENT:
        type = NJS_TOKEN_POST_INCREMENT;
        operation = NJS_VMCODE_POST_INCREMENT;
        break;

    case NJS_TOKEN_DECREMENT:
        type = NJS_TOKEN_POST_DECREMENT;
        operation = NJS_VMCODE_POST_DECREMENT;
        break;

    default:
        return njs_parser_stack_pop(parser);
    }

    if (parser->lexer->prev_type == NJS_TOKEN_LINE_END) {
        return njs_parser_stack_pop(parser);
    }

    if (!njs_parser_is_lvalue(parser->node)) {
        njs_lexer_consume_token(parser->lexer, 1);

        njs_parser_ref_error(parser,
                             "Invalid left-hand side in postfix operation");
        return NJS_DONE;
    }

    node = njs_parser_node_new(parser, type);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    node->u.operation = operation;
    node->left = parser->node;

    parser->node = node;

    njs_lexer_consume_token(parser->lexer, 1);

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_update_expression_unary(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (!njs_parser_is_lvalue(parser->node)) {
        njs_parser_ref_error(parser,
                             "Invalid left-hand side in prefix operation");
        return NJS_DONE;
    }

    parser->target->left = parser->node;
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


/*
 * 12.5 Unary Operators.
 */
static njs_int_t
njs_parser_unary_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_token_type_t        type;
    njs_parser_node_t       *node;
    njs_vmcode_operation_t  operation;

    switch (token->type) {
    case NJS_TOKEN_DELETE:
        type = NJS_TOKEN_DELETE;
        operation = NJS_VMCODE_DELETE;
        break;

    case NJS_TOKEN_VOID:
        type = NJS_TOKEN_VOID;
        operation = NJS_VMCODE_VOID;
        break;

    case NJS_TOKEN_TYPEOF:
        type = NJS_TOKEN_TYPEOF;
        operation = NJS_VMCODE_TYPEOF;
        break;

    case NJS_TOKEN_ADDITION:
        type = NJS_TOKEN_UNARY_PLUS;
        operation = NJS_VMCODE_UNARY_PLUS;
        break;

    case NJS_TOKEN_SUBSTRACTION:
        type = NJS_TOKEN_UNARY_NEGATION;
        operation = NJS_VMCODE_UNARY_NEGATION;
        break;

    case NJS_TOKEN_BITWISE_NOT:
        type = NJS_TOKEN_BITWISE_NOT;
        operation = NJS_VMCODE_BITWISE_NOT;
        break;

    case NJS_TOKEN_LOGICAL_NOT:
        type = NJS_TOKEN_LOGICAL_NOT;
        operation = NJS_VMCODE_LOGICAL_NOT;
        break;

    /* AwaitExpression */
    case NJS_TOKEN_AWAIT:
        return njs_parser_not_supported(parser, token);

    default:
        njs_parser_next(parser, njs_parser_update_expression);

        return njs_parser_after(parser, current, parser->target, 1,
                                njs_parser_unary_expression_after);
    }

    node = njs_parser_node_new(parser, type);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    node->u.operation = operation;

    parser->target = node;

    njs_lexer_consume_token(parser->lexer, 1);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_unary_expression_next);
}


static njs_int_t
njs_parser_unary_expression_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (parser->target == NULL &&
        token->type == NJS_TOKEN_EXPONENTIATION)
    {
        return njs_parser_exponentiation_expression_match(parser, token,
                                                          current);
    }

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_unary_expression_next(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    double             num;
    njs_token_type_t   type;
    njs_parser_node_t  *node;

    type = parser->target->token_type;
    node = parser->node;

    if (token->type == NJS_TOKEN_EXPONENTIATION) {
        njs_parser_syntax_error(parser, "Either left-hand side or entire "
                                "exponentiation must be parenthesized");
        return NJS_DONE;
    }

    if (node->token_type == NJS_TOKEN_NUMBER) {
        if (type == NJS_TOKEN_UNARY_PLUS) {
            /* Skip the unary plus of number. */
            return njs_parser_stack_pop(parser);
        }

        if (type == NJS_TOKEN_UNARY_NEGATION) {
            /* Optimization of common negative number. */
            num = -njs_number(&node->u.value);
            njs_set_number(&node->u.value, num);

            return njs_parser_stack_pop(parser);
        }
    }

    if (type == NJS_TOKEN_DELETE) {
        switch (node->token_type) {

        case NJS_TOKEN_PROPERTY:
            node->token_type = NJS_TOKEN_PROPERTY_DELETE;
            node->u.operation = NJS_VMCODE_PROPERTY_DELETE;

            return njs_parser_stack_pop(parser);

        case NJS_TOKEN_NAME:
            njs_parser_syntax_error(parser,
                                    "Delete of an unqualified identifier");
            return NJS_DONE;

        default:
            break;
        }
    }

    if (type == NJS_TOKEN_TYPEOF && node->token_type == NJS_TOKEN_NAME) {
        node->u.reference.type = NJS_TYPEOF;
    }

    parser->target->left = parser->node;
    parser->target->left->dest = parser->target;
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


/*
 * 12.6 Exponentiation Operator.
 */
static njs_int_t
njs_parser_exponentiation_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    parser->target = NULL;

    njs_parser_next(parser, njs_parser_unary_expression);

    /* For UpdateExpression, see njs_parser_unary_expression_after. */

    return NJS_OK;
}


static njs_int_t
njs_parser_exponentiation_expression_match(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t  *node;

    if (parser->target != NULL) {
        parser->target->right = parser->node;
        parser->target->right->dest = parser->target;
        parser->node = parser->target;

        return njs_parser_stack_pop(parser);
    }

    if (token->type != NJS_TOKEN_EXPONENTIATION) {
        return njs_parser_stack_pop(parser);
    }

    node = njs_parser_node_new(parser, token->type);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    node->u.operation = NJS_VMCODE_EXPONENTIATION;
    node->left = parser->node;
    node->left->dest = node;

    njs_lexer_consume_token(parser->lexer, 1);

    njs_parser_next(parser, njs_parser_exponentiation_expression);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_exponentiation_expression_match);
}


/*
 * 12.7 Multiplicative Operators.
 */
static njs_int_t
njs_parser_multiplicative_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_exponentiation_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_multiplicative_expression_match);
}


static njs_int_t
njs_parser_multiplicative_expression_match(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t       *node;
    njs_vmcode_operation_t  operation;

    if (parser->target != NULL) {
        parser->target->right = parser->node;
        parser->target->right->dest = parser->target;
        parser->node = parser->target;
    }

    switch (token->type) {
    case NJS_TOKEN_MULTIPLICATION:
        operation = NJS_VMCODE_MULTIPLICATION;
        break;

    case NJS_TOKEN_DIVISION:
        operation = NJS_VMCODE_DIVISION;
        break;

    case NJS_TOKEN_REMAINDER:
        operation = NJS_VMCODE_REMAINDER;
        break;

    default:
        return njs_parser_stack_pop(parser);
    }

    node = njs_parser_node_new(parser, token->type);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    node->u.operation = operation;
    node->left = parser->node;
    node->left->dest = node;

    njs_lexer_consume_token(parser->lexer, 1);

    njs_parser_next(parser, njs_parser_exponentiation_expression);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_multiplicative_expression_match);
}


/*
 * 12.8 Additive Operators.
 */
static njs_int_t
njs_parser_additive_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_multiplicative_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_additive_expression_match);
}


static njs_int_t
njs_parser_additive_expression_match(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t       *node;
    njs_vmcode_operation_t  operation;

    if (parser->target != NULL) {
        parser->target->right = parser->node;
        parser->target->right->dest = parser->target;
        parser->node = parser->target;
    }

    switch (token->type) {
    case NJS_TOKEN_ADDITION:
        operation = NJS_VMCODE_ADDITION;
        break;

    case NJS_TOKEN_SUBSTRACTION:
        operation = NJS_VMCODE_SUBSTRACTION;
        break;

    default:
        return njs_parser_stack_pop(parser);
    }

    node = njs_parser_node_new(parser, token->type);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    node->u.operation = operation;
    node->left = parser->node;
    node->left->dest = node;

    njs_lexer_consume_token(parser->lexer, 1);

    njs_parser_next(parser, njs_parser_multiplicative_expression);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_additive_expression_match);
}


/*
 * 12.9 Bitwise Shift Operators
 */
static njs_int_t
njs_parser_shift_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_additive_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_shift_expression_match);
}


static njs_int_t
njs_parser_shift_expression_match(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t       *node;
    njs_vmcode_operation_t  operation;

    if (parser->target != NULL) {
        parser->target->right = parser->node;
        parser->target->right->dest = parser->target;
        parser->node = parser->target;
    }

    switch (token->type) {
    case NJS_TOKEN_RIGHT_SHIFT:
        operation = NJS_VMCODE_RIGHT_SHIFT;
        break;

    case NJS_TOKEN_LEFT_SHIFT:
        operation = NJS_VMCODE_LEFT_SHIFT;
        break;

    case NJS_TOKEN_UNSIGNED_RIGHT_SHIFT:
        operation = NJS_VMCODE_UNSIGNED_RIGHT_SHIFT;
        break;

    default:
        return njs_parser_stack_pop(parser);
    }

    node = njs_parser_node_new(parser, token->type);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    node->u.operation = operation;
    node->left = parser->node;
    node->left->dest = node;

    njs_lexer_consume_token(parser->lexer, 1);

    njs_parser_next(parser, njs_parser_additive_expression);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_shift_expression_match);
}


/*
 * 12.10 Relational Operators.
 */
static njs_int_t
njs_parser_relational_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_shift_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_relational_expression_match);
}


static njs_int_t
njs_parser_relational_expression_match(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t       *node;
    njs_vmcode_operation_t  operation;

    if (parser->target != NULL) {
        parser->target->right = parser->node;
        parser->target->right->dest = parser->target;
        parser->node = parser->target;
    }

    switch (token->type) {
    case NJS_TOKEN_LESS:
        operation = NJS_VMCODE_LESS;
        break;

    case NJS_TOKEN_GREATER:
        operation = NJS_VMCODE_GREATER;
        break;

    case NJS_TOKEN_LESS_OR_EQUAL:
        operation = NJS_VMCODE_LESS_OR_EQUAL;
        break;

    case NJS_TOKEN_GREATER_OR_EQUAL:
        operation = NJS_VMCODE_GREATER_OR_EQUAL;
        break;

    case NJS_TOKEN_INSTANCEOF:
        operation = NJS_VMCODE_INSTANCE_OF;
        break;

    case NJS_TOKEN_IN:
        operation = NJS_VMCODE_PROPERTY_IN;
        break;

    default:
        return njs_parser_stack_pop(parser);
    }

    node = njs_parser_node_new(parser, token->type);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    node->u.operation = operation;
    node->left = parser->node;
    node->left->dest = node;

    njs_lexer_consume_token(parser->lexer, 1);

    njs_parser_next(parser, njs_parser_shift_expression);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_relational_expression_match);
}


/*
 * 12.11 Equality Operators.
 */
static njs_int_t
njs_parser_equality_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_relational_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_equality_expression_match);
}


static njs_int_t
njs_parser_equality_expression_match(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t       *node;
    njs_vmcode_operation_t  operation;

    if (parser->target != NULL) {
        parser->target->right = parser->node;
        parser->target->right->dest = parser->target;
        parser->node = parser->target;
    }

    switch (token->type) {
    case NJS_TOKEN_EQUAL:
        operation = NJS_VMCODE_EQUAL;
        break;

    case NJS_TOKEN_NOT_EQUAL:
        operation = NJS_VMCODE_NOT_EQUAL;
        break;

    case NJS_TOKEN_STRICT_EQUAL:
        operation = NJS_VMCODE_STRICT_EQUAL;
        break;

    case NJS_TOKEN_STRICT_NOT_EQUAL:
        operation = NJS_VMCODE_STRICT_NOT_EQUAL;
        break;

    default:
        return njs_parser_stack_pop(parser);
    }

    node = njs_parser_node_new(parser, token->type);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    node->u.operation = operation;
    node->left = parser->node;
    node->left->dest = node;

    njs_lexer_consume_token(parser->lexer, 1);

    njs_parser_next(parser, njs_parser_relational_expression);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_equality_expression_match);
}


/*
 * 12.12 Binary Bitwise Operators.
 */
static njs_int_t
njs_parser_bitwise_AND_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_equality_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_bitwise_AND_expression_and);
}


static njs_int_t
njs_parser_bitwise_AND_expression_and(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_equality_expression);

    return njs_parser_expression_node(parser, token, current,
                                       NJS_TOKEN_BITWISE_AND,
                                       NJS_VMCODE_BITWISE_AND,
                                       njs_parser_bitwise_AND_expression_and);
}


static njs_int_t
njs_parser_bitwise_XOR_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_bitwise_AND_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_bitwise_XOR_expression_xor);
}


static njs_int_t
njs_parser_bitwise_XOR_expression_xor(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_bitwise_AND_expression);

    return njs_parser_expression_node(parser, token, current,
                                       NJS_TOKEN_BITWISE_XOR,
                                       NJS_VMCODE_BITWISE_XOR,
                                       njs_parser_bitwise_XOR_expression_xor);
}


static njs_int_t
njs_parser_bitwise_OR_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_bitwise_XOR_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_bitwise_OR_expression_or);
}


static njs_int_t
njs_parser_bitwise_OR_expression_or(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_bitwise_XOR_expression);

    return njs_parser_expression_node(parser, token, current,
                                       NJS_TOKEN_BITWISE_OR,
                                       NJS_VMCODE_BITWISE_OR,
                                       njs_parser_bitwise_OR_expression_or);
}


/*
 * 12.13 Binary Logical Operators.
 */
static njs_int_t
njs_parser_logical_AND_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_bitwise_OR_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_logical_AND_expression_and);
}


static njs_int_t
njs_parser_logical_AND_expression_and(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_bitwise_OR_expression);

    return njs_parser_expression_node(parser, token, current,
                                       NJS_TOKEN_LOGICAL_AND,
                                       NJS_VMCODE_TEST_IF_FALSE,
                                       njs_parser_logical_AND_expression_and);
}


static njs_int_t
njs_parser_logical_OR_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_logical_AND_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_logical_OR_expression_or);
}


static njs_int_t
njs_parser_logical_OR_expression_or(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_logical_AND_expression);

    return njs_parser_expression_node(parser, token, current,
                                       NJS_TOKEN_LOGICAL_OR,
                                       NJS_VMCODE_TEST_IF_TRUE,
                                       njs_parser_logical_OR_expression_or);
}


static njs_int_t
njs_parser_coalesce_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_token_type_t   type;
    njs_parser_node_t  *node;

    node = parser->node;

    if (parser->target != NULL) {
        parser->target->right = node;
        parser->target->right->dest = parser->target;
        parser->node = parser->target;
    }

    if (token->type != NJS_TOKEN_COALESCE) {
        return njs_parser_stack_pop(parser);
    }

    type = node->token_type;

    if (parser->lexer->prev_type != NJS_TOKEN_CLOSE_PARENTHESIS
        && (type == NJS_TOKEN_LOGICAL_OR || type == NJS_TOKEN_LOGICAL_AND))
    {
        return njs_parser_failed(parser);
    }

    node = njs_parser_node_new(parser, NJS_TOKEN_COALESCE);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    node->u.operation = NJS_VMCODE_COALESCE;
    node->left = parser->node;
    node->left->dest = node;

    njs_lexer_consume_token(parser->lexer, 1);
    njs_parser_next(parser, njs_parser_bitwise_OR_expression);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_coalesce_expression);
}


static njs_int_t
njs_parser_short_circuit_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_logical_OR_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_coalesce_expression);
}


/*
 * 12.14 Conditional Operator ( ? : ).
 */
static njs_int_t
njs_parser_conditional_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_short_circuit_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_conditional_question_mark);
}


static njs_int_t
njs_parser_conditional_question_mark(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t  *node, *cond;

    if (token->type != NJS_TOKEN_CONDITIONAL) {
        return njs_parser_stack_pop(parser);
    }

    cond = njs_parser_node_new(parser, NJS_TOKEN_CONDITIONAL);
    if (cond == NULL) {
        return NJS_ERROR;
    }

    cond->token_line = token->line;
    cond->left = parser->node;

    node = njs_parser_node_new(parser, NJS_TOKEN_BRANCHING);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    cond->right = node;

    njs_lexer_consume_token(parser->lexer, 1);
    njs_parser_next(parser, njs_parser_assignment_expression);

    return njs_parser_after(parser, current, cond, 1,
                            njs_parser_conditional_colon);
}


static njs_int_t
njs_parser_conditional_colon(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *node;

    if (token->type != NJS_TOKEN_COLON) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    node = parser->target->right;

    node->left = parser->node;
    node->left->dest = parser->target;

    njs_parser_next(parser, njs_parser_assignment_expression);

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_conditional_colon_after);
}


static njs_int_t
njs_parser_conditional_colon_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t  *node;

    node = parser->target->right;

    node->right = parser->node;
    node->right->dest = parser->target;

    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


/*
 * 12.15 Assignment Operators.
 */
static njs_int_t
njs_parser_assignment_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t  ret;

    ret = njs_parser_match_arrow_expression(parser, token);
    if (ret == NJS_OK) {
        njs_parser_next(parser, njs_parser_arrow_function);

        return NJS_OK;

    } else if (ret == NJS_ERROR) {
        return NJS_ERROR;
    }

    njs_parser_next(parser, njs_parser_conditional_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_assignment_expression_after);
}


/*
 * TODO: this function is a crutch.
 * See NJS_TOKEN_OPEN_PARENTHESIS in njs_parser_primary_expression_test.
 * and look CoverParenthesizedExpressionAndArrowParameterList in spec.
 */
static njs_int_t
njs_parser_match_arrow_expression(njs_parser_t *parser,
    njs_lexer_token_t *token)
{
    njs_bool_t  rest_parameters;

    if (token->type != NJS_TOKEN_OPEN_PARENTHESIS
        && !njs_lexer_token_is_binding_identifier(token))
    {
        return NJS_DECLINED;
    }

    if (njs_lexer_token_is_binding_identifier(token)) {
        goto arrow;
    }

    token = njs_lexer_peek_token(parser->lexer, token, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    rest_parameters = 0;

    while (token->type != NJS_TOKEN_CLOSE_PARENTHESIS) {

        if (rest_parameters) {
            return NJS_DECLINED;
        }

        if (token->type == NJS_TOKEN_ELLIPSIS) {
            rest_parameters = 1;

            token = njs_lexer_peek_token(parser->lexer, token, 0);
            if (token == NULL) {
                return NJS_ERROR;
            }
        }

        if (!njs_lexer_token_is_binding_identifier(token)) {
            return NJS_DECLINED;
        }

        token = njs_lexer_peek_token(parser->lexer, token, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        if (token->type == NJS_TOKEN_COMMA) {
            token = njs_lexer_peek_token(parser->lexer, token, 0);
            if (token == NULL) {
               return NJS_ERROR;
            }
        }
    }

arrow:

    token = njs_lexer_peek_token(parser->lexer, token, 1);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type == NJS_TOKEN_LINE_END) {
        return NJS_DECLINED;
    }

    if (token->type != NJS_TOKEN_ARROW) {
        return NJS_DECLINED;
    }

    return NJS_OK;
}


static njs_int_t
njs_parser_assignment_expression_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    return njs_parser_assignment_operator(parser, token, current);
}


static njs_int_t
njs_parser_assignment_operator(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_token_type_t        type;
    njs_parser_node_t       *node;
    njs_vmcode_operation_t  operation;

    switch (token->type) {
    case NJS_TOKEN_ASSIGNMENT:
        njs_thread_log_debug("JS: =");
        operation = NJS_VMCODE_MOVE;
        break;

    case NJS_TOKEN_MULTIPLICATION_ASSIGNMENT:
        njs_thread_log_debug("JS: *=");
        operation = NJS_VMCODE_MULTIPLICATION;
        break;

    case NJS_TOKEN_DIVISION_ASSIGNMENT:
        njs_thread_log_debug("JS: /=");
        operation = NJS_VMCODE_DIVISION;
        break;

    case NJS_TOKEN_REMAINDER_ASSIGNMENT:
        njs_thread_log_debug("JS: %=");
        operation = NJS_VMCODE_REMAINDER;
        break;

    case NJS_TOKEN_ADDITION_ASSIGNMENT:
        njs_thread_log_debug("JS: +=");
        operation = NJS_VMCODE_ADDITION;
        break;

    case NJS_TOKEN_SUBSTRACTION_ASSIGNMENT:
        njs_thread_log_debug("JS: -=");
        operation = NJS_VMCODE_SUBSTRACTION;
        break;

    case NJS_TOKEN_LEFT_SHIFT_ASSIGNMENT:
        njs_thread_log_debug("JS: <<=");
        operation = NJS_VMCODE_LEFT_SHIFT;
        break;

    case NJS_TOKEN_RIGHT_SHIFT_ASSIGNMENT:
        njs_thread_log_debug("JS: >>=");
        operation = NJS_VMCODE_RIGHT_SHIFT;
        break;

    case NJS_TOKEN_UNSIGNED_RIGHT_SHIFT_ASSIGNMENT:
        njs_thread_log_debug("JS: >>>=");
        operation = NJS_VMCODE_UNSIGNED_RIGHT_SHIFT;
        break;

    case NJS_TOKEN_BITWISE_AND_ASSIGNMENT:
        njs_thread_log_debug("JS: &=");
        operation = NJS_VMCODE_BITWISE_AND;
        break;

    case NJS_TOKEN_BITWISE_XOR_ASSIGNMENT:
        njs_thread_log_debug("JS: ^=");
        operation = NJS_VMCODE_BITWISE_XOR;
        break;

    case NJS_TOKEN_BITWISE_OR_ASSIGNMENT:
        njs_thread_log_debug("JS: |=");
        operation = NJS_VMCODE_BITWISE_OR;
        break;

    case NJS_TOKEN_EXPONENTIATION_ASSIGNMENT:
        njs_thread_log_debug("JS: **=");
        operation = NJS_VMCODE_EXPONENTIATION;
        break;

    default:
        return njs_parser_stack_pop(parser);
    }

    if (!njs_parser_is_lvalue(parser->node)) {
        type = parser->node->token_type;

        if (njs_parser_restricted_identifier(type)) {
            njs_parser_syntax_error(parser, "Identifier \"%s\" "
                                    "is forbidden as left-hand in assignment",
                                    (type == NJS_TOKEN_EVAL) ? "eval"
                                                             : "arguments");

        } else {
            njs_parser_ref_error(parser,
                                 "Invalid left-hand side in assignment");
        }

        return NJS_DONE;
    }

    node = njs_parser_node_new(parser, token->type);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    node->u.operation = operation;
    node->left = parser->node;

    njs_lexer_consume_token(parser->lexer, 1);
    njs_parser_next(parser, njs_parser_assignment_expression);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_assignment_operator_after);
}


static njs_int_t
njs_parser_assignment_operator_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    parser->target->right = parser->node;
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


/*
 * 12.16 Comma Operator ( , ).
 */
static njs_int_t
njs_parser_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_assignment_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_expression_comma);
}


static njs_int_t
njs_parser_expression_comma(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_assignment_expression);

    return njs_parser_expression_node(parser, token, current, NJS_TOKEN_COMMA,
                                       NJS_VMCODE_NOP,
                                       njs_parser_expression_comma);
}


/*
 * 13 Statements and Declarations.
 */
static njs_int_t
njs_parser_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t                 ret;
    njs_queue_link_t          *lnk;
    njs_parser_stack_entry_t  *entry;

    if (token->type == NJS_TOKEN_END) {
        lnk = njs_queue_next(njs_queue_first(&parser->stack));

        if (lnk == njs_queue_head(&parser->stack)) {
            return njs_parser_reject(parser);
        }

        entry = njs_queue_link_data(lnk, njs_parser_stack_entry_t, link);

        if (entry->state == njs_parser_check_error_state) {
            return NJS_DONE;
        }

        return njs_parser_reject(parser);
    }

    switch (token->type) {
    case NJS_TOKEN_SEMICOLON:
        njs_lexer_consume_token(parser->lexer, 1);
        return njs_parser_stack_pop(parser);

    case NJS_TOKEN_EXPORT:
        parser->line = token->line;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_export);

        return njs_parser_after(parser, current, parser->node, 1,
                                njs_parser_statement_after);

    case NJS_TOKEN_IMPORT:
        parser->line = token->line;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_import);

        return njs_parser_after(parser, current, parser->node, 1,
                                njs_parser_statement_after);
    default:
        break;
    }

    ret = njs_parser_statement_wo_node(parser, token, current);
    if (ret != NJS_OK) {
        return ret;
    }

    return njs_parser_after(parser, current, parser->node, 1,
                            njs_parser_statement_after);
}


static njs_int_t
njs_parser_statement_wo_node(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    switch (token->type) {
    case NJS_TOKEN_OPEN_BRACE:
        njs_parser_next(parser, njs_parser_block_statement);
        break;

    case NJS_TOKEN_VAR:
        njs_lexer_consume_token(parser->lexer, 1);
        return njs_parser_variable_statement(parser, token, current);

    case NJS_TOKEN_SEMICOLON:
        njs_lexer_consume_token(parser->lexer, 1);
        return njs_parser_stack_pop(parser);

    case NJS_TOKEN_IF:
        njs_parser_next(parser, njs_parser_if_statement);
        break;

    /* BreakableStatement */
    case NJS_TOKEN_DO:
        njs_parser_next(parser, njs_parser_iteration_statement_do);
        break;

    case NJS_TOKEN_WHILE:
        njs_parser_next(parser, njs_parser_iteration_statement_while);
        break;

    case NJS_TOKEN_FOR:
        njs_parser_next(parser, njs_parser_iteration_statement_for);
        break;

    case NJS_TOKEN_SWITCH:
        njs_parser_next(parser, njs_parser_switch_statement);
        break;

    case NJS_TOKEN_CONTINUE:
        njs_parser_next(parser, njs_parser_continue_statement);
        break;

    case NJS_TOKEN_BREAK:
        njs_parser_next(parser, njs_parser_break_statement);
        break;

    case NJS_TOKEN_RETURN:
        njs_parser_next(parser, njs_parser_return_statement);
        break;

    case NJS_TOKEN_WITH:
        njs_parser_next(parser, njs_parser_with_statement);
        break;

    case NJS_TOKEN_THROW:
        njs_parser_next(parser, njs_parser_throw_statement);
        break;

    case NJS_TOKEN_TRY:
        njs_parser_next(parser, njs_parser_try_statement);
        break;

    case NJS_TOKEN_DEBUGGER:
        njs_parser_next(parser, njs_parser_debugger_statement);
        break;

    case NJS_TOKEN_END:
        return njs_parser_failed(parser);

    default:
        if (njs_lexer_token_is_identifier_reference(token)) {
            token = njs_lexer_peek_token(parser->lexer, token, 0);
            if (token == NULL) {
                return NJS_ERROR;
            }

            if (token->type == NJS_TOKEN_COLON) {
                njs_parser_next(parser, njs_parser_labelled_statement);
                return NJS_OK;
            }
        }

        njs_parser_next(parser, njs_parser_expression_statement);
        return NJS_OK;
    }

    parser->line = token->line;

    njs_lexer_consume_token(parser->lexer, 1);

    return NJS_OK;
}


static njs_int_t
njs_parser_statement_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *stmt;

    stmt = njs_parser_node_new(parser, NJS_TOKEN_STATEMENT);
    if (njs_slow_path(stmt == NULL)) {
        return NJS_ERROR;
    }

    stmt->left = parser->target;
    stmt->right = parser->node;

    parser->node = stmt;

    njs_parser_chain_top_set(parser, stmt);

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_declaration(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t  ret;

    ret = njs_parser_hoistable_declaration(parser, token, current);
    if (ret == NJS_OK) {
        return NJS_OK;
    }

    switch (token->type) {
    case NJS_TOKEN_CLASS:
        njs_parser_next(parser, njs_parser_class_declaration);
        break;

    case NJS_TOKEN_LET:
    case NJS_TOKEN_CONST:
        token = njs_lexer_peek_token(parser->lexer, token, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        switch (token->type) {
        case NJS_TOKEN_OPEN_BRACE:
        case NJS_TOKEN_OPEN_BRACKET:
            njs_parser_next(parser, njs_parser_lexical_declaration);
            break;

        default:
            if (njs_lexer_token_is_binding_identifier(token)) {
                njs_parser_next(parser, njs_parser_lexical_declaration);
                break;
            }

            return NJS_DECLINED;
        }

        break;

    default:
        return NJS_DECLINED;
    }

    return NJS_OK;
}


static njs_int_t
njs_parser_hoistable_declaration(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t  ret;

    ret = njs_parser_function_or_generator(parser, token, current);
    if (ret == NJS_OK) {
        return NJS_OK;
    }

    ret = njs_parser_async_function_or_generator(parser, token, current);
    if (ret == NJS_OK) {
        return NJS_OK;
    }

    return NJS_DECLINED;
}


/*
 * 13.2 Block.
 */
static njs_int_t
njs_parser_block_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    void       *target;
    njs_int_t  ret;

    ret = njs_parser_scope_begin(parser, NJS_SCOPE_BLOCK);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    target = (void *) (uintptr_t) parser->line;
    parser->node = NULL;

    if (token->type == NJS_TOKEN_CLOSE_BRACE) {
        parser->target = target;

        njs_parser_next(parser, njs_parser_block_statement_close_brace);
        return NJS_OK;
    }

    njs_parser_next(parser, njs_parser_statement_list);

    return njs_parser_after(parser, current, target, 0,
                            njs_parser_block_statement_close_brace);
}


static njs_int_t
njs_parser_block_statement_open_brace(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_OPEN_BRACE) {
        return njs_parser_failed(parser);
    }

    parser->line = token->line;

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    return njs_parser_block_statement(parser, token, current);
}


static njs_int_t
njs_parser_block_statement_close_brace(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t  *node;

    if (parser->ret != NJS_OK) {
        return njs_parser_failed(parser);
    }

    if (token->type != NJS_TOKEN_CLOSE_BRACE) {
        return njs_parser_failed(parser);
    }

    node = njs_parser_node_new(parser, NJS_TOKEN_BLOCK);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = (uint32_t) (uintptr_t) parser->target;
    node->left = parser->node;
    node->right = NULL;

    parser->target = NULL;
    parser->node = node;

    njs_parser_scope_end(parser);

    njs_lexer_consume_token(parser->lexer, 1);
    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_statement_list(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_statement_list_item);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_statement_list_next);
}


static njs_int_t
njs_parser_statement_list_next(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (parser->ret != NJS_OK) {
        if (token->type != NJS_TOKEN_CLOSE_BRACE) {
            parser->node = parser->target;
            return njs_parser_stack_pop(parser);
        }

        return njs_parser_failed(parser);
    }

    if (token->type == NJS_TOKEN_CLOSE_BRACE) {
        return njs_parser_stack_pop(parser);
    }

    njs_parser_next(parser, njs_parser_statement_list_item);

    return njs_parser_after(parser, current, parser->node, 0,
                            njs_parser_statement_list_next);
}


static njs_int_t
njs_parser_statement_list_item(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t  ret;

    ret = njs_parser_declaration(parser, token, current);
    if (ret == NJS_OK) {
        return NJS_OK;
    }

    njs_parser_next(parser, njs_parser_statement);

    return NJS_OK;
}


/*
 * 13.3.2 Variable Statement
 */
static njs_int_t
njs_parser_variable_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_variable_declaration_list);

    return njs_parser_after(parser, current, NULL, 1, njs_parser_semicolon);
}


static njs_int_t
njs_parser_variable_declaration_list(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_next(parser, njs_parser_variable_declaration);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_variable_declaration_list_next);
}


static njs_int_t
njs_parser_variable_declaration_list_next(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t  *node;

    if (parser->target != NULL) {
        parser->node->left = parser->target;
    }

    if (token->type != NJS_TOKEN_COMMA) {
        return njs_parser_stack_pop(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    node = parser->node;

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_variable_declaration);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_variable_declaration_list_next);
}


static njs_int_t
njs_parser_variable_declaration(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t          ret;
    njs_parser_node_t  *name;

    ret = njs_parser_binding_pattern(parser, token, current);
    if (ret == NJS_OK) {
        return njs_parser_after(parser, current, NULL, 1,
                                njs_parser_initializer);
    }

    if (!njs_lexer_token_is_binding_identifier(token)) {
        return njs_parser_failed(parser);
    }

    if (njs_parser_restricted_identifier(token->type)) {
        njs_parser_syntax_error(parser, "Identifier \"%V\" is forbidden in"
                                " var declaration", &token->text);
        return NJS_DONE;
    }

    name = njs_parser_variable_node(parser, token->unique_id, NJS_VARIABLE_VAR);
    if (name == NULL) {
        return NJS_ERROR;
    }

    name->token_line = token->line;

    parser->node = name;

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    ret = njs_parser_initializer_assign(parser, NJS_TOKEN_VAR);
    if (ret != NJS_OK) {
        return ret;
    }

    parser->node->token_line = token->line;

    if (token->type == NJS_TOKEN_ASSIGNMENT) {
        njs_parser_next(parser, njs_parser_initializer);

        return NJS_OK;
    }

    parser->target = parser->node;
    parser->node = NULL;

    njs_parser_next(parser, njs_parser_initializer_after);

    return NJS_OK;
}


/*
 * 13.3.3 Destructuring Binding Patterns.
 */
static njs_int_t
njs_parser_binding_pattern(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (token->type == NJS_TOKEN_OPEN_BRACE) {
        njs_parser_next(parser, njs_parser_object_binding_pattern);

    } else if (token->type == NJS_TOKEN_OPEN_BRACKET) {
        njs_parser_next(parser, njs_parser_array_binding_pattern);

    } else {
        return NJS_DECLINED;
    }

    njs_lexer_consume_token(parser->lexer, 1);

    return NJS_OK;
}

static njs_int_t
njs_parser_object_binding_pattern(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    return njs_parser_not_supported(parser, token);
}


static njs_int_t
njs_parser_array_binding_pattern(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    return njs_parser_not_supported(parser, token);
}


/*
 * 13.5 Expression Statement.
 */
static njs_int_t
njs_parser_expression_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_lexer_token_t  *next;

    switch (token->type) {
    case NJS_TOKEN_FUNCTION:
        njs_parser_syntax_error(parser, "Functions can only be declared "
                                        "at top level or inside a block");
        return NJS_DONE;

    case NJS_TOKEN_CLASS:
        njs_parser_syntax_error(parser, "Class can only be declared "
                                        "at top level or inside a block");
        return NJS_DONE;

    case NJS_TOKEN_OPEN_BRACE:
        return njs_parser_reject(parser);

    case NJS_TOKEN_ASYNC:
        next = njs_lexer_peek_token(parser->lexer, token, 1);
        if (next == NULL) {
            return NJS_ERROR;
        }

        if (next->type == NJS_TOKEN_FUNCTION) {
            return njs_parser_not_supported(parser, token);
        }

        break;

    case NJS_TOKEN_LET:
        token = njs_lexer_peek_token(parser->lexer, token, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        if (token->type == NJS_TOKEN_OPEN_BRACKET) {
            return njs_parser_failed(parser);
        }

        break;

    default:
        break;
    }

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_expression);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_expression_statement_after);
}


static njs_int_t
njs_parser_expression_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (njs_parser_expect_semicolon(parser, token) != NJS_OK) {
        return njs_parser_failed(parser);
    }

    return njs_parser_stack_pop(parser);
}


/*
 * 13.6 if Statement.
 */
static njs_int_t
njs_parser_if_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t          ret;
    njs_parser_node_t  *node;

    if (token->type != NJS_TOKEN_OPEN_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    node = njs_parser_node_new(parser, NJS_TOKEN_IF);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = parser->line;

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_expression);

    ret = njs_parser_after(parser, current, node, 1,
                           njs_parser_if_close_parenthesis);
    if (ret != NJS_OK) {
        return ret;
    }

    ret = njs_parser_after(parser, current, NULL, 1,
                           njs_parser_statement_wo_node);
    if (ret != NJS_OK) {
        return ret;
    }

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_else_statement);
}


static njs_int_t
njs_parser_if_close_parenthesis(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_CLOSE_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    parser->target->left = parser->node;
    parser->node = NULL;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_else_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *node;

    parser->target->right = parser->node;
    parser->node = NULL;

    if (token->type == NJS_TOKEN_ELSE) {
        node = njs_parser_node_new(parser, NJS_TOKEN_BRANCHING);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->token_line = token->line;
        node->left = parser->target->right;

        parser->target->right = node;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_statement_wo_node);

        return njs_parser_after(parser, current, parser->target, 1,
                                njs_parser_else_statement_after);
    }

    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_else_statement_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    parser->target->right->right = parser->node;

    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_iteration_statement_do(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t  *node;

    node = njs_parser_node_new(parser, NJS_TOKEN_DO);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = parser->line;

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_statement_wo_node);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_iteration_statement_do_while);
}


static njs_int_t
njs_parser_iteration_statement_do_while(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_WHILE) {
        return njs_parser_failed(parser);
    }

    parser->target->left = parser->node;

    njs_lexer_consume_token(parser->lexer, 1);

    njs_parser_next(parser, njs_parser_expression_parenthesis);

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_do_while_semicolon);
}


static njs_int_t
njs_parser_do_while_semicolon(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (parser->strict_semicolon) {
        return njs_parser_failed(parser);
    }

    parser->target->right = parser->node;
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_iteration_statement_while(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t  *node;

    node = njs_parser_node_new(parser, NJS_TOKEN_WHILE);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;

    njs_parser_next(parser, njs_parser_expression_parenthesis);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_while_statement);
}


static njs_int_t
njs_parser_while_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    parser->target->right = parser->node;
    parser->node = NULL;

    njs_parser_next(parser, njs_parser_statement_wo_node);

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_while_after);
}


static njs_int_t
njs_parser_while_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    parser->target->left = parser->node;
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_iteration_statement_for(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (token->type == NJS_TOKEN_OPEN_PARENTHESIS) {
        njs_lexer_consume_token(parser->lexer, 1);

        njs_parser_next(parser, njs_parser_iteration_statement_for_map);

        return njs_parser_after(parser, current,
                                (void *) (uintptr_t) parser->line, 1,
                                njs_parser_set_line_state);
    }

    if (token->type == NJS_TOKEN_AWAIT) {
        return njs_parser_not_supported(parser, token);
    }

    return njs_parser_failed(parser);
}


static njs_int_t
njs_parser_iteration_statement_for_map(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t  ret;
    njs_str_t  *text;

    /*
     * "var" <VariableDeclarationList> ";" <Expression>? ";" <Expression>? ")"
     *     <Statement>
     * "var" <ForBinding> "in" <Expression> ")" <Statement>
     * "var" <ForBinding> "of" <AssignmentExpression> ")" <Statement>

     * <ForDeclaration> "in" <Expression> ")" <Statement>
     * <ForDeclaration> "of" <AssignmentExpression> ")" <Statement>

     * <Expression>? ";" <Expression>? ";" <Expression>? ")" <Statement>
     * <LexicalDeclaration> <Expression>? ";" <Expression>? ")" <Statement>

     * <LeftHandSideExpression> "in" <Expression> ")" <Statement>
     * <LeftHandSideExpression> "of" <AssignmentExpression> ")" <Statement>
     */

    parser->node = NULL;

    switch (token->type) {
    case NJS_TOKEN_SEMICOLON:
        token = njs_lexer_peek_token(parser->lexer, token, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        if (token->type != NJS_TOKEN_SEMICOLON) {
            njs_lexer_consume_token(parser->lexer, 1);

            parser->target = NULL;

            njs_parser_next(parser, njs_parser_expression);

            return njs_parser_after(parser, current, NULL, 1,
                                    njs_parser_for_expression);
        }

        parser->node = NULL;
        parser->target = NULL;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_for_expression);

        return NJS_OK;

    case NJS_TOKEN_VAR:
        token = njs_lexer_peek_token(parser->lexer, token, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        njs_lexer_consume_token(parser->lexer, 1);

        ret = njs_parser_for_var_binding_or_var_list(parser, token, current);
        if (ret != NJS_OK) {
            if (ret == NJS_DONE) {
                return NJS_OK;
            }

            return ret;
        }

        break;

    case NJS_TOKEN_LET:
    case NJS_TOKEN_CONST:
        return njs_parser_not_supported(parser, token);

    default:
        njs_parser_next(parser, njs_parser_expression);
        break;
    }

    /*
     * Here we pass not a node, but a token, this is important.
     * This is necessary for correct error output.
     */

    text = njs_mp_alloc(parser->vm->mem_pool, sizeof(njs_str_t));
    if (text == NULL) {
        return NJS_ERROR;
    }

    *text = token->text;

    return njs_parser_after(parser, current, text, 1,
                            njs_parser_for_var_in_of_expression);
}


static njs_int_t
njs_parser_for_var_binding_or_var_list(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t          ret;
    njs_lexer_token_t  *next;
    njs_parser_node_t  *node, *var;

    switch (token->type) {
    /* BindingPattern */
    case NJS_TOKEN_OPEN_BRACE:
        njs_parser_next(parser, njs_parser_object_binding_pattern);
        return NJS_DONE;

    case NJS_TOKEN_OPEN_BRACKET:
        njs_parser_next(parser, njs_parser_array_binding_pattern);
        return NJS_DONE;

    default:
        if (njs_lexer_token_is_binding_identifier(token)) {
            if (njs_parser_restricted_identifier(token->type)) {
                njs_parser_syntax_error(parser, "Identifier \"%V\" is forbidden"
                                        " in var declaration", &token->text);
                return NJS_DONE;
            }

            next = njs_lexer_peek_token(parser->lexer, token, 0);
            if (next == NULL) {
                return NJS_ERROR;
            }

            if (next->type != NJS_TOKEN_IN) {
                njs_parser_next(parser, njs_parser_variable_declaration_list);
                return NJS_OK;
            }

            var = njs_parser_variable_node(parser, token->unique_id,
                                            NJS_VARIABLE_VAR);
            if (var == NULL) {
                return NJS_ERROR;
            }

            var->token_line = token->line;

            parser->node = NULL;

            node = njs_parser_node_new(parser, NJS_TOKEN_IN);
            if (node == NULL) {
                return NJS_ERROR;
            }

            node->token_line = next->line;
            node->left = var;

            njs_parser_next(parser, njs_parser_expression);

            ret = njs_parser_after(parser, current, node, 1,
                                   njs_parser_for_var_in_statement);
            if (ret != NJS_OK) {
                return NJS_ERROR;
            }

            njs_lexer_consume_token(parser->lexer, 2);

            return NJS_DONE;
        }

        parser->node = NULL;

        njs_parser_next(parser, njs_parser_expression);
        return NJS_OK;
    }
}


static njs_int_t
njs_parser_for_var_in_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_CLOSE_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    parser->target->right = parser->node;
    parser->node = NULL;

    njs_parser_next(parser, njs_parser_statement_wo_node);

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_for_var_in_statement_after);
}


static njs_int_t
njs_parser_for_var_in_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t  *foreach;

    foreach = njs_parser_node_new(parser, NJS_TOKEN_FOR_IN);
    if (foreach == NULL) {
        return NJS_ERROR;
    }

    foreach->left = parser->target;
    foreach->right = parser->node;

    parser->node = foreach;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_for_var_in_of_expression(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_str_t          *text;
    njs_parser_node_t  *node;

    /*
     * ";" <Expression>? ";" <Expression>? ")" <Statement>
     * "in" <Expression> ")" <Statement>
     * "of" <AssignmentExpression> ")" <Statement>
     */

    if (parser->node->token_type == NJS_TOKEN_IN) {
        node = parser->node->left;

        if (node->token_type != NJS_TOKEN_NAME) {
            text = (njs_str_t *) parser->target;

            njs_parser_ref_error(parser, "Invalid left-hand side \"%V\" "
                                 "in for-in statement", text);

            njs_mp_free(parser->vm->mem_pool, text);

            return NJS_DONE;
        }

        njs_parser_next(parser, njs_parser_for_in_statement);
        return NJS_OK;
    }

    if (parser->target != NULL) {
        text = (njs_str_t *) parser->target;

        njs_mp_free(parser->vm->mem_pool, text);
    }

    switch (token->type) {
    case NJS_TOKEN_SEMICOLON:
        token = njs_lexer_peek_token(parser->lexer, token, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        node = parser->node;
        parser->node = NULL;

        if (token->type != NJS_TOKEN_SEMICOLON) {
            njs_lexer_consume_token(parser->lexer, 1);

            njs_parser_next(parser, njs_parser_expression);

            return njs_parser_after(parser, current, node, 1,
                                    njs_parser_for_expression);
        }

        parser->target = node;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_for_expression);

        return NJS_OK;

    case NJS_TOKEN_OF:
        return njs_parser_not_supported(parser, token);

    default:
        return njs_parser_failed(parser);
    }
}


static njs_int_t
njs_parser_for_in_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *node, *forin;

    if (token->type != NJS_TOKEN_CLOSE_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    node = parser->node;

    if (node->right != NULL && node->right->token_type == NJS_TOKEN_VAR) {
        return NJS_ERROR;
    }

    forin = njs_parser_node_new(parser, NJS_TOKEN_FOR_IN);
    if (forin == NULL) {
        return NJS_ERROR;
    }

    forin->left = parser->node;

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_statement_wo_node);

    return njs_parser_after(parser, current, forin, 1,
                            njs_parser_for_in_statement_after);
}


static njs_int_t
njs_parser_for_in_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    parser->target->right = parser->node;
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_for_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *body, *cond, *for_node;

    if (token->type != NJS_TOKEN_SEMICOLON) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    for_node = njs_parser_node_new(parser, NJS_TOKEN_FOR);
    if (for_node == NULL) {
        return NJS_ERROR;
    }

    cond = njs_parser_node_new(parser, 0);
    if (cond == NULL) {
        return NJS_ERROR;
    }

    body = njs_parser_node_new(parser, 0);
    if (body == NULL) {
        return NJS_ERROR;
    }

    for_node->left = parser->target;
    for_node->right = cond;

    cond->left = parser->node;
    cond->right = body;

    parser->node = NULL;

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type == NJS_TOKEN_CLOSE_PARENTHESIS) {
        parser->target = for_node;

        njs_parser_next(parser, njs_parser_for_expression_end);

        return NJS_OK;
    }

    njs_parser_next(parser, njs_parser_expression);

    return njs_parser_after(parser, current, for_node, 1,
                            njs_parser_for_expression_end);
}


static njs_int_t
njs_parser_for_expression_end(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *body;

    if (token->type != NJS_TOKEN_CLOSE_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    body = parser->target->right->right;

    body->right = parser->node;

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_statement_wo_node);

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_for_end);
}


static njs_int_t
njs_parser_for_end(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *body;

    body = parser->target->right->right;

    body->left = parser->node;
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


/*
 * 13.8 continue Statement.
 */
static njs_int_t
njs_parser_continue_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    return njs_parser_break_continue(parser, token, NJS_TOKEN_CONTINUE);
}


/*
 * 13.9 break Statement.
 */
static njs_int_t
njs_parser_break_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    return njs_parser_break_continue(parser, token, NJS_TOKEN_BREAK);
}


static njs_int_t
njs_parser_break_continue(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_token_type_t type)
{
    njs_int_t  ret;

    parser->node = njs_parser_node_new(parser, type);
    if (parser->node == NULL) {
        return NJS_ERROR;
    }

    parser->node->token_line = parser->line;

    switch (token->type) {
    case NJS_TOKEN_SEMICOLON:
        break;

    case NJS_TOKEN_LINE_END:
        return njs_parser_failed(parser);

    default:
        if (njs_lexer_token_is_label_identifier(token)) {
            if (parser->lexer->prev_type == NJS_TOKEN_LINE_END) {
                return njs_parser_stack_pop(parser);
            }

            if (njs_label_find(parser->vm, parser->scope,
                               token->unique_id) == NULL)
            {
                njs_parser_syntax_error(parser, "Undefined label \"%V\"",
                                        &token->text);
                return NJS_DONE;
            }

            ret = njs_name_copy(parser->vm, &parser->node->name, &token->text);
            if (ret != NJS_OK) {
                return NJS_ERROR;
            }

            break;
        }

        if (parser->strict_semicolon
            || (token->type != NJS_TOKEN_END
                && token->type != NJS_TOKEN_CLOSE_BRACE
                && parser->lexer->prev_type != NJS_TOKEN_LINE_END))
        {
            return njs_parser_failed(parser);
        }

        return njs_parser_stack_pop(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    return njs_parser_stack_pop(parser);
}


/*
 * 13.10 return Statement.
 */
static njs_int_t
njs_parser_return_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t   *node;
    njs_parser_scope_t  *scope;

    for (scope = parser->scope;
         scope != NULL;
         scope = scope->parent)
    {
        if (scope->type == NJS_SCOPE_FUNCTION && !scope->module) {
            break;
        }

        if (scope->type == NJS_SCOPE_GLOBAL) {
            njs_parser_syntax_error(parser, "Illegal return statement");
            return NJS_ERROR;
        }
    }

    node = njs_parser_node_new(parser, NJS_TOKEN_RETURN);
    if (njs_slow_path(node == NULL)) {
        return NJS_ERROR;
    }

    node->token_line = parser->line;

    switch (token->type) {
    case NJS_TOKEN_SEMICOLON:
        njs_lexer_consume_token(parser->lexer, 1);
        break;

    case NJS_TOKEN_LINE_END:
        return njs_parser_failed(parser);

    default:
        if (!parser->strict_semicolon
            && parser->lexer->prev_type == NJS_TOKEN_LINE_END)
        {
            break;
        }

        parser->node = NULL;

        njs_parser_next(parser, njs_parser_expression);

        return njs_parser_after(parser, current, node, 0,
                                njs_parser_return_statement_after);
    }

    parser->node = node;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_return_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (parser->ret != NJS_OK) {
        parser->node = parser->target;
        return njs_parser_stack_pop(parser);
    }

    if (njs_parser_expect_semicolon(parser, token) != NJS_OK) {
        return njs_parser_failed(parser);
    }

    parser->target->right = parser->node;
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


/*
 * 13.11 with Statement
 */
static njs_int_t
njs_parser_with_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    return njs_parser_not_supported(parser, token);
}


/*
 * 13.12 switch Statement
 */
static njs_int_t
njs_parser_switch_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t          ret;
    njs_parser_node_t  *swtch;

    swtch = njs_parser_node_new(parser, NJS_TOKEN_SWITCH);
    if (swtch == NULL) {
        return NJS_ERROR;
    }

    swtch->token_line = parser->line;

    njs_parser_next(parser, njs_parser_expression_parenthesis);

    ret = njs_parser_after(parser, current, swtch, 1,
                           njs_parser_switch_block);
    if (ret != NJS_OK) {
        return ret;
    }

    return njs_parser_after(parser, current, swtch, 1,
                            njs_parser_switch_statement_after);
}


static njs_int_t
njs_parser_switch_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_switch_block(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_OPEN_BRACE) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    parser->target->left = parser->node;

    njs_parser_next(parser, njs_parser_switch_case);

    return NJS_OK;
}


static njs_int_t
njs_parser_switch_case(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    return njs_parser_switch_case_def(parser, token, current, 1);
}


static njs_int_t
njs_parser_switch_case_wo_def(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    return njs_parser_switch_case_def(parser, token, current, 0);
}


static njs_int_t
njs_parser_switch_case_def(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current, njs_bool_t with_default)
{
    njs_parser_node_t  *node, *branch;

    node = njs_parser_node_new(parser, 0);
    if (node == NULL) {
        return NJS_ERROR;
    }

    parser->node = NULL;

    switch (token->type) {
    case NJS_TOKEN_CASE:
        branch = njs_parser_node_new(parser, 0);
        if (branch == NULL) {
            return NJS_ERROR;
        }

        branch->token_line = token->line;
        branch->right = node;

        njs_parser_next(parser, njs_parser_expression);

        njs_lexer_consume_token(parser->lexer, 1);

        if (parser->target->token_type == NJS_TOKEN_SWITCH) {
            parser->target->right = branch;

        } else {
            parser->target->left = branch;
        }

        if (with_default) {
            return njs_parser_after(parser, current, branch, 1,
                                    njs_parser_switch_case_after);

        } else {
            return njs_parser_after(parser, current, branch, 1,
                                    njs_parser_switch_case_after_wo_def);
        }

    case NJS_TOKEN_DEFAULT:
        if (!with_default) {
            njs_parser_syntax_error(parser, "More than one default clause "
                                            "in switch statement");
            return NJS_DONE;
        }

        if (parser->target->token_type == NJS_TOKEN_SWITCH) {
            parser->target->right = node;

        } else {
            parser->target->left = node;
        }

        node->token_line = token->line;
        node->token_type = NJS_TOKEN_DEFAULT;

        parser->target = node;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_switch_case_after_wo_def);

        break;

    case NJS_TOKEN_CLOSE_BRACE:
        njs_lexer_consume_token(parser->lexer, 1);
        return njs_parser_stack_pop(parser);

    default:
        return njs_parser_failed(parser);
    }

    return NJS_OK;
}


static njs_int_t
njs_parser_switch_case_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_COLON) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    parser->target->right->left = parser->node;
    parser->node = NULL;

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    switch (token->type) {
    case NJS_TOKEN_CASE:
    case NJS_TOKEN_DEFAULT:
    case NJS_TOKEN_CLOSE_BRACE:
        njs_parser_next(parser, njs_parser_switch_case_block);
        return NJS_OK;

    default:
        njs_parser_next(parser, njs_parser_statement_list);
        break;
    }

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_switch_case_block);
}


static njs_int_t
njs_parser_switch_case_block(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    parser->target->right->right = parser->node;

    njs_parser_next(parser, njs_parser_switch_case);

    return NJS_OK;
}


static njs_int_t
njs_parser_switch_case_after_wo_def(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_COLON) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    if (parser->target->right != NULL) {
        parser->target->right->left = parser->node;
    }

    parser->node = NULL;

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    switch (token->type) {
    case NJS_TOKEN_CASE:
    case NJS_TOKEN_DEFAULT:
    case NJS_TOKEN_CLOSE_BRACE:
        njs_parser_next(parser, njs_parser_switch_case_block_wo_def);
        return NJS_OK;

    default:
        njs_parser_next(parser, njs_parser_statement_list);
        break;
    }

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_switch_case_block_wo_def);
}


static njs_int_t
njs_parser_switch_case_block_wo_def(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (parser->target->right != NULL) {
        parser->target->right->right = parser->node;

    } else {
        parser->target->right = parser->node;
    }

    njs_parser_next(parser, njs_parser_switch_case_wo_def);

    return NJS_OK;
}


/*
 * 13.13 Labelled Statements
 */
static njs_int_t
njs_parser_labelled_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    uintptr_t          unique_id;
    njs_variable_t     *label;

    unique_id = token->unique_id;

    label = njs_label_find(parser->vm, parser->scope, unique_id);
    if (label != NULL) {
        njs_parser_syntax_error(parser, "Label \"%V\" "
                                "has already been declared", &token->text);
        return NJS_DONE;
    }

    label = njs_label_add(parser->vm, parser->scope, unique_id);
    if (label == NULL) {
        return NJS_ERROR;
    }

    njs_lexer_consume_token(parser->lexer, 2);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    parser->node = NULL;

    if (token->type == NJS_TOKEN_FUNCTION) {
        njs_syntax_error(parser->vm, "In strict mode code, functions can only "
                                "be declared at top level or inside a block.");
        return NJS_DONE;

    } else {
        njs_parser_next(parser, njs_parser_statement_wo_node);
    }

    return njs_parser_after(parser, current, (void *) unique_id, 1,
                            njs_parser_labelled_statement_after);
}


static njs_int_t
njs_parser_labelled_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t                ret;
    uintptr_t                unique_id;
    const njs_lexer_entry_t  *entry;

    if (parser->node != NULL) {
        /* The statement is not empty block or just semicolon. */

        unique_id = (uintptr_t) parser->target;
        entry = (const njs_lexer_entry_t *) unique_id;

        ret = njs_name_copy(parser->vm, &parser->node->name, &entry->name);
        if (ret != NJS_OK) {
            return NJS_ERROR;
        }

        ret = njs_label_remove(parser->vm, parser->scope, unique_id);
        if (ret != NJS_OK) {
            return NJS_ERROR;
        }
    }

    return njs_parser_stack_pop(parser);
}


/*
 * 13.14 throw Statement
 */
static njs_int_t
njs_parser_throw_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *node;

    node = njs_parser_node_new(parser, NJS_TOKEN_THROW);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = parser->line;

    if (parser->lexer->prev_type == NJS_TOKEN_LINE_END) {
        njs_parser_syntax_error(parser, "Illegal newline after throw");
        return NJS_DONE;
    }

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_expression);

    return njs_parser_after(parser, current, node, 1,
                            njs_parser_throw_statement_after);
}


static njs_int_t
njs_parser_throw_statement_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (parser->ret != NJS_OK) {
        parser->node = parser->target;
        return njs_parser_reject(parser);
    }

    if (njs_parser_expect_semicolon(parser, token) != NJS_OK) {
        return njs_parser_failed(parser);
    }

    parser->target->right = parser->node;
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


/*
 * 13.15 try Statement.
 */
static njs_int_t
njs_parser_try_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *try;

    try = njs_parser_node_new(parser, NJS_TOKEN_TRY);
    if (try == NULL) {
        return NJS_ERROR;
    }

    try->token_line = parser->line;

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_block_statement_open_brace);

    return njs_parser_after(parser, current, try, 1,
                            njs_parser_catch_or_finally);
}


static njs_int_t
njs_parser_catch_or_finally(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t          ret;
    njs_parser_node_t  *catch, *node, *try;

    try = parser->target;

    try->left = parser->node;

    if (token->type == NJS_TOKEN_FINALLY) {
        node = njs_parser_node_new(parser, NJS_TOKEN_FINALLY);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->token_line = token->line;

        if (try->right != NULL) {
            node->left = try->right;
        }

        try->right = node;
        parser->node = NULL;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_block_statement_open_brace);

        return njs_parser_after(parser, current, try, 0,
                                njs_parser_catch_finally);

    } else if (token->type != NJS_TOKEN_CATCH) {
        njs_parser_syntax_error(parser, "Missing catch or finally after try");
        return NJS_DONE;
    }

    catch = njs_parser_node_new(parser, NJS_TOKEN_CATCH);
    if (catch == NULL) {
        return NJS_ERROR;
    }

    catch->token_line = token->line;

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    ret = njs_parser_scope_begin(parser, NJS_SCOPE_BLOCK);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    if (token->type != NJS_TOKEN_OPEN_PARENTHESIS) {
        parser->node = NULL;

        njs_parser_next(parser, njs_parser_block_statement_open_brace);

        try->right = catch;

        /* TODO: it is necessary to change the generator. */

        return njs_parser_not_supported(parser, token);

        /*
         *  return njs_parser_after(parser, current, parser->target, 0,
         *                          njs_parser_catch_after);
         */
    }

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    try->right = catch;

    if (njs_lexer_token_is_binding_identifier(token)) {
        node = njs_parser_variable_node(parser, token->unique_id,
                                        NJS_VARIABLE_CATCH);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->token_line = token->line;

        catch->left = node;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_catch_parenthesis);
        return NJS_OK;
    }

    if (token->type != NJS_TOKEN_OPEN_BRACE) {
        return njs_parser_failed(parser);
    }

    /*
     * njs_parser_next(parser, njs_parser_block_statement_open_brace);
     *
     * return njs_parser_after(parser, current, try, 1,
     *                       njs_parser_catch_parenthesis);
     */

    return njs_parser_not_supported(parser, token);
}


static njs_int_t
njs_parser_catch_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *node;

    njs_parser_scope_end(parser);

    parser->target->right->right = parser->node;

    if (token->type == NJS_TOKEN_FINALLY) {
        node = njs_parser_node_new(parser, NJS_TOKEN_FINALLY);
        if (node == NULL) {
            return NJS_ERROR;
        }

        node->token_line = token->line;

        if (parser->target->right != NULL) {
            node->left = parser->target->right;
        }

        parser->target->right = node;
        parser->node = NULL;

        njs_lexer_consume_token(parser->lexer, 1);
        njs_parser_next(parser, njs_parser_block_statement_open_brace);

        return njs_parser_after(parser, current, parser->target, 1,
                                njs_parser_catch_finally);
    }

    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_catch_parenthesis(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_CLOSE_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    parser->target->right->right = parser->node;
    parser->node = NULL;

    njs_parser_next(parser, njs_parser_block_statement_open_brace);

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_catch_after);
}


static njs_int_t
njs_parser_catch_finally(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (parser->ret != NJS_OK) {
        return njs_parser_failed(parser);
    }

    parser->target->right->right = parser->node;
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_debugger_statement(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    return njs_parser_not_supported(parser, token);
}


/*
 * 14.1 Function Definitions.
 */
static njs_function_t *
njs_parser_function_alloc(njs_parser_t *parser, njs_variable_t *var)
{
    njs_value_t            *value;
    njs_function_t         *function;
    njs_function_lambda_t  *lambda;

    lambda = njs_function_lambda_alloc(parser->vm, 1);
    if (lambda == NULL) {
        njs_memory_error(parser->vm);
        return NULL;
    }

    /* TODO:
     *  njs_function_t is used to pass lambda to
     *  njs_generate_function_declaration() and is not actually needed.
     *  real njs_function_t is created by njs_vmcode_function() in runtime.
     */

    function = njs_function_alloc(parser->vm, lambda, NULL, 1);
    if (function == NULL) {
        return NULL;
    }

    njs_set_function(&var->value, function);

    if (var->index != NJS_INDEX_NONE
        && njs_scope_accumulative(parser->vm, parser->scope))
    {
        value = (njs_value_t *) var->index;
        *value = var->value;
    }

    return function;
}


static njs_int_t
njs_parser_function_declaration(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t          ret;
    uintptr_t          unique_id;
    njs_variable_t     *var;
    njs_function_t     *function;
    njs_parser_node_t  *node, *temp;

    if (!njs_lexer_token_is_binding_identifier(token)) {
        return njs_parser_failed(parser);
    }

    if (njs_parser_restricted_identifier(token->type)) {
        njs_parser_syntax_error(parser, "Identifier \"%V\" is forbidden"
                                " in function declaration", &token->text);
        return NJS_DONE;
    }

    node = parser->node;
    unique_id = token->unique_id;

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type != NJS_TOKEN_OPEN_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    var = njs_variable_add(parser->vm, parser->scope, unique_id,
                           NJS_VARIABLE_FUNCTION);
    if (var == NULL) {
        return NJS_ERROR;
    }

    ret = njs_variable_reference(parser->vm, parser->scope, node,
                                 unique_id, NJS_DECLARATION);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    function = njs_parser_function_alloc(parser, var);
    if (function == NULL) {
        return NJS_ERROR;
    }

    temp = njs_parser_node_new(parser, 0);
    if (temp == NULL) {
        return NJS_ERROR;
    }

    temp->left = node;
    temp->u.value.data.u.lambda = function->u.lambda;

    node->left = (njs_parser_node_t *) function;

    parser->node = temp;

    njs_parser_next(parser, njs_parser_function_parse);

    return njs_parser_after(parser, current, temp, 1,
                            njs_parser_function_declaration_after);
}


static njs_int_t
njs_parser_function_declaration_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_function_t  *function;

    parser->node = parser->target->left;

    function = (njs_function_t *) parser->node->left;

    function->args_count = function->u.lambda->nargs
                           - function->u.lambda->rest_parameters;

    parser->node->right = parser->target->right;
    parser->node->left = NULL;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_function_parse(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t  ret;

    ret = njs_parser_scope_begin(parser, NJS_SCOPE_FUNCTION);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    parser->target = parser->node;
    parser->node = NULL;

    njs_parser_next(parser, njs_parser_formal_parameters);

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_function_lambda_args_after);
}


static njs_int_t
njs_parser_function_expression(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t              ret;
    njs_variable_t         *var;
    njs_function_t         *function;
    njs_function_lambda_t  *lambda;

    ret = njs_parser_scope_begin(parser, NJS_SCOPE_SHIM);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    if (njs_lexer_token_is_binding_identifier(token)) {
        var = njs_variable_add(parser->vm, parser->scope, token->unique_id,
                               NJS_VARIABLE_SHIM);
        if (var == NULL) {
            return NJS_ERROR;
        }

        njs_lexer_consume_token(parser->lexer, 1);

        token = njs_lexer_token(parser->lexer, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        function = njs_parser_function_alloc(parser, var);
        if (function == NULL) {
            return NJS_ERROR;
        }

        lambda = function->u.lambda;

    } else {
        /* Anonymous function. */
        lambda = njs_function_lambda_alloc(parser->vm, 1);
        if (lambda == NULL) {
            return NJS_ERROR;
        }
    }

    if (token->type != NJS_TOKEN_OPEN_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    parser->node->u.value.data.u.lambda = lambda;

    njs_parser_next(parser, njs_parser_function_parse);

    return njs_parser_after(parser, current, NULL, 1,
                            njs_parser_function_expression_after);
}


static njs_int_t
njs_parser_function_expression_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_scope_end(parser);

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_unique_formal_parameters(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    parser->node = NULL;

    njs_parser_next(parser, njs_parser_formal_parameters);

    return NJS_OK;
}


static njs_int_t
njs_parser_formal_parameters(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_variable_t         *arg, *cur_arg;
    njs_function_lambda_t  *lambda;

    lambda = parser->target->u.value.data.u.lambda;

    switch (token->type) {
    /* BindingRestElement */
    case NJS_TOKEN_ELLIPSIS:
        if (lambda->rest_parameters != 0) {
            return njs_parser_failed(parser);
        }

        njs_lexer_consume_token(parser->lexer, 1);

        lambda->rest_parameters = 1;

        return NJS_OK;

    /* BindingElement */
    case NJS_TOKEN_OPEN_BRACE:
        return njs_parser_not_supported(parser, token);

    case NJS_TOKEN_OPEN_BRACKET:
        return njs_parser_not_supported(parser, token);

    default:
        /* SingleNameBinding */
        if (njs_lexer_token_is_binding_identifier(token)) {
            arg = njs_variable_add(parser->vm, parser->scope,
                                   token->unique_id, NJS_VARIABLE_VAR);
            if (arg == NULL) {
                return NJS_ERROR;
            }

            if (arg->index > 0) {
                njs_parser_syntax_error(parser, "Duplicate parameter names");
                return NJS_DONE;
            }

            cur_arg = (njs_variable_t *) parser->node;

            if (cur_arg == NULL) {
                arg->index = NJS_SCOPE_ARGUMENTS;

                /* A "this" reservation. */
                arg->index += sizeof(njs_value_t);

            } else {
                arg->index = cur_arg->index + sizeof(njs_value_t);
            }

            lambda->nargs++;

            /* Crutch for temporary storage. */
            parser->node = (njs_parser_node_t *) arg;

            njs_lexer_consume_token(parser->lexer, 1);

            njs_parser_next(parser, njs_parser_formal_parameters_after);
            break;
        }

        return njs_parser_stack_pop(parser);
    }

    return NJS_OK;
}


static njs_int_t
njs_parser_formal_parameters_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_function_lambda_t  *lambda;

    if (token->type != NJS_TOKEN_COMMA) {
        return njs_parser_stack_pop(parser);
    }

    lambda = parser->target->u.value.data.u.lambda;

    if (lambda->rest_parameters) {
        njs_parser_syntax_error(parser, "Rest parameter must be "
                                        "last formal parameter");
        return NJS_DONE;
    }

    njs_lexer_consume_token(parser->lexer, 1);

    njs_parser_next(parser, njs_parser_formal_parameters);

    return NJS_OK;
}


/*
 * 14.2 Arrow Function Definitions
 *
 * TODO: implement according to specification.
 */
static njs_int_t
njs_parser_arrow_function(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t              ret;
    njs_variable_t         *arg;
    njs_parser_node_t      *node;
    njs_function_lambda_t  *lambda;

    node = njs_parser_node_new(parser, NJS_TOKEN_FUNCTION_EXPRESSION);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    parser->node = node;

    lambda = njs_function_lambda_alloc(parser->vm, 0);
    if (lambda == NULL) {
        return NJS_ERROR;
    }

    node->u.value.data.u.lambda = lambda;

    ret = njs_parser_scope_begin(parser, NJS_SCOPE_FUNCTION);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    parser->scope->arrow_function = 1;

    if (token->type == NJS_TOKEN_OPEN_PARENTHESIS) {
        njs_lexer_consume_token(parser->lexer, 1);

        parser->node = NULL;
        parser->target = node;

        njs_parser_next(parser, njs_parser_formal_parameters);

        return njs_parser_after(parser, current, node, 1,
                                njs_parser_arrow_function_args_after);

    } else if (njs_lexer_token_is_binding_identifier(token)) {
        arg = njs_variable_add(parser->vm, parser->scope,
                               token->unique_id, NJS_VARIABLE_VAR);
        if (arg == NULL) {
            return NJS_ERROR;
        }

        arg->index = NJS_SCOPE_ARGUMENTS;

        /* A "this" reservation. */
        arg->index += sizeof(njs_value_t);

        lambda->nargs = 1;

        njs_lexer_consume_token(parser->lexer, 1);

        parser->target = node;

        njs_parser_next(parser, njs_parser_arrow_function_arrow);

    } else {
        return njs_parser_failed(parser);
    }

    return NJS_OK;
}


static njs_int_t
njs_parser_arrow_function_args_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_CLOSE_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    njs_parser_next(parser, njs_parser_arrow_function_arrow);

    return NJS_OK;
}


static njs_int_t
njs_parser_arrow_function_arrow(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_ARROW) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type == NJS_TOKEN_OPEN_BRACE) {
        njs_lexer_consume_token(parser->lexer, 1);

        token = njs_lexer_token(parser->lexer, 0);
        if (token == NULL) {
            return NJS_ERROR;
        }

        if (token->type == NJS_TOKEN_CLOSE_BRACE) {
            parser->node = NULL;

            njs_parser_next(parser, njs_parser_function_lambda_body_after);

            return NJS_OK;
        }

        parser->node = NULL;

        njs_parser_next(parser, njs_parser_statement_list);

        return njs_parser_after(parser, current, parser->target, 1,
                                njs_parser_function_lambda_body_after);
    }

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_assignment_expression);

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_arrow_function_body_after);
}


static njs_int_t
njs_parser_arrow_function_body_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t  *body;

    body = njs_parser_return_set(parser, parser->node);
    if (body == NULL) {
        return NJS_ERROR;
    }

    parser->target->right = body;
    parser->node = parser->target;

    njs_parser_scope_end(parser);

    return njs_parser_stack_pop(parser);
}


/*
 * 14.3 Method Definitions.
 */
static njs_int_t
njs_parser_method_definition(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_lexer_token_t  *next;
    njs_parser_node_t  *expr;

    switch (token->type) {
    /* PropertyName */
    case NJS_TOKEN_STRING:
    case NJS_TOKEN_ESCAPE_STRING:
    case NJS_TOKEN_NUMBER:
        break;

    default:
        if (njs_lexer_token_is_identifier_name(token)) {
            break;
        }

        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    next = njs_lexer_token(parser->lexer, 0);
    if (next == NULL) {
        return NJS_ERROR;
    }

    if (next->type != NJS_TOKEN_OPEN_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    expr = njs_parser_node_new(parser, NJS_TOKEN_FUNCTION_EXPRESSION);
    if (expr == NULL) {
        return NJS_ERROR;
    }

    expr->token_line = next->line;

    parser->node = expr;

    njs_lexer_consume_token(parser->lexer, 1);
    njs_parser_next(parser, njs_parser_function_lambda);

    return NJS_OK;
}


static njs_int_t
njs_parser_get_set(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_token_type_t   accessor;
    njs_lexer_token_t  *name;
    njs_parser_node_t  *property, *expression, *temp;

    temp = parser->target;
    accessor = (njs_token_type_t) (uintptr_t) temp->right;

    name = token;

    token = njs_lexer_peek_token(parser->lexer, token, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    switch (token->type) {
    /* IdentifierReference */
    case NJS_TOKEN_NAME:
    case NJS_TOKEN_STRING:
    case NJS_TOKEN_ESCAPE_STRING:
    case NJS_TOKEN_NUMBER:
        break;

    case NJS_TOKEN_OPEN_BRACKET:
        njs_lexer_consume_token(parser->lexer, 2);

        njs_parser_next(parser, njs_parser_assignment_expression);

        return njs_parser_after(parser, current, temp, 1,
                                njs_parser_get_set_after);

    default:
        if (njs_lexer_token_is_identifier_name(token)) {
            break;
        }

        return njs_parser_property_definition_ident(parser, name, temp);
    }

    property = njs_parser_property_name_node(parser, token);
    if (property == NULL) {
        return NJS_ERROR;
    }

    njs_lexer_consume_token(parser->lexer, 2);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type != NJS_TOKEN_OPEN_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    expression = njs_parser_node_new(parser, NJS_TOKEN_FUNCTION_EXPRESSION);
    if (expression == NULL) {
        return NJS_ERROR;
    }

    expression->token_line = token->line;

    temp->right = property;

    parser->node = expression;

    njs_lexer_consume_token(parser->lexer, 1);
    njs_parser_next(parser, njs_parser_function_lambda);

    if (accessor == NJS_TOKEN_PROPERTY_GETTER) {
        return njs_parser_after(parser, current, temp, 1, njs_parser_get_after);
    }

    return njs_parser_after(parser, current, temp, 1, njs_parser_set_after);
}


static njs_int_t
njs_parser_get_set_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_token_type_t   accessor;
    njs_parser_node_t  *expression, *temp;

    if (token->type != NJS_TOKEN_CLOSE_BRACKET) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type != NJS_TOKEN_OPEN_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    expression = njs_parser_node_new(parser, NJS_TOKEN_FUNCTION_EXPRESSION);
    if (expression == NULL) {
        return NJS_ERROR;
    }

    expression->token_line = token->line;

    temp = parser->target;

    accessor = (njs_token_type_t) (uintptr_t) temp->right;
    temp->right = parser->node;

    parser->node = expression;

    njs_lexer_consume_token(parser->lexer, 1);
    njs_parser_next(parser, njs_parser_function_lambda);

    if (accessor == NJS_TOKEN_PROPERTY_GETTER) {
        return njs_parser_after(parser, current, temp, 1, njs_parser_get_after);
    }

    return njs_parser_after(parser, current, temp, 1, njs_parser_set_after);
}


static njs_int_t
njs_parser_get_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t              ret;
    njs_parser_node_t      *expression, *temp;
    njs_function_lambda_t  *lambda;

    temp = parser->target;

    expression = parser->node;
    lambda = expression->u.value.data.u.lambda;

    if (lambda->nargs != 0) {
        njs_parser_syntax_error(parser,
                                "Getter must not have any formal parameters");
        return NJS_DONE;
    }

    ret = njs_parser_property_accessor(parser, temp->left, temp->right,
                                       expression, NJS_TOKEN_PROPERTY_GETTER);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    parser->node = temp->left;
    parser->target = NULL;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_set_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t              ret;
    njs_parser_node_t      *expression, *temp;
    njs_function_lambda_t  *lambda;

    temp = parser->target;

    expression = parser->node;
    lambda = expression->u.value.data.u.lambda;

    if (lambda->nargs != 1) {
        njs_parser_syntax_error(parser,
                               "Setter must have exactly one formal parameter");
        return NJS_DONE;
    }

    ret = njs_parser_property_accessor(parser, temp->left, temp->right,
                                       expression, NJS_TOKEN_PROPERTY_SETTER);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    parser->node = temp->left;
    parser->target = NULL;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_function_lambda(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_int_t              ret;
    njs_parser_node_t      *expr;
    njs_function_lambda_t  *lambda;

    lambda = njs_function_lambda_alloc(parser->vm, 0);
    if (lambda == NULL) {
        return NJS_ERROR;
    }

    expr = parser->node;
    expr->u.value.data.u.lambda = lambda;

    ret = njs_parser_scope_begin(parser, NJS_SCOPE_FUNCTION);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    parser->node = NULL;
    parser->target = expr;

    njs_parser_next(parser, njs_parser_unique_formal_parameters);

    return njs_parser_after(parser, current, expr, 1,
                            njs_parser_function_lambda_args_after);
}


static njs_int_t
njs_parser_function_lambda_args_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    if (token->type != NJS_TOKEN_CLOSE_PARENTHESIS) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type != NJS_TOKEN_OPEN_BRACE) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type == NJS_TOKEN_CLOSE_BRACE) {
        parser->node = NULL;

        njs_parser_next(parser, njs_parser_function_lambda_body_after);

        return NJS_OK;
    }

    parser->node = NULL;

    njs_parser_next(parser, njs_parser_statement_list);

    return njs_parser_after(parser, current, parser->target, 1,
                            njs_parser_function_lambda_body_after);
}


static njs_int_t
njs_parser_function_lambda_body_after(njs_parser_t *parser,
    njs_lexer_token_t *token, njs_queue_link_t *current)
{
    njs_parser_node_t *body, *last, *parent;

    if (token->type != NJS_TOKEN_CLOSE_BRACE) {
        return njs_parser_failed(parser);
    }

    parent = parser->target;

    last = NULL;
    body = njs_parser_chain_top(parser);

    if (body != NULL) {
        /* Take the last function body statement. */
        last = body->right;

        if (last == NULL) {
            /*
             * The last statement is terminated by semicolon.
             * Take the last statement itself.
             */
            last = body->left;
        }
    }

    if (last == NULL || last->token_type != NJS_TOKEN_RETURN) {
        /*
         * There is no function body or the last function body
         * body statement is not "return" statement.
         */
        body = njs_parser_return_set(parser, NULL);
        if (body == NULL) {
            return NJS_ERROR;
        }

        body->right->token_line = token->line;
    }

    parent->right = body;
    parser->node = parent;

    njs_parser_scope_end(parser);

    njs_lexer_consume_token(parser->lexer, 1);

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_export(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *node;

    if (!parser->scope->module) {
        njs_parser_syntax_error(parser, "Illegal export statement");
        return NJS_DONE;
    }

    if (token->type != NJS_TOKEN_DEFAULT) {
        njs_parser_syntax_error(parser, "Non-default export is not supported");
        return NJS_DONE;
    }

    njs_lexer_consume_token(parser->lexer, 1);

    node = njs_parser_node_new(parser, NJS_TOKEN_EXPORT);
    if (node == NULL) {
        return NJS_ERROR;
    }

    node->token_line = parser->line;
    parser->node = node;

    njs_parser_next(parser, njs_parser_expression);

    return njs_parser_after(parser, current, node, 1, njs_parser_export_after);
}


static njs_int_t
njs_parser_export_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    parser->target->right = parser->node;
    parser->node = parser->target;

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_import(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_parser_node_t  *name, *import;

    if (parser->scope->type != NJS_SCOPE_GLOBAL && !parser->scope->module) {
        njs_parser_syntax_error(parser, "Illegal import statement");
        return NJS_DONE;
    }

    if (token->type != NJS_TOKEN_NAME) {
        njs_parser_syntax_error(parser, "Non-default import is not supported");
        return NJS_DONE;
    }

    name = njs_parser_variable_node(parser, token->unique_id, NJS_VARIABLE_VAR);
    if (name == NULL) {
        return NJS_ERROR;
    }

    name->token_line = token->line;

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type != NJS_TOKEN_FROM) {
        return njs_parser_failed(parser);
    }

    njs_lexer_consume_token(parser->lexer, 1);

    token = njs_lexer_token(parser->lexer, 0);
    if (token == NULL) {
        return NJS_ERROR;
    }

    if (token->type != NJS_TOKEN_STRING) {
        return njs_parser_failed(parser);
    }

    import = njs_parser_node_new(parser, NJS_TOKEN_IMPORT);
    if (import == NULL) {
        return NJS_ERROR;
    }

    import->token_line = parser->line;
    import->left = name;

    njs_parser_next(parser, njs_parser_module);

    return njs_parser_after(parser, current, import, 1,
                            njs_parser_import_after);
}


static njs_int_t
njs_parser_import_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    if (njs_parser_expect_semicolon(parser, token) != NJS_OK) {
        return njs_parser_failed(parser);
    }

    parser->target->right = parser->node;

    parser->node = parser->target;
    parser->node->hoist = 1;

    return njs_parser_stack_pop(parser);
}


njs_int_t
njs_parser_module_lambda(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t              ret;
    njs_parser_node_t      *node, *parent;
    njs_function_lambda_t  *lambda;

    node = njs_parser_node_new(parser, NJS_TOKEN_FUNCTION_EXPRESSION);
    if (node == NULL) {
        return NJS_ERROR;
    }

    lambda = njs_function_lambda_alloc(parser->vm, 1);
    if (lambda == NULL) {
        return NJS_ERROR;
    }

    node->token_line = token->line;
    node->u.value.data.u.lambda = lambda;

    parser->node = node;

    ret = njs_parser_scope_begin(parser, NJS_SCOPE_FUNCTION);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    parser->scope->module = 1;

    parent = parser->node;
    parser->node = NULL;

    njs_parser_next(parser, njs_parser_statement_list);

    return njs_parser_after(parser, current, parent, 0,
                            njs_parser_module_lambda_after);
}


static njs_int_t
njs_parser_module_lambda_after(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_queue_link_t *current)
{
    njs_int_t  ret;

    ret = njs_parser_export_sink(parser);
    if (ret != NJS_OK) {
        return NJS_ERROR;
    }

    parser->target->right = njs_parser_chain_top(parser);

    parser->node = parser->target;

    njs_parser_scope_end(parser);

    return njs_parser_stack_pop(parser);
}


static njs_int_t
njs_parser_export_sink(njs_parser_t *parser)
{
    njs_uint_t         n;
    njs_parser_node_t  *node, *prev;

    n = 0;

    for (node = njs_parser_chain_top(parser);
         node != NULL;
         node = node->left)
    {
        if (node->right != NULL
            && node->right->token_type == NJS_TOKEN_EXPORT)
        {
            n++;
        }
    }

    if (n != 1) {
        njs_parser_syntax_error(parser,
             (n == 0) ? "export statement is required"
                      : "Identifier \"default\" has already been declared");
        return NJS_ERROR;
    }

    node = njs_parser_chain_top(parser);

    if (node->right && node->right->token_type == NJS_TOKEN_EXPORT) {
        return NJS_OK;
    }

    prev = njs_parser_chain_top(parser);

    while (prev->left != NULL) {
        node = prev->left;

        if (node->right != NULL
            && node->right->token_type == NJS_TOKEN_EXPORT)
        {
            prev->left = node->left;
            break;
        }

        prev = prev->left;
    }

    node->left = njs_parser_chain_top(parser);
    njs_parser_chain_top_set(parser, node);

    return NJS_OK;
}


static njs_parser_node_t *
njs_parser_return_set(njs_parser_t *parser, njs_parser_node_t *expr)
{
    njs_parser_node_t  *stmt, *node;

    node = njs_parser_node_new(parser, NJS_TOKEN_RETURN);
    if (njs_slow_path(node == NULL)) {
        return NULL;
    }

    if (expr != NULL) {
        node->token_line = expr->token_line;
    }

    node->right = expr;

    stmt = njs_parser_node_new(parser, NJS_TOKEN_STATEMENT);
    if (njs_slow_path(stmt == NULL)) {
        return NULL;
    }

    stmt->left = njs_parser_chain_top(parser);
    stmt->right = node;

    njs_parser_chain_top_set(parser, stmt);

    return stmt;
}


static njs_parser_node_t *
njs_parser_variable_node(njs_parser_t *parser, uintptr_t unique_id,
    njs_variable_type_t type)
{
    njs_int_t          ret;
    njs_variable_t     *var;
    njs_parser_node_t  *node;

    var = njs_variable_add(parser->vm, parser->scope, unique_id, type);
    if (njs_slow_path(var == NULL)) {
        return NULL;
    }

    if (njs_is_null(&var->value)) {

        switch (type) {

        case NJS_VARIABLE_VAR:
            njs_set_undefined(&var->value);
            break;

        default:
            break;
        }
    }

    node = njs_parser_node_new(parser, NJS_TOKEN_NAME);
    if (njs_slow_path(node == NULL)) {
        return NULL;
    }

    ret = njs_variable_reference(parser->vm, parser->scope, node, unique_id,
                                 NJS_DECLARATION);
    if (njs_slow_path(ret != NJS_OK)) {
        return NULL;
    }

    return node;
}


static njs_parser_node_t *
njs_parser_reference(njs_parser_t *parser, njs_lexer_token_t *token)
{
    njs_int_t           ret;
    njs_variable_t      *var;
    njs_parser_node_t   *node;
    njs_parser_scope_t  *scope;

    node = njs_parser_node_new(parser, token->type);
    if (njs_slow_path(node == NULL)) {
        return NULL;
    }

    switch (token->type) {

    case NJS_TOKEN_NULL:
        njs_thread_log_debug("JS: null");

        node->u.value = njs_value_null;
        break;

    case NJS_TOKEN_THIS:
        njs_thread_log_debug("JS: this");

        scope = njs_function_scope(parser->scope, 0);

        if (scope != NULL) {
            if (scope == njs_function_scope(parser->scope, 1)) {
                node->index = NJS_INDEX_THIS;

            } else {
                node->token_type = NJS_TOKEN_NON_LOCAL_THIS;
                node->token_line = token->line;

                ret = njs_variable_reference(parser->vm, scope, node,
                                             token->unique_id, NJS_REFERENCE);
                if (njs_slow_path(ret != NJS_OK)) {
                    return NULL;
                }

                var = njs_variable_add(parser->vm, scope, token->unique_id,
                                       NJS_VARIABLE_VAR);
                if (njs_slow_path(var == NULL)) {
                    return NULL;
                }

                var->this_object = 1;
            }

            break;
        }

        node->token_type = NJS_TOKEN_GLOBAL_OBJECT;

        break;

    case NJS_TOKEN_ARGUMENTS:
        njs_thread_log_debug("JS: arguments");

        scope = njs_function_scope(parser->scope, 0);

        if (scope == NULL) {
            njs_parser_syntax_error(parser, "\"%V\" object in global scope",
                                    &token->text);
            return NULL;
        }

        node->token_line = token->line;

        ret = njs_variable_reference(parser->vm, scope, node, token->unique_id,
                                     NJS_REFERENCE);
        if (njs_slow_path(ret != NJS_OK)) {
            return NULL;
        }

        var = njs_variable_add(parser->vm, scope, token->unique_id,
                               NJS_VARIABLE_VAR);
        if (njs_slow_path(var == NULL)) {
            return NULL;
        }

        var->arguments_object = 1;

        break;

    default:
        if (token->type == NJS_TOKEN_EVAL
            || njs_lexer_token_is_identifier_reference(token))
        {
            njs_thread_log_debug("JS: %V", name);

            if (token->type != NJS_TOKEN_EVAL) {
                node->token_type = NJS_TOKEN_NAME;
            }

            node->token_line = token->line;

            ret = njs_variable_reference(parser->vm, parser->scope, node,
                                         token->unique_id, NJS_REFERENCE);
            if (njs_slow_path(ret != NJS_OK)) {
                return NULL;
            }

            break;
        }

        (void) njs_parser_unexpected_token(parser->vm, parser, &token->text,
                                           token->type);
        return NULL;
    }

    return node;
}


static njs_parser_node_t *
njs_parser_argument(njs_parser_t *parser, njs_parser_node_t *expr,
    njs_index_t index)
{
    njs_parser_node_t  *node;

    node = njs_parser_node_new(parser, NJS_TOKEN_ARGUMENT);
    if (njs_slow_path(node == NULL)) {
        return NULL;
    }

    node->token_line = expr->token_line;
    node->index = index;

    node->left = expr;
    expr->dest = node;

    return node;
}


static njs_int_t
njs_parser_object_property(njs_parser_t *parser, njs_parser_node_t *parent,
    njs_parser_node_t *property, njs_parser_node_t *value,
    njs_bool_t proto_init)
{
    njs_token_type_t   type;
    njs_parser_node_t  *stmt, *assign, *object, *propref;

    object = njs_parser_node_new(parser, NJS_TOKEN_OBJECT_VALUE);
    if (njs_slow_path(object == NULL)) {
        return NJS_TOKEN_ERROR;
    }

    object->token_line = value->token_line;
    object->u.object = parent;

    type = proto_init ? NJS_TOKEN_PROTO_INIT : NJS_TOKEN_PROPERTY_INIT;

    propref = njs_parser_node_new(parser, type);
    if (njs_slow_path(propref == NULL)) {
        return NJS_ERROR;
    }

    propref->token_line = value->token_line;
    propref->left = object;
    propref->right = property;

    assign = njs_parser_node_new(parser, NJS_TOKEN_ASSIGNMENT);
    if (njs_slow_path(assign == NULL)) {
        return NJS_ERROR;
    }

    assign->token_line = value->token_line;
    assign->u.operation = NJS_VMCODE_MOVE;
    assign->left = propref;
    assign->right = value;

    stmt = njs_parser_node_new(parser, NJS_TOKEN_STATEMENT);
    if (njs_slow_path(stmt == NULL)) {
        return NJS_ERROR;
    }

    stmt->right = assign;
    stmt->left = parent->left;
    parent->left = stmt;

    return NJS_OK;
}


static njs_int_t
njs_parser_property_accessor(njs_parser_t *parser, njs_parser_node_t *parent,
    njs_parser_node_t *property, njs_parser_node_t *value,
    njs_token_type_t accessor)
{
    njs_parser_node_t  *node, *stmt, *object, *propref;

    object = njs_parser_node_new(parser, NJS_TOKEN_OBJECT_VALUE);
    if (njs_slow_path(object == NULL)) {
        return NJS_TOKEN_ERROR;
    }

    object->token_line = value->token_line;
    object->u.object = parent;

    propref = njs_parser_node_new(parser, 0);
    if (njs_slow_path(propref == NULL)) {
        return NJS_ERROR;
    }

    propref->left = object;
    propref->right = property;

    node = njs_parser_node_new(parser, accessor);
    if (njs_slow_path(node == NULL)) {
        return NJS_ERROR;
    }

    node->token_line = value->token_line;
    node->left = propref;
    node->right = value;

    stmt = njs_parser_node_new(parser, NJS_TOKEN_STATEMENT);
    if (njs_slow_path(stmt == NULL)) {
        return NJS_ERROR;
    }

    stmt->right = node;
    stmt->left = parent->left;
    parent->left = stmt;

    return NJS_OK;
}


static njs_int_t
njs_parser_array_item(njs_parser_t *parser, njs_parser_node_t *array,
    njs_parser_node_t *value)
{
    njs_int_t          ret;
    njs_parser_node_t  *number;

    number = njs_parser_node_new(parser, NJS_TOKEN_NUMBER);
    if (njs_slow_path(number == NULL)) {
        return NJS_ERROR;
    }

    njs_set_number(&number->u.value, array->u.length);

    number->token_line = value->token_line;

    ret = njs_parser_object_property(parser, array, number, value, 0);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    array->ctor = 0;
    array->u.length++;

    return NJS_OK;
}


static njs_int_t
njs_parser_template_string(njs_parser_t *parser, njs_lexer_token_t *token)
{
    u_char             *p, c;
    njs_int_t          ret;
    njs_str_t          *text;
    njs_bool_t         escape;
    njs_lexer_t        *lexer;
    njs_parser_node_t  *node;

    lexer = parser->lexer;
    text = &token->text;

    escape = 0;
    p = text->start;

    if (p == NULL) {
        return NJS_ERROR;
    }

    while (p < lexer->end) {

        c = *p++;

        switch (c) {
        case '\\':
            if (p == lexer->end) {
                return NJS_ERROR;
            }

            p++;
            escape = 1;

            continue;

        case '`':
            text->length = p - text->start - 1;
            goto done;

        case '$':
            if (p < lexer->end && *p == '{') {
                p++;
                text->length = p - text->start - 2;
                goto done;
            }

            break;

        case '\n':
            parser->lexer->line++;
            break;
        }
    }

    return NJS_ERROR;

done:

    node = njs_parser_node_new(parser, NJS_TOKEN_STRING);
    if (njs_slow_path(node == NULL)) {
        return NJS_ERROR;
    }

    node->token_line = token->line;

    if (escape) {
        ret = njs_parser_escape_string_create(parser, token, &node->u.value);
        if (njs_slow_path(ret != NJS_TOKEN_STRING)) {
            return NJS_ERROR;
        }

    } else {
        ret = njs_parser_string_create(parser->vm, token, &node->u.value);
        if (njs_slow_path(ret != NJS_OK)) {
            return NJS_ERROR;
        }
    }

    lexer->start = p;
    parser->node = node;

    return c == '`' ? NJS_DONE : NJS_OK;
}


njs_int_t
njs_parser_string_create(njs_vm_t *vm, njs_lexer_token_t *token,
    njs_value_t *value)
{
    u_char                *dst;
    ssize_t               size, length;
    uint32_t              cp;
    njs_str_t             *src;
    const u_char          *p, *end;
    njs_unicode_decode_t  ctx;

    src = &token->text;

    length = njs_utf8_safe_length(src->start, src->length, &size);

    dst = njs_string_alloc(vm, value, size, length);
    if (njs_slow_path(dst == NULL)) {
        return NJS_ERROR;
    }

    p = src->start;
    end = src->start + src->length;

    njs_utf8_decode_init(&ctx);

    while (p < end) {
        cp = njs_utf8_decode(&ctx, &p, end);

        if (cp <= NJS_UNICODE_MAX_CODEPOINT) {
            dst = njs_utf8_encode(dst, cp);

        } else {
            dst = njs_utf8_encode(dst, NJS_UNICODE_REPLACEMENT);
        }
    }

    if (length > NJS_STRING_MAP_STRIDE && size != length) {
        njs_string_offset_map_init(value->long_string.data->start, size);
    }

    return NJS_OK;
}


static njs_token_type_t
njs_parser_escape_string_create(njs_parser_t *parser, njs_lexer_token_t *token,
    njs_value_t *value)
{
    u_char                c, *start, *dst;
    size_t                size, length, hex_length;
    uint64_t              cp, cp_pair;
    njs_int_t             ret;
    njs_str_t             *string;
    const u_char          *src, *end, *hex_end;
    njs_unicode_decode_t  ctx;

    ret = njs_parser_escape_string_calc_length(parser, token, &size, &length);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_TOKEN_ILLEGAL;
    }

    start = njs_string_alloc(parser->vm, value, size, length);
    if (njs_slow_path(start == NULL)) {
        return NJS_TOKEN_ERROR;
    }

    dst = start;
    cp_pair = 0;

    string = &token->text;
    src = string->start;
    end = src + string->length;

    while (src < end) {
        c = *src++;

        if (c == '\\') {
            /*
             * Testing "src == end" is not required here
             * since this has been already tested by lexer.
             */

            c = *src++;

            switch (c) {
            case 'u':
                /*
                 * A character after "u" can be safely tested here
                 * because there is always a closing quote at the
                 * end of string: ...\u".
                 */

                if (*src != '{') {
                    hex_length = 4;
                    goto hex_length;
                }

                src++;
                hex_length = 0;
                hex_end = end;

                goto hex;

            case 'x':
                hex_length = 2;
                goto hex_length;

            case '0':
                c = '\0';
                break;

            case 'b':
                c = '\b';
                break;

            case 'f':
                c = '\f';
                break;

            case 'n':
                c = '\n';
                break;

            case 'r':
                c = '\r';
                break;

            case 't':
                c = '\t';
                break;

            case 'v':
                c = '\v';
                break;

            case '\r':
                /*
                 * A character after "\r" can be safely tested here
                 * because there is always a closing quote at the
                 * end of string: ...\\r".
                 */

                if (*src == '\n') {
                    src++;
                }

                continue;

            case '\n':
                continue;

            default:
                if (c >= 0x80) {
                    goto utf8_copy;
                }

                break;
            }
        }

        if (c < 0x80) {
            *dst++ = c;

            continue;
        }

    utf8_copy:

        src--;

        njs_utf8_decode_init(&ctx);

        cp = njs_utf8_decode(&ctx, &src, end);
        if (cp > NJS_UNICODE_MAX_CODEPOINT) {
            cp = NJS_UNICODE_REPLACEMENT;
        }

        dst = njs_utf8_encode(dst, cp);

        continue;

    hex_length:

        hex_end = src + hex_length;

    hex:
        cp = njs_number_hex_parse(&src, hex_end);

        /* Skip '}' character. */

        if (hex_length == 0) {
            src++;
        }

        if (cp_pair != 0) {
            if (njs_fast_path(njs_surrogate_trailing(cp))) {
                cp = njs_surrogate_pair(cp_pair, cp);

            } else if (njs_slow_path(njs_surrogate_leading(cp))) {
                cp = NJS_UNICODE_REPLACEMENT;

                dst = njs_utf8_encode(dst, (uint32_t) cp);

            } else {
                dst = njs_utf8_encode(dst, NJS_UNICODE_REPLACEMENT);
            }

            cp_pair = 0;

        } else if (njs_surrogate_any(cp)) {
            if (cp <= 0xdbff && src[0] == '\\' && src[1] == 'u') {
                cp_pair = cp;
                continue;
            }

            cp = NJS_UNICODE_REPLACEMENT;
        }

        dst = njs_utf8_encode(dst, (uint32_t) cp);
        if (njs_slow_path(dst == NULL)) {
            njs_parser_syntax_error(parser, "Invalid Unicode code point \"%V\"",
                                    &token->text);

            return NJS_TOKEN_ILLEGAL;
        }
    }

    if (length > NJS_STRING_MAP_STRIDE && length != size) {
        njs_string_offset_map_init(start, size);
    }

    return NJS_TOKEN_STRING;
}


static njs_int_t
njs_parser_escape_string_calc_length(njs_parser_t *parser,
    njs_lexer_token_t *token, size_t *out_size, size_t *out_length)
{
    size_t                size, length, hex_length;
    uint64_t              cp, cp_pair;
    njs_str_t             *string;
    const u_char          *ptr, *src, *end, *hex_end;
    njs_unicode_decode_t  ctx;

    size = 0;
    length = 0;
    cp_pair = 0;

    string = &token->text;
    src = string->start;
    end = src + string->length;

    while (src < end) {

        if (*src == '\\') {
            src++;

            switch (*src) {
            case 'u':
                src++;

                if (*src != '{') {
                    hex_length = 4;
                    goto hex_length;
                }

                src++;
                hex_length = 0;
                hex_end = end;

                goto hex;

            case 'x':
                src++;
                hex_length = 2;
                goto hex_length;

            case '\r':
                src++;

                if (*src == '\n') {
                    src++;
                }

                continue;

            case '\n':
                src++;
                continue;

            default:
                break;
            }
        }

        if (*src >= 0x80) {
            njs_utf8_decode_init(&ctx);

            cp = njs_utf8_decode(&ctx, &src, end);
            if (cp > NJS_UNICODE_MAX_CODEPOINT) {
                cp = NJS_UNICODE_REPLACEMENT;
            }

            size += njs_utf8_size(cp);
            length++;

            continue;
        }

        src++;
        size++;
        length++;

        continue;

    hex_length:

        hex_end = src + hex_length;

        if (njs_slow_path(hex_end > end)) {
            goto invalid;
        }

    hex:

        ptr = src;
        cp = njs_number_hex_parse(&src, hex_end);

        if (hex_length != 0) {
            if (src != hex_end) {
                goto invalid;
            }

        } else {
            if (src == ptr || (src - ptr) > 6) {
                goto invalid;
            }

            if (src == end || *src++ != '}') {
                goto invalid;
            }
        }

        if (cp_pair != 0) {
            if (njs_fast_path(njs_surrogate_trailing(cp))) {
                cp = njs_surrogate_pair(cp_pair, cp);

            } else if (njs_slow_path(njs_surrogate_leading(cp))) {
                cp = NJS_UNICODE_REPLACEMENT;

                size += njs_utf8_size(cp);
                length++;

            } else {
                size += njs_utf8_size(NJS_UNICODE_REPLACEMENT);
                length++;
            }

            cp_pair = 0;

        } else if (njs_surrogate_any(cp)) {
            if (cp <= 0xdbff && src[0] == '\\' && src[1] == 'u') {
                cp_pair = cp;
                continue;
            }

            cp = NJS_UNICODE_REPLACEMENT;
        }

        size += njs_utf8_size(cp);
        length++;
    }

    *out_size = size;
    *out_length = length;

    return NJS_OK;

invalid:

    njs_parser_syntax_error(parser, "Invalid Unicode code point \"%V\"",
                            &token->text);
    return NJS_ERROR;
}


njs_bool_t
njs_parser_has_side_effect(njs_parser_node_t *node)
{
    njs_bool_t  side_effect;

    if (node == NULL) {
        return 0;
    }

    if (node->token_type >= NJS_TOKEN_ASSIGNMENT
        && node->token_type <= NJS_TOKEN_LAST_ASSIGNMENT)
    {
        return 1;
    }

    if (node->token_type == NJS_TOKEN_FUNCTION_CALL
        || node->token_type == NJS_TOKEN_METHOD_CALL)
    {
        return 1;
    }

    side_effect = njs_parser_has_side_effect(node->left);

    if (njs_fast_path(!side_effect)) {
        return njs_parser_has_side_effect(node->right);
    }

    return side_effect;
}


njs_token_type_t
njs_parser_unexpected_token(njs_vm_t *vm, njs_parser_t *parser,
    njs_str_t *name, njs_token_type_t type)
{
    if (type != NJS_TOKEN_END) {
        njs_parser_syntax_error(parser, "Unexpected token \"%V\"", name);

    } else {
        njs_parser_syntax_error(parser, "Unexpected end of input");
    }

    return NJS_DONE;
}


u_char *
njs_parser_trace_handler(njs_trace_t *trace, njs_trace_data_t *td,
    u_char *start)
{
    u_char        *p;
    size_t        size;
    njs_vm_t      *vm;
    njs_lexer_t   *lexer;
    njs_parser_t  *parser;

    size = njs_length("InternalError: ");
    memcpy(start, "InternalError: ", size);
    p = start + size;

    vm = trace->data;

    trace = trace->next;
    p = trace->handler(trace, td, p);

    parser = vm->parser;

    if (parser != NULL && parser->lexer != NULL) {
        lexer = parser->lexer;

        if (lexer->file.length != 0) {
            njs_internal_error(vm, "%s in %V:%uD", start, &lexer->file,
                               parser->lexer->token->line);
        } else {
            njs_internal_error(vm, "%s in %uD", start,
                               parser->lexer->token->line);
        }

    } else {
        njs_internal_error(vm, "%s", start);
    }

    return p;
}


static void
njs_parser_scope_error(njs_vm_t *vm, njs_parser_scope_t *scope,
    njs_object_type_t type, uint32_t line, const char *fmt, va_list args)
{
    size_t       width;
    u_char       msg[NJS_MAX_ERROR_STR];
    u_char       *p, *end;
    njs_str_t    *file;
    njs_int_t    ret;
    njs_value_t  value;

    static const njs_value_t  file_name = njs_string("fileName");
    static const njs_value_t  line_number = njs_string("lineNumber");

    file = &scope->file;

    p = msg;
    end = msg + NJS_MAX_ERROR_STR;

    p = njs_vsprintf(p, end, fmt, args);

    width = njs_length(" in ") + file->length + NJS_INT_T_LEN;

    if (p > end - width) {
        p = end - width;
    }

    if (file->length != 0 && !vm->options.quiet) {
        p = njs_sprintf(p, end, " in %V:%uD", file, line);

    } else {
        p = njs_sprintf(p, end, " in %uD", line);
    }

    njs_error_new(vm, &vm->retval, type, msg, p - msg);

    njs_set_number(&value, line);
    njs_value_property_set(vm, &vm->retval, njs_value_arg(&line_number),
                           &value);

    if (file->length != 0) {
        ret = njs_string_set(vm, &value, file->start, file->length);
        if (ret == NJS_OK) {
            njs_value_property_set(vm, &vm->retval, njs_value_arg(&file_name),
                                   &value);
        }
    }
}


void
njs_parser_lexer_error(njs_parser_t *parser, njs_object_type_t type,
    const char *fmt, ...)
{
    va_list  args;

    if (njs_is_error(&parser->vm->retval)) {
        return;
    }

    va_start(args, fmt);
    njs_parser_scope_error(parser->vm, parser->scope, type,
                           parser->lexer->line, fmt, args);
    va_end(args);
}


void
njs_parser_node_error(njs_vm_t *vm, njs_parser_node_t *node,
    njs_object_type_t type, const char *fmt, ...)
{
    va_list  args;

    va_start(args, fmt);
    njs_parser_scope_error(vm, node->scope, type, node->token_line, fmt, args);
    va_end(args);
}


njs_int_t
njs_parser_serialize_ast(njs_parser_node_t *node, njs_chb_t *chain)
{
    njs_int_t  ret;

    ret = NJS_OK;

    njs_parser_serialize_tree(chain, node, &ret, 0);
    njs_chb_append_literal(chain, "\n");

    return ret;
}


njs_inline void
njs_parser_serialize_indent(njs_chb_t *chain, size_t indent)
{
    size_t  i;

    for (i = 0; i < indent; i++) {
        njs_chb_append_literal(chain, "  ");
    }
}


static void
njs_parser_serialize_tree(njs_chb_t *chain, njs_parser_node_t *node,
    njs_int_t *ret, size_t indent)
{
    njs_str_t  str;

    njs_chb_append_literal(chain, "{\"name\": \"");

    *ret |= njs_parser_serialize_node(chain, node);

    njs_chb_append_literal(chain, "\",\n");
    njs_parser_serialize_indent(chain, indent);
    njs_chb_sprintf(chain, 32, " \"line\": %d", node->token_line);

    switch (node->token_type) {
    case NJS_TOKEN_NUMBER:
    case NJS_TOKEN_STRING:
    case NJS_TOKEN_NAME:
    case NJS_TOKEN_FUNCTION_CALL:
        njs_chb_append_literal(chain, ",\n");
        njs_parser_serialize_indent(chain, indent);
        njs_chb_sprintf(chain, 32, " \"index\": \"%p\"", node->index);

        switch (node->token_type) {
        case NJS_TOKEN_NUMBER:
        case NJS_TOKEN_STRING:
            njs_chb_append_literal(chain, ",\n");
            njs_parser_serialize_indent(chain, indent);

            if (node->token_type == NJS_TOKEN_NUMBER) {
                njs_chb_sprintf(chain, 32, " \"value\": %f",
                                njs_number(&node->u.value));

            } else {
                njs_string_get(&node->u.value, &str);
                njs_chb_append_literal(chain, " \"value\": \"");
                njs_chb_append_str(chain, &str);
                njs_chb_append_literal(chain, "\"");
            }

            break;

        default:
            break;
        }

        break;

    default:
        break;
    }

    if (node->left != NULL) {
        njs_chb_append_literal(chain, ",\n");
        njs_parser_serialize_indent(chain, indent);
        njs_chb_append_literal(chain, " \"left\": ");

        njs_parser_serialize_tree(chain, node->left, ret, indent + 1);
    }

    if (node->right != NULL) {
        njs_chb_append_literal(chain, ",\n");
        njs_parser_serialize_indent(chain, indent);
        njs_chb_append_literal(chain, " \"right\": ");

        njs_parser_serialize_tree(chain, node->right, ret, indent + 1);
    }

    njs_chb_append_literal(chain, "}");
}


static njs_int_t
njs_parser_serialize_node(njs_chb_t *chain, njs_parser_node_t *node)
{
    const char  *name;

#define njs_token_serialize(token)                                          \
    case token:                                                             \
        name = &njs_stringify(token)[njs_length("NJS_TOKEN_")];             \
        njs_chb_append(chain, name, njs_strlen(name));                      \
        break

    switch (node->token_type) {
    njs_token_serialize(NJS_TOKEN_END);
    /* FIXME: NJS_TOKEN_ILLEGAL should not be present in AST */
    njs_token_serialize(NJS_TOKEN_ILLEGAL);
    njs_token_serialize(NJS_TOKEN_COMMA);
    njs_token_serialize(NJS_TOKEN_CONDITIONAL);
    njs_token_serialize(NJS_TOKEN_ASSIGNMENT);
    njs_token_serialize(NJS_TOKEN_ADDITION_ASSIGNMENT);
    njs_token_serialize(NJS_TOKEN_SUBSTRACTION_ASSIGNMENT);
    njs_token_serialize(NJS_TOKEN_MULTIPLICATION_ASSIGNMENT);
    njs_token_serialize(NJS_TOKEN_EXPONENTIATION_ASSIGNMENT);
    njs_token_serialize(NJS_TOKEN_DIVISION_ASSIGNMENT);
    njs_token_serialize(NJS_TOKEN_REMAINDER_ASSIGNMENT);
    njs_token_serialize(NJS_TOKEN_LEFT_SHIFT_ASSIGNMENT);
    njs_token_serialize(NJS_TOKEN_RIGHT_SHIFT_ASSIGNMENT);
    njs_token_serialize(NJS_TOKEN_UNSIGNED_RIGHT_SHIFT_ASSIGNMENT);
    njs_token_serialize(NJS_TOKEN_BITWISE_OR_ASSIGNMENT);
    njs_token_serialize(NJS_TOKEN_BITWISE_XOR_ASSIGNMENT);
    njs_token_serialize(NJS_TOKEN_BITWISE_AND_ASSIGNMENT);
    njs_token_serialize(NJS_TOKEN_EQUAL);
    njs_token_serialize(NJS_TOKEN_NOT_EQUAL);
    njs_token_serialize(NJS_TOKEN_STRICT_EQUAL);
    njs_token_serialize(NJS_TOKEN_STRICT_NOT_EQUAL);
    njs_token_serialize(NJS_TOKEN_ADDITION);
    njs_token_serialize(NJS_TOKEN_UNARY_PLUS);
    njs_token_serialize(NJS_TOKEN_INCREMENT);
    njs_token_serialize(NJS_TOKEN_POST_INCREMENT);
    njs_token_serialize(NJS_TOKEN_SUBSTRACTION);
    njs_token_serialize(NJS_TOKEN_UNARY_NEGATION);
    njs_token_serialize(NJS_TOKEN_DECREMENT);
    njs_token_serialize(NJS_TOKEN_POST_DECREMENT);
    njs_token_serialize(NJS_TOKEN_MULTIPLICATION);
    njs_token_serialize(NJS_TOKEN_EXPONENTIATION);
    njs_token_serialize(NJS_TOKEN_DIVISION);
    njs_token_serialize(NJS_TOKEN_REMAINDER);
    njs_token_serialize(NJS_TOKEN_LESS);
    njs_token_serialize(NJS_TOKEN_LESS_OR_EQUAL);
    njs_token_serialize(NJS_TOKEN_LEFT_SHIFT);
    njs_token_serialize(NJS_TOKEN_GREATER);
    njs_token_serialize(NJS_TOKEN_GREATER_OR_EQUAL);
    njs_token_serialize(NJS_TOKEN_RIGHT_SHIFT);
    njs_token_serialize(NJS_TOKEN_UNSIGNED_RIGHT_SHIFT);
    njs_token_serialize(NJS_TOKEN_BITWISE_OR);
    njs_token_serialize(NJS_TOKEN_LOGICAL_OR);
    njs_token_serialize(NJS_TOKEN_BITWISE_XOR);
    njs_token_serialize(NJS_TOKEN_BITWISE_AND);
    njs_token_serialize(NJS_TOKEN_LOGICAL_AND);
    njs_token_serialize(NJS_TOKEN_BITWISE_NOT);
    njs_token_serialize(NJS_TOKEN_LOGICAL_NOT);
    njs_token_serialize(NJS_TOKEN_COALESCE);
    njs_token_serialize(NJS_TOKEN_IN);
    njs_token_serialize(NJS_TOKEN_OF);
    njs_token_serialize(NJS_TOKEN_INSTANCEOF);
    njs_token_serialize(NJS_TOKEN_TYPEOF);
    njs_token_serialize(NJS_TOKEN_VOID);
    njs_token_serialize(NJS_TOKEN_NEW);
    njs_token_serialize(NJS_TOKEN_DELETE);
    njs_token_serialize(NJS_TOKEN_YIELD);

    njs_token_serialize(NJS_TOKEN_NULL);
    njs_token_serialize(NJS_TOKEN_NUMBER);
    njs_token_serialize(NJS_TOKEN_TRUE);
    njs_token_serialize(NJS_TOKEN_FALSE);
    njs_token_serialize(NJS_TOKEN_STRING);
    njs_token_serialize(NJS_TOKEN_TEMPLATE_LITERAL);
    njs_token_serialize(NJS_TOKEN_NAME);
    njs_token_serialize(NJS_TOKEN_OBJECT);
    njs_token_serialize(NJS_TOKEN_OBJECT_VALUE);
    njs_token_serialize(NJS_TOKEN_ARRAY);
    njs_token_serialize(NJS_TOKEN_REGEXP);

    njs_token_serialize(NJS_TOKEN_PROPERTY);
    njs_token_serialize(NJS_TOKEN_PROPERTY_INIT);
    njs_token_serialize(NJS_TOKEN_PROPERTY_DELETE);
    njs_token_serialize(NJS_TOKEN_PROPERTY_GETTER);
    njs_token_serialize(NJS_TOKEN_PROPERTY_SETTER);

    njs_token_serialize(NJS_TOKEN_PROTO_INIT);

    njs_token_serialize(NJS_TOKEN_FUNCTION);
    njs_token_serialize(NJS_TOKEN_FUNCTION_EXPRESSION);
    njs_token_serialize(NJS_TOKEN_FUNCTION_CALL);
    njs_token_serialize(NJS_TOKEN_METHOD_CALL);

    njs_token_serialize(NJS_TOKEN_ARGUMENT);
    njs_token_serialize(NJS_TOKEN_RETURN);
    njs_token_serialize(NJS_TOKEN_STATEMENT);
    njs_token_serialize(NJS_TOKEN_BLOCK);
    njs_token_serialize(NJS_TOKEN_VAR);
    njs_token_serialize(NJS_TOKEN_LET);
    njs_token_serialize(NJS_TOKEN_IF);
    njs_token_serialize(NJS_TOKEN_ELSE);
    njs_token_serialize(NJS_TOKEN_BRANCHING);
    njs_token_serialize(NJS_TOKEN_WHILE);
    njs_token_serialize(NJS_TOKEN_DO);
    njs_token_serialize(NJS_TOKEN_FOR);
    njs_token_serialize(NJS_TOKEN_FOR_IN);
    njs_token_serialize(NJS_TOKEN_BREAK);
    njs_token_serialize(NJS_TOKEN_CONTINUE);
    njs_token_serialize(NJS_TOKEN_SWITCH);
    njs_token_serialize(NJS_TOKEN_CASE);
    njs_token_serialize(NJS_TOKEN_DEFAULT);
    njs_token_serialize(NJS_TOKEN_WITH);
    njs_token_serialize(NJS_TOKEN_TRY);
    njs_token_serialize(NJS_TOKEN_CATCH);
    njs_token_serialize(NJS_TOKEN_FINALLY);
    njs_token_serialize(NJS_TOKEN_THROW);
    njs_token_serialize(NJS_TOKEN_THIS);
    njs_token_serialize(NJS_TOKEN_GLOBAL_OBJECT);
    njs_token_serialize(NJS_TOKEN_NON_LOCAL_THIS);
    njs_token_serialize(NJS_TOKEN_ARGUMENTS);
    njs_token_serialize(NJS_TOKEN_EVAL);
    njs_token_serialize(NJS_TOKEN_IMPORT);
    njs_token_serialize(NJS_TOKEN_EXPORT);

#if 0

    njs_token_serialize(NJS_TOKEN_TARGET);
    njs_token_serialize(NJS_TOKEN_META);
    njs_token_serialize(NJS_TOKEN_ASYNC);
    njs_token_serialize(NJS_TOKEN_AWAIT);
    njs_token_serialize(NJS_TOKEN_CONST);
    njs_token_serialize(NJS_TOKEN_DEBUGGER);
    njs_token_serialize(NJS_TOKEN_ENUM);

    njs_token_serialize(NJS_TOKEN_CLASS);
    njs_token_serialize(NJS_TOKEN_EXTENDS);
    njs_token_serialize(NJS_TOKEN_IMPLEMENTS);
    njs_token_serialize(NJS_TOKEN_INTERFACE);
    njs_token_serialize(NJS_TOKEN_PACKAGE);
    njs_token_serialize(NJS_TOKEN_PRIVATE);
    njs_token_serialize(NJS_TOKEN_PROTECTED);
    njs_token_serialize(NJS_TOKEN_PUBLIC);
    njs_token_serialize(NJS_TOKEN_STATIC);
    njs_token_serialize(NJS_TOKEN_SUPER);

#endif

    default:
        njs_chb_sprintf(chain, 32, "#UNDEF(%d)", (int) node->token_type);
        return NJS_DECLINED;
    }

    return NJS_OK;
}
