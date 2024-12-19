/**
 * \file
 * Functions to process and navigate DLPO pages.
 * http://www.priberam.pt/dlpo
 */
#include "common.h"

#include <tidy.h>

/** Base URL for the service. */
#define DLPO_BASE "https://dicionario.priberam.org"

/**
 * Finds the element containing the word definitions.
 * \param node The `#resultados` element.
 */
TidyNode dlpo_find_definitions(TidyNode node);

/**
 * Prints the definitions in plain text.
 * \param f Output stream.
 * \param doc The root document.
 * \param node The element returned by \ref dlpo_find_definitions.
 */
void dlpo_print_definitions(
    FILE *f, TidyDoc doc, TidyNode def, TidyBuffer *buf);
