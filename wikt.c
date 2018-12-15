#include "wikt.h"

#include <tidybuffio.h>
#include "html.h"
#include "util.h"

static const char *TOC_ID = "toc";

bool wikt_parse_page(TidyDoc doc, wikt_page *p) {
    TidyNode node = find_node_by_id(tidyGetBody(doc), TOC_ID, true);
    if(!node) {
        log_err("table of contents (#%s) not found\n", TOC_ID);
        return false;
    }
    p->toc = node;
    if(!(node = tidyGetNext(node))) {
        log_err("contents not found\n");
        return false;
    }
    p->contents = node;
    return true;
}

TidyNode wikt_find_lang(TidyNode node, const char *name) {
    for(; node; node = tidyGetNext(node)) {
        const char *n = tidyNodeGetName(node);
        if(n && strcmp(n, name) == 0)
            return node;
    }
    return NULL;
}

static bool wikt_next(
        const char *header, const char *prefix, size_t len, TidyNode *node,
        bool sub) {
    while(*node) {
        *node = tidyGetNext(*node);
        const char *name = tidyNodeGetName(*node);
        if(!name)
            continue;
        if(strcmp(name, "h2") == 0)
            break;
        if(strcmp(name, header) != 0)
            continue;
        TidyNode cmp = sub ? *node : tidyGetChild(*node);
        TidyAttr attr = find_attr(cmp, "id");
        if(!attr)
            continue;
        const char *id = tidyAttrValue(attr);
        if(!id || strncmp(id, prefix, len))
            continue;
        return true;
    }
    return false;
}

bool wikt_next_section(
        const char *header, const char *prefix, size_t len, TidyNode *node)
    { return wikt_next(header, prefix, len, node, false); }
bool wikt_next_subsection(
        const char *tag, const char *prefix, size_t len, TidyNode *node)
    { return wikt_next(tag, prefix, len, node, true); }
