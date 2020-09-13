#include "dlpo.h"

#include <tidybuffio.h>

#include "html.h"
#include "utils.h"

TidyNode dlpo_find_definitions(TidyNode node) {
    if(!(node = tidyGetChild(node))) {
        log_err("no children\n");
        return NULL;
    }
    if(tidyGetNext(node)) {
        log_err("more than one child\n");
        return NULL;
    }
    if(!(node = tidyGetChild(node))) {
        log_err("no grandchildren\n");
        return NULL;
    }
    for(; node; node = tidyGetNext(node))
        if(find_node_by_name(tidyGetChild(node), "br"))
            return tidyGetChild(node);
    log_err("definition not found\n");
    return NULL;
}

bool dlpo_print_definitions(FILE *f, TidyDoc doc, TidyNode node) {
    for(;; node = tidyGetNext(node)) {
        node = find_node_by_name(node, "br");
        if(!node)
            return true;
        node = tidyGetNext(node);
        if(!node) {
            log_err("definition not found\n");
            return false;
        }
        TidyNode child = node;
        for(size_t i = 0; i < 4; ++i) {
            child = tidyGetChild(child);
            if(!child) {
                log_err("child^%lu of definition not found\n", i);
                return false;
            }
        }
        {
            TidyBuffer buf = {0};
            tidyNodeGetText(doc, child, &buf);
            fprintf(f, "- %s", buf.bp);
            tidyBufFree(&buf);
        }
        child = tidyGetNext(tidyGetChild(node));
        for(; child; child = tidyGetNext(child)) {
            TidyAttr a;
            for(a = tidyAttrFirst(child); a; a = tidyAttrNext(a)) {
                if(strcmp(tidyAttrName(a), "class"))
                    continue;
                ctmbstr v = tidyAttrValue(a);
                if(!v || strcmp(v, "def"))
                    continue;
                TidyBuffer buf = {0};
                tidyNodeGetText(doc, child, &buf);
                unsigned char *b = buf.bp;
                unsigned char *e = buf.bp + buf.size - 1;
                const unsigned char **cb = (const unsigned char **)&b;
                const unsigned char **ce = (const unsigned char **)&e;
                trim_tag(cb, ce);
                join_lines(b, e);
                fprintf(f, "  %.*s\n", (int)(e - b), b);
                tidyBufFree(&buf);
                break;
            }
            if(a)
                break;
        }
    }
    return true;
}
