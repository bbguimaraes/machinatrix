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
    return NULL;
}

TidyAttr find_attr(TidyNode node, const char *name) {
    TidyAttr ret = tidyAttrFirst(node);
    while(ret && strcmp(tidyAttrName(ret), name))
        ret = tidyAttrNext(ret);
    return ret;
}

static void *memrchr(const void *p, int c, size_t n) {
    const unsigned char *b = p, *e = b + n;
    while(--e >= b)
        if(*e == c)
            return (void *)e;
    return NULL;
}

void trim_tag(const unsigned char **pb, const unsigned char **pe) {
    const unsigned char *b = *pb, *e = *pe;
    assert(b <= e);
    if(*b == '<') {
        const unsigned char *n = memchr(b, '>', (size_t)(e - b));
        if(n)
            b = n + 1;
    }
    if(*(e - 1) == '>') {
        const unsigned char *n = memrchr(b, '<', (size_t)(e - b - 1));
        if(n)
            e = n;
    }
    *pb = b;
    *pe = e;
}

void print_unescaped(FILE *f, const unsigned char *s) {
    for(;;) {
        if(strncmp((const char *)s, "&nbsp;", 6) == 0) {
            fprintf(f, " ");
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
        fprintf(f, "%.*s", (int)(e - s), s);
        s = e;
    }
}
