#include "wikt.h"

#include <strings.h>

#include <tidybuffio.h>

#include "html.h"
#include "utils.h"

#define CONTENTS_ID "mw-content-text"

bool wikt_parse_page(TidyDoc doc, wikt_page *p) {
    TidyNode node = tidyGetBody(doc);
    if(!(node = find_node_by_id(node, CONTENTS_ID, true)))
        return log_err("contents not found\n"), false;
    if(!(node = find_node_by_class(node, "mw-heading", true)))
        return log_err("no section found\n"), false;
    p->contents = node;
    return true;
}

TidyNode wikt_translation_head(TidyNode n) {
    return ((n = find_node_by_class(n, "NavHead", true))
        && (n = tidyGetChild(n))
    ) ? n : NULL;
}

TidyNode wikt_translation_body(TidyNode n) {
    return ((n = find_node_by_class(n, "translations", true))
        && (n = find_node_by_name(n, "table"))
        && (n = tidyGetChild(n))
        && (n = find_node_by_name(n, "tbody"))
    ) ? n : NULL;
}

TidyNode wikt_next_translation_block(TidyNode node, TidyNode *list) {
    for(;
        (node = find_node_by_class(node, "translations-cell", true));
        node = tidyGetNext(node)
    ) {
        TidyNode li = node;
        if((li = tidyGetChild(li)) && (li = tidyGetChild(li)))
            return *list = li, node;
    }
    return *list = NULL;
}

bool wikt_translation_is_language(TidyBuffer buf, const char *lang) {
    const char *const tag = strchrnul((char*)buf.bp, '>');
    const char *const base = tag ? tag + 1: (char*)buf.bp;
    const char *const colon = strchrnul(base, ':');
    return strncasecmp(lang, base, (size_t)(colon - base)) == 0;
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
