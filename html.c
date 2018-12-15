#include "html.h"

TidyNode find_node_by_name(TidyNode node, const char *name) {
    for(; node; node = tidyGetNext(node)) {
        ctmbstr n = tidyNodeGetName(node);
        if(n && strcmp(n, name) == 0)
            break;
    }
    return node;
}

TidyNode find_node_by_name_prefix(TidyNode node, const char *name) {
    size_t len = strlen(name);
    for(; node; node = tidyGetNext(node)) {
        ctmbstr n = tidyNodeGetName(node);
        if(n && strncmp(n, name, len) == 0)
            break;
    }
    return node;
}

TidyAttr find_attr(TidyNode node, const char *name) {
    TidyAttr ret = tidyAttrFirst(node);
    while(ret && strcmp(tidyAttrName(ret), name))
        ret = tidyAttrNext(ret);
    return ret;
}

TidyNode find_node_by_id(TidyNode node, const char *id, bool rec) {
    TidyNode child;
    for(child = tidyGetChild(node); child; child = tidyGetNext(child)) {
        for(TidyAttr a = tidyAttrFirst(child); a; a = tidyAttrNext(a)) {
            if(strcmp(tidyAttrName(a), "id"))
                continue;
            if(strcmp(tidyAttrValue(a), id))
                break;
            else
                return child;
        }
        if(!rec)
            continue;
        TidyNode ret = find_node_by_id(child, id, rec);
        if(ret)
            return ret;
    }
    return 0;
}

void trim_tag(const unsigned char **b, const unsigned char **e) {
    for(; *b && **b != '>'; ++*b);
    ++*b;
    for(; *e > *b && **e != '<'; --*e);
}

void print_unescaped(const unsigned char *s) {
    for(;;) {
        if(strncmp((const char *)s, "&nbsp;", 6) == 0) {
            printf(" ");
            s += 5;
        } else if(*s == '<')
            while(*s && *s != '>')
                ++s;
        if(!*s)
            break;
        ++s;
        const unsigned char *e = s;
        while(*e && *e != '&' && *e != '<')
            ++e;
        printf("%.*s", (int)(e - s), s);
        s = e;
    }
    printf("\n");
}
