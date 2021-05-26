#include "dlpo.h"

#include <tidybuffio.h>

#include "html.h"
#include "utils.h"

TidyNode dlpo_find_definitions(TidyNode node) {
    if(!(node = tidyGetChild(node))) {
        log_err("no children\n");
        return NULL;
    }
    if(!(node = tidyGetNext(node))) {
        log_err("only one child\n");
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
        if(!(node = tidyGetNext(node))) {
            log_err("definition not found\n");
            return false;
        }
        if(strcmp(tidyNodeGetName(node), "div") != 0)
            continue;
        {
            TidyAttr id = find_attr(node, "id");
            if(id && strncmp(tidyAttrValue(id), "aa", 2) == 0)
                continue;
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
            fprintf(f, "- ");
            print_unescaped(f, buf.bp);
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
                join_lines(buf.bp, buf.bp + buf.size);
                fprintf(f, "  ");
                print_unescaped(f, buf.bp);
                fprintf(f, "\n");
                tidyBufFree(&buf);
                break;
            }
            if(a)
                break;
        }
    }
    return true;
}
