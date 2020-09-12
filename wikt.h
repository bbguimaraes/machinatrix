#include <stdbool.h>

#include <tidy.h>

typedef struct {
    TidyNode toc, contents;
} wikt_page;

bool wikt_parse_page(TidyDoc doc, wikt_page *p);
TidyNode wikt_find_lang(TidyNode node, const char *name);
TidyNode wikt_translation_head(TidyNode node);
TidyNode wikt_translation_body(TidyNode node);
bool wikt_next_section(const char *header, const char *prefix, TidyNode *node);
bool wikt_next_subsection(const char *tag, const char *prefix, TidyNode *node);
