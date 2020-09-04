#include <stdbool.h>
#include <stdio.h>
#include <tidy.h>

/**
 * find_node_by_name searches a list for a given name.
 */
TidyNode find_node_by_name(TidyNode node, const char *name);

/**
 * find_node_by_name_prefix searches a list for a given name prefix.
 */
TidyNode find_node_by_name_prefix(TidyNode node, const char *name);

/**
 * find_node_by_id searches a list or tree for a given ID.
 */
TidyNode find_node_by_id(TidyNode node, const char *id, bool rec);

/**
 * find_attr finds a node attribute by name.
 */
TidyAttr find_attr(TidyNode node, const char *name);

/**
 * trim_tag moves both pointers to remove an HTML tag from the beginning/end.
 */
void trim_tag(const unsigned char **b, const unsigned char **e);

/**
 * print_unescaped writes the HTML string without tags.
 */
void print_unescaped(FILE *f, const unsigned char *s);
