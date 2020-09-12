#include "html.h"

#include "tests/common.h"

#define HTML_START \
    "<!DOCTYPE html><html><head><title>title</title></head><body>"
#define HTML_END "</body></html>"
#define HTML(x) HTML_START x HTML_END

const char *PROG_NAME = NULL;
const char *CMD_NAME = NULL;

static bool test_find_node_by_name() {
    const char html[] = HTML(
        "<h1>h1</h1>"
        "<span name=\"span\"/>"
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

static bool test_find_node_by_name_prefix() {
    const char html[] = HTML(
        "<span name=\"span\"/>"
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

static bool test_find_node_by_id() {
    const char html[] = HTML(
        "<h1 id=\"h1\">h1</h1>"
        "<span id=\"span\"/>"
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

static bool test_find_node_by_id_rec() {
    const char html[] = HTML(
        "<div>"
        "<h1 id=\"h1\">h1</h1>"
        "<span id=\"span\"/>"
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

static bool test_find_attr() {
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

static bool test_trim_tag() {
    // clang-format off
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
    // clang-format on
    bool ret = true;
    for(size_t i = 0, n = sizeof(t) / sizeof(*t); i < n; ++i) {
        assert(printf("\n  \"%s\" ", t[i].in) > 0);
        assert(fflush(stdout) == 0);
        const char *e = t[i].in + t[i].in_len;
        trim_tag((const unsigned char **)&t[i].in, (const unsigned char **)&e);
        const size_t n = e - t[i].in;
        const bool iret = ASSERT_STR_EQ_N(t[i].in, t[i].out, n);
        if(!iret)
            printf("fail ");
        ret &= iret;
    }
    return ret;
}

static bool test_print_unescaped() {
    FILE *out = tmpfile();
    char buf[] =
        "<p>A paragraph of <b>text</b> with <i>several</i> HTML tags.</p>"
        "<hr />"
        "<img src=\"test.png\" />";
    print_unescaped(out, (const unsigned char *)buf);
    const size_t n = ftell(out);
    assert(n >= 0);
    assert(n < sizeof(buf));
    assert(fseek(out, 0, SEEK_SET) >= 0);
    const char expected[] = "A paragraph of text with several HTML tags.";
    bool ret = ASSERT_EQ(n, sizeof(expected) - 1);
    ret = ASSERT_EQ(fread(buf, 1, n, out), n) && ret;
    buf[n] = 0;
    ret = ASSERT_STR_EQ(buf, expected) && ret;
    return ret;
}

int main() {
    bool ret = true;
    ret = RUN(test_find_node_by_name) && ret;
    ret = RUN(test_find_node_by_name_prefix) && ret;
    ret = RUN(test_find_node_by_id) && ret;
    ret = RUN(test_find_node_by_id_rec) && ret;
    ret = RUN(test_find_attr) && ret;
    ret = RUN(test_trim_tag) && ret;
    ret = RUN(test_print_unescaped) && ret;
    return !ret;
}
