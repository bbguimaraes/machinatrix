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

static bool node_has_id_prefix(TidyNode node, const char *p) {
    TidyAttr a = find_attr(node, "id");
    if(!a)
        return false;
    const char *const id = tidyAttrValue(a);
    return id && is_prefix(p, id);
}

static bool next_section(
    const char *cls, const char *prefix, TidyNode *n, bool sub)
{
    bool ret = false;
    TidyNode node = *n;
    while((node = tidyGetNext(node))) {
        TidyAttr const a = find_attr(node, "class");
        if(!a)
            continue;
        ctmbstr const v = tidyAttrValue(a);
        if(!sub && !list_has_class(v, "mw-heading"))
            continue;
        if(list_has_class(v, "mw-heading2"))
            break;
        if(cls && !list_has_class(v, cls))
            continue;
        if(!node_has_id_prefix(sub ? node : tidyGetChild(node), prefix))
            continue;
        ret = true;
        break;
    }
    *n = node;
    return ret;
}

bool wikt_next_section(const char *cls, const char *prefix, TidyNode *node) {
    return next_section(cls, prefix, node, false);
}

bool wikt_next_subsection(const char *cls, const char *prefix, TidyNode *node) {
    return next_section(cls, prefix, node, true);
}
