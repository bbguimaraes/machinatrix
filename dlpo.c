#include "dlpo.h"

#include <tidybuffio.h>

#include "html.h"
#include "utils.h"

#define DEF "dp-definicao"
#define ETYM "Origem etimológica:"

TidyNode dlpo_find_definitions(TidyNode node) {
    return find_node_by_class(node, DEF, true);
}

void dlpo_print_definitions(
    FILE *f, TidyDoc doc, TidyNode def, TidyBuffer *buf
) {
    for(; def; def = find_node_by_class(tidyGetNext(def), DEF, false)) {
        for(TidyNode sec = tidyGetChild(def); sec; sec = tidyGetNext(sec)) {
            TidyNode node;
            if(!(
                (node = find_node_by_content(
                    doc, tidyGetChild(sec), ETYM "\n", buf, true))
                && (node = tidyGetParent(node))
                && (node = tidyGetNext(node))
            ))
                continue;
            tidyNodeGetText(doc, node, buf);
            join_lines(buf->bp, buf->bp + buf->size);
            fprintf(f, "- ");
            print_unescaped(f, buf->bp);
            putchar('\n');
        }
    }
}
