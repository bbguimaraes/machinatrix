#include <stdbool.h>
#include <tidy.h>

TidyNode find_node_by_name(TidyNode node, const char *name);
TidyNode find_node_by_name_prefix(TidyNode node, const char *name);
TidyAttr find_attr(TidyNode node, const char *name);
TidyNode find_node_by_id(TidyNode node, const char *id, bool rec);
void trim_tag(const unsigned char **b, const unsigned char **e);
void print_unescaped(const unsigned char *s);
