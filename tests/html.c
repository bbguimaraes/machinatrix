#include "html.h"

#include "tests/common.h"

#define HTML_START \
    "<!DOCTYPE html><html><head><title>title</title></head><body>"
#define HTML_END "</body></html>"

const char *PROG_NAME = NULL;
const char *CMD_NAME = NULL;

static bool test_find_node_by_name() {
    const char html[] =
        HTML_START
        "<h1>h1</h1>"
        "<span name=\"span\"/>"
        "<div name=\"div\"/>"
        HTML_END;
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
    const char html[] =
        HTML_START
        "<span name=\"span\"/>"
        "<div name=\"div\"/>"
        "<h1>h1</h1>"
        HTML_END;
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
    const char html[] =
        HTML_START
        "<h1 id=\"h1\">h1</h1>"
        "<span id=\"span\"/>"
        "<div id=\"test\"/>"
        HTML_END;
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
    const char html[] =
        HTML_START
        "<div>"
            "<h1 id=\"h1\">h1</h1>"
            "<span id=\"span\"/>"
            "<div id=\"test\"/>"
        "</div>"
        HTML_END;
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
    const char html[] =
        HTML_START
        "<div name=\"name\" id=\"id\" style=\"test\" />"
        HTML_END;
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
    const char buf[] = "<p>text</p>";
    const char *b = buf, *e = buf + sizeof(buf);
    trim_tag((const unsigned char**)&b, (const unsigned char**)&e);
    const char expected[] = "text";
    if(strncmp(b, expected, e - b) != 0) {
        fprintf(stderr, "unexpected output: %s\n", buf);
        return false;
    }
    return true;
}

static bool test_print_unescaped() {
    FILE *out = tmpfile();
    char buf[] =
        "<p>A paragraph of <b>text</b> with <i>several</i> HTML tags.</p>"
        "<hr />"
        "<img src=\"test.png\" />";
    print_unescaped(out, (const unsigned char*)buf);
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
