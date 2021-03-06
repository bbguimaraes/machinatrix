#include "wikt.h"

#include <tidybuffio.h>

#include "html.h"
#include "utils.h"

#define TOC_ID "toc"

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

TidyNode wikt_translation_head(TidyNode n) {
    return (n = tidyGetChild(n)) && (n = tidyGetChild(n)) ? n : NULL;
}

TidyNode wikt_translation_body(TidyNode n) {
    return (n = tidyGetChild(n)) && (n = tidyGetNext(n))
            && (n = tidyGetChild(n)) && (n = find_node_by_name(n, "table"))
            && (n = tidyGetChild(n)) && (n = tidyGetChild(n))
        ? n
        : NULL;
}

static bool
next(const char *header, const char *prefix, TidyNode *n, bool sub) {
    bool ret = false;
    TidyNode node = *n;
    while((node = tidyGetNext(node))) {
        const char *name = tidyNodeGetName(node);
        if(!name)
            continue;
        if(strcmp(name, "h2") == 0)
            break;
        if(strcmp(name, header) != 0)
            continue;
        TidyNode cmp = sub ? node : tidyGetChild(node);
        TidyAttr attr = find_attr(cmp, "id");
        if(!attr)
            continue;
        const char *id = tidyAttrValue(attr);
        if(!id || !is_prefix(prefix, id))
            continue;
        ret = true;
        break;
    }
    *n = node;
    return ret;
}

bool wikt_next_section(const char *header, const char *prefix, TidyNode *node) {
    return next(header, prefix, node, false);
}

bool wikt_next_subsection(const char *tag, const char *prefix, TidyNode *node) {
    return next(tag, prefix, node, true);
}
