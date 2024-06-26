#include "html.h"

#include <tidybuffio.h>

#include "tests/common.h"

#define HTML_START \
    "<!DOCTYPE html><html><head><title>title</title></head><body>"
#define HTML_END "</body></html>"
#define HTML(x) HTML_START x HTML_END

const char *PROG_NAME = NULL;
const char *CMD_NAME = NULL;

static bool test_find_node_by_name(void) {
    const char html[] = HTML(
        "<h1>h1</h1>"
        "<span name=\"span\">span</span>"
        "<div name=\"div\"/>");
    const TidyDoc doc = tidyCreate();
    tidyParseString(doc, html);
    const TidyNode node =
        find_node_by_name(tidyGetChild(tidyGetBody(doc)), "div");
    bool ret = ASSERT(node);
    const ctmbstr name = node ? tidyNodeGetName(node) : "";
    ret = ASSERT_STR_EQ(name, "div") && ret;
    tidyRelease(doc);
    return ret;
}

static bool test_find_node_by_name_prefix(void) {
    const char html[] = HTML(
        "<span name=\"span\">span</span>"
        "<div name=\"div\"/>"
        "<h1>h1</h1>");
    const TidyDoc doc = tidyCreate();
    tidyParseString(doc, html);
    const TidyNode node =
        find_node_by_name_prefix(tidyGetChild(tidyGetBody(doc)), "h");
    bool ret = ASSERT(node);
    const ctmbstr name = node ? tidyNodeGetName(node) : "";
    ret = ASSERT_STR_EQ(name, "h1") && ret;
    tidyRelease(doc);
    return ret;
}

static bool test_find_node_by_class(void) {
    const char html[] = HTML(
        "<h1 class=\"h1\">h1</h1>"
        "<span class=\"span\">span</span>"
        "<div class=\"test\"/>");
    const TidyDoc doc = tidyCreate();
    tidyParseString(doc, html);
    const TidyNode node =
        find_node_by_class(tidyGetChild(tidyGetBody(doc)), "test", true);
    bool ret = ASSERT(node);
    const ctmbstr name = node ? tidyNodeGetName(node) : "";
    ret = ASSERT_STR_EQ(name, "div") && ret;
    const TidyAttr attr = find_attr(node, "class");
    const ctmbstr id = attr ? tidyAttrValue(attr) : "";
    ret = ASSERT_STR_EQ(id, "test") && ret;
    tidyRelease(doc);
    return ret;
}

static bool test_find_node_by_id(void) {
    const char html[] = HTML(
        "<h1 id=\"h1\">h1</h1>"
        "<span id=\"span\">span</span>"
        "<div id=\"test\"/>");
    const TidyDoc doc = tidyCreate();
    tidyParseString(doc, html);
    const TidyNode node = find_node_by_id(tidyGetBody(doc), "test", false);
    bool ret = ASSERT(node);
    const ctmbstr name = node ? tidyNodeGetName(node) : "";
    ret = ASSERT_STR_EQ(name, "div") && ret;
    const TidyAttr attr = tidyAttrFirst(node);
    const ctmbstr id = attr ? tidyAttrValue(attr) : "";
    ret = ASSERT_STR_EQ(id, "test") && ret;
    tidyRelease(doc);
    return ret;
}

static bool test_find_node_by_id_rec(void) {
    const char html[] = HTML(
        "<div>"
        "<h1 id=\"h1\">h1</h1>"
        "<span id=\"span\">span</span>"
        "<div id=\"test\"/>"
        "</div>");
    const TidyDoc doc = tidyCreate();
    tidyParseString(doc, html);
    const TidyNode first = tidyGetBody(doc);
    bool ret = ASSERT(!find_node_by_id(first, "test", false));
    const TidyNode node = find_node_by_id(first, "test", true);
    ret = ASSERT(node) && ret;
    const ctmbstr name = node ? tidyNodeGetName(node) : "";
    ret = ASSERT_STR_EQ(name, "div") && ret;
    const TidyAttr attr = tidyAttrFirst(node);
    const ctmbstr id = attr ? tidyAttrValue(attr) : "";
    ret = ASSERT_STR_EQ(id, "test") && ret;
    tidyRelease(doc);
    return ret;
}

static bool test_find_node_by_content(void) {
    const char html[] = HTML(
        "<div>"
            "<h1 id=\"h1\">h1</h1>"
            "<span id=\"span\">content</span>"
            "<div id=\"test\"/>"
        "</div>");
    const TidyDoc doc = tidyCreate();
    tidyParseString(doc, html);
    const TidyNode first = tidyGetBody(doc);
    TidyBuffer buf = {0};
    const TidyNode node = find_node_by_content(
        doc, first, "content\n", &buf, true);
    bool ret = ASSERT(node);
    const ctmbstr name = tidyNodeGetName(tidyGetParent(node));
    ret = ASSERT(name) && ret;
    if(name)
        ret = ASSERT_STR_EQ(name, "span") && ret;
    tidyBufClear(&buf);
    ret = tidyNodeGetText(doc, node, &buf) && ret;
    ret = ASSERT_STR_EQ_N((const char*)buf.bp, "content\n", buf.size) && ret;
    tidyBufFree(&buf);
    tidyRelease(doc);
    return ret;
}

static bool test_find_attr(void) {
    const char html[] = HTML("<div name=\"name\" id=\"id\" style=\"test\" />");
    const TidyDoc doc = tidyCreate();
    tidyParseString(doc, html);
    const TidyAttr attr = find_attr(tidyGetChild(tidyGetBody(doc)), "style");
    bool ret = ASSERT(attr);
    const ctmbstr value = attr ? tidyAttrValue(attr) : "";
    ret = ASSERT_STR_EQ(value, "test") && ret;
    tidyRelease(doc);
    return ret;
}

static bool test_trim_tag(void) {
#define CASE(x, y) {sizeof(x) - 1, sizeof(y) - 1, x, y}
    const struct {
        size_t in_len, out_len;
        const char *in, *out;
    } t[] = {
        CASE("text", "text"),
        CASE("<text", "<text"),
        CASE(">text", ">text"),
        CASE("<>text", "text"),
        CASE("<p>text", "text"),
        CASE("<p>text<", "text<"),
        CASE("<p>text</p", "text</p"),
        CASE("<p>text</p>", "text")};
#undef CASE
    bool ret = true;
    for(size_t i = 0, n = sizeof(t) / sizeof(*t); i < n; ++i) {
        assert(printf("\n  \"%s\" ", t[i].in) > 0);
        assert(fflush(stdout) == 0);
        const char *e = t[i].in + t[i].in_len;
        trim_tag((const unsigned char **)&t[i].in, (const unsigned char **)&e);
        const size_t n = (size_t)(e - t[i].in);
        const bool iret = ASSERT_STR_EQ_N(t[i].in, t[i].out, n);
        if(!iret)
            printf("fail ");
        ret &= iret;
    }
    return ret;
}

static bool test_print_unescaped(void) {
    FILE *out = tmpfile();
    char buf[] =
        "<p>"
            "A paragraph of <b>text</b> with <i>several</i> HTML tags.</p>"
            "&nbsp;&nbsp; &nbsp;<hr />&lt;&gt;&amp;&xxx;text"
        "</p>"
        "text>"
        "<hr />"
        "<img src=\"test.png\" />&<";
    print_unescaped(out, (const unsigned char *)buf);
    const long n = ftell(out);
    assert(n >= 0);
    assert((size_t)n < sizeof(buf));
    assert(fseek(out, 0, SEEK_SET) >= 0);
    const char expected[] =
        "A paragraph of text with several HTML tags."
        "    <>&&xxx;texttext>&";
    bool ret = ASSERT_EQ((size_t)n, sizeof(expected) - 1);
    ret = ASSERT_EQ(fread(buf, 1, (size_t)n, out), (size_t)n) && ret;
    buf[n] = 0;
    ret = ASSERT_STR_EQ(buf, expected) && ret;
    return ret;
}

int main(void) {
    bool ret = true;
    ret = RUN(test_find_node_by_name) && ret;
    ret = RUN(test_find_node_by_name_prefix) && ret;
    ret = RUN(test_find_node_by_class) && ret;
    ret = RUN(test_find_node_by_id) && ret;
    ret = RUN(test_find_node_by_id_rec) && ret;
    ret = RUN(test_find_node_by_content) && ret;
    ret = RUN(test_find_attr) && ret;
    ret = RUN(test_trim_tag) && ret;
    ret = RUN(test_print_unescaped) && ret;
    return !ret;
}
