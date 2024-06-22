#include "html.h"

#include <tidybuffio.h>

#include "utils.h"

bool list_has_class(const char *s, const char *cls) {
    const size_t n = strlen(cls);
    while(*(s += strspn(s, " "))) {
        const char *const e = strchrnul(s + 1, ' ');
        if(n == (size_t)(e - s) && strncmp(s, cls, n) == 0)
            return true;
        s = e + !!*e;
    }
    return false;
}

bool node_has_class(TidyNode node, const char *cls) {
    TidyAttr const a = find_attr(node, "class");
    ctmbstr const v = tidyAttrValue(a);
    return a && v && list_has_class(v, cls);
}

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

TidyNode find_node_by_class(TidyNode node, const char *cls, bool rec) {
    for(; node; node = tidyGetNext(node)) {
        if(node_has_class(node, cls))
            break;
        if(!rec)
            continue;
        TidyNode const ret = find_node_by_class(tidyGetChild(node), cls, rec);
        if(ret)
            return ret;
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

TidyNode find_node_by_content(
    TidyDoc doc, TidyNode node, const char *s, TidyBuffer *b, bool rec)
{
    for(; node; node = tidyGetNext(node)) {
        TidyNode const child = tidyGetChild(node);
        if(!child) {
            tidyNodeGetText(doc, node, b);
            const bool ret = strncmp((const char*)b->bp, s, b->size) == 0;
            tidyBufClear(b);
            if(ret)
                break;
        }
        if(!rec)
            continue;
        TidyNode const ret = find_node_by_content(doc, child, s, b, rec);
        if(ret)
            return ret;
    }
    return node;
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

static const unsigned char *unescape(FILE *f, const unsigned char *s);
static const unsigned char *skip_tags(const unsigned char *s);
static const unsigned char *copy_text(FILE *f, const unsigned char *s);

void print_unescaped(FILE *f, const unsigned char *s) {
    while(*s)
        s = (*s == '&') ? unescape(f, s)
            : (*s == '<') ? skip_tags(s)
            : copy_text(f, s);
}

const unsigned char *unescape(FILE *f, const unsigned char *s) {
    ++s;
#define X(t, r) \
    if(strncmp((const char*)s, #t ";", sizeof(#t)) == 0) { \
        fputc(r, f); \
        return s + sizeof(#t); \
    }
    X(nbsp, ' ')
    X(lt, '<')
    X(gt, '>')
    X(amp, '&')
#undef X
    fputc('&', f);
    return s;
}

const unsigned char *skip_tags(const unsigned char *s) {
    while(*++s && *s != '>');
    return s + (*s == '>');
}

const unsigned char *copy_text(FILE *f, const unsigned char *s) {
    const unsigned char *const p = s;
    while(*s && *s != '&' && *s != '<')
        ++s;
    fprintf(f, "%.*s", (int)(s - p), p);
    return s;
}
