/**
 * \file
 * Functions to process and navigate Wiktionary pages.
 * https://www.wiktionary.org
 */
#include <stdbool.h>

#include "common.h"

#include <tidy.h>

/** Base URL for the service. */
#define WIKTIONARY_BASE "https://en.wiktionary.org/wiki"

/** Relevant elements of a page. */
typedef struct {
    /** Table-of-contents element. */
    TidyNode toc;
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
