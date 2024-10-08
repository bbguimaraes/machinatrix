/**
 * \file
 * HTML utility functions.
 */
#include <stdbool.h>
#include <stdio.h>

#include "common.h"

#include <tidy.h>

/** Similar to \ref request, but parses the response as HTML. */
TidyDoc request_and_parse(const char *url, bool verbose);

/** Checks if space-separated list \p l contains the class \p c. */
bool list_has_class(const char *s, const char *cls);

/** Checks whether \p cls is one of the classes of p node. */
bool node_has_class(TidyNode node, const char *cls);

/** Searches a list for a given name. */
TidyNode find_node_by_name(TidyNode node, const char *name);

/** Searches a list for a given name prefix. */
TidyNode find_node_by_name_prefix(TidyNode node, const char *name);

/** Searches a list or tree for an element with a given lcass. */
TidyNode find_node_by_class(TidyNode node, const char *cls, bool rec);

/** Searches a list or tree for a given ID. */
TidyNode find_node_by_id(TidyNode node, const char *id, bool rec);

/** Searches a list or tree for an element containing a text (substring). */
TidyNode find_node_by_content(
    TidyDoc doc, TidyNode node, const char *s, TidyBuffer *b, bool rec);

/** Finds a node attribute by name. */
TidyAttr find_attr(TidyNode node, const char *name);

/** Moves both pointers to remove an HTML tag from the beginning/end. */
void trim_tag(const unsigned char **b, const unsigned char **e);

/** Writes the HTML string without tags. */
void print_unescaped(FILE *f, const unsigned char *s);
