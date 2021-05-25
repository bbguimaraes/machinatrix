/**
 * \file
 * HTML utility functions.
 */
#include <stdbool.h>
#include <stdio.h>

#include "common.h"

#include <tidy.h>

/**
 * Searches a list for a given name.
 */
TidyNode find_node_by_name(TidyNode node, const char *name);

/**
 * Searches a list for a given name prefix.
 */
TidyNode find_node_by_name_prefix(TidyNode node, const char *name);

/**
 * Searches a list or tree for a given ID.
 */
TidyNode find_node_by_id(TidyNode node, const char *id, bool rec);

/**
 * Finds a node attribute by name.
 */
TidyAttr find_attr(TidyNode node, const char *name);

/**
 * Moves both pointers to remove an HTML tag from the beginning/end.
 */
void trim_tag(const unsigned char **b, const unsigned char **e);

/**
 * Writes the HTML string without tags.
 */
void print_unescaped(FILE *f, const unsigned char *s);
