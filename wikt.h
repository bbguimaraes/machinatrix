/**
 * \file
 * Functions to process and navigate Wiktionary pages.
 * https://www.wiktionary.org
 */
#include <stdbool.h>

#include "common.h"

#include <tidy.h>
#include <tidybuffio.h>

/** Base URL for the service. */
#define WIKTIONARY_BASE "https://en.wiktionary.org/wiki"
#define WIKTIONARY_H2 "mw-heading2"
#define WIKTIONARY_H3 "mw-heading3"

/** Relevant elements of a page. */
typedef struct {
    /** Main content element of the page. */
    TidyNode contents;
} wikt_page;

/**
 * Finds the page elements.
 * \param doc The root document.
 * \param p Output parameter.
 */
bool wikt_parse_page(TidyDoc doc, wikt_page *p);

/** Finds the header of a translation element. */
TidyNode wikt_translation_head(TidyNode node);

/** Finds the body of a translation element. */
TidyNode wikt_translation_body(TidyNode node);

/** Moves forward to the next translation item. */
TidyNode wikt_next_translation_block(TidyNode node, TidyNode *list);

/** Checks whether an item is a translation to a given language. */
bool wikt_translation_is_language(TidyBuffer buf, const char *lang);

/**
 * Advances `node` until the next section.
 * \return Whether a section was found.
 */
bool wikt_next_section(const char *cls, const char *prefix, TidyNode *node);

/**
 * Advances `node` until the next subsection.
 * \return Whether a subsection was found.
 */
bool wikt_next_subsection(const char *cls, const char *prefix, TidyNode *node);
