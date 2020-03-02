
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <tidy.h>
#include <tidybuffio.h>
#include <unistd.h>

#include "html_parser.h"

// What field of the teksti tv are we parsing
static int centers = 0;

// Copy text to target buffer and return amount of bytes written
static size_t node_text_to_buffer(TidyDoc doc, TidyNode tnod, char* target)
{
    size_t buffer_size;
    // Get the text from node
    TidyBuffer buf;
    tidyBufInit(&buf);
    tidyNodeGetText(doc, tnod, &buf);
    buffer_size = strlen((char*)buf.bp);
    buffer_size = buffer_size <= HTML_TEXT_MAX ? buffer_size : HTML_TEXT_MAX;
    // copy the node
    strncpy(target, (const char*)buf.bp, HTML_TEXT_MAX);
    target[buffer_size] = '\0';
    // Free text buffer
    tidyBufFree(&buf);

    return buffer_size;
}

static html_item node_to_html_text_item(TidyDoc doc, TidyNode tnod)
{
    html_item item;
    item.type = HTML_TEXT;
    item.item.text.size = node_text_to_buffer(doc, tnod, item.item.text.text);
    return item;
}

// inFont = is text under font node
static html_item node_to_html_link_item(TidyDoc doc, TidyNode tnod, Bool inFont)
{
    html_item item;
    html_link link;
    item.type = HTML_LINK;

    // Get the href and it's size
    TidyAttr atag = tidyAttrFirst(tnod);
    char* tag_text = (char*)tidyAttrValue(atag);
    size_t tag_size = strlen(tag_text);

    // Set href and size to link item url
    strncpy(link.url.text, tag_text, HTML_TEXT_MAX);
    link.url.size = tag_size;
    link.url.text[tag_size] = '\0';

    // Get the inner text and size of the link
    TidyNode atext = tidyGetChild(tnod);
    if (inFont)
        atext = tidyGetChild(atext);

    // Set inner text and size to link item inner text
    link.inner_text.size = node_text_to_buffer(doc, atext, link.inner_text.text);

    item.item.link = link;
    return item;
}

static void parse_title(TidyDoc doc, TidyNode tnod, html_parser* parser)
{
    // Title node is <pre><big>title</big></pre>
    TidyNode pre = tidyGetChild(tnod);
    TidyNode big = tidyGetChild(pre);
    TidyNode title_text = tidyGetChild(big);
    parser->title = html_item_as_text(node_to_html_text_item(doc, title_text));
}

static void parse_top_navigation(TidyDoc doc, TidyNode tnod, html_parser* parser)
{
    int node_index = 0;
    // First tag is is always p tag that contains the navigation information
    TidyNode ptag = tidyGetChild(tnod);
    for (TidyNode child = tidyGetChild(ptag); child; child = tidyGetNext(child)) {
        ctmbstr name = tidyNodeGetName(child);
        // Ignore non a tags
        if (!name)
            continue;

        // Set the navigations links
        parser->top_navigation[node_index] = html_item_as_link(node_to_html_link_item(doc, child, no));
        node_index++;
    }
}

static void parse_bottom_navigation(TidyDoc doc, TidyNode tnod, html_parser* parser)
{
    int node_index = 0;
    // First tag is is always p tag that contains the navigation information
    TidyNode ptag = tidyGetChild(tnod);
    // Get first 2 tags that contain the navigation information
    for (int i = 0; i < 2; i++, ptag = tidyGetNext(ptag)) {
        for (TidyNode child = tidyGetChild(ptag); child; child = tidyGetNext(child)) {
            ctmbstr name = tidyNodeGetName(child);
            // Ignore non a tags
            if (!name)
                continue;

            // parse a tag
            // every child tag with a name is an a tag

            // a tag only has href attribute
            TidyAttr atag = tidyAttrFirst(child);
            printf("Nav link: %s\n", tidyAttrValue(atag));
            // a tag always has the link text
            TidyNode text = tidyGetChild(child);
            TidyBuffer buf;
            tidyBufInit(&buf);
            tidyNodeGetText(doc, text, &buf);
            printf("Nav text: %s\n", buf.bp ? (char*)buf.bp : "");
            tidyBufFree(&buf);
            // Set the navigations links
            parser->bottom_navigation[node_index] = html_item_as_link(node_to_html_link_item(doc, child, no));
            node_index++;
        }
    }
}

static void parse_middle(TidyDoc doc, TidyNode tnod, html_parser* parser)
{
    html_item item_buffer[128];
    size_t item_buffer_size = 0;

    // Infos are under pre tag
    TidyNode pre = tidyGetChild(tnod);

    for (TidyNode child = tidyGetChild(pre); child; child = tidyGetNext(child)) {
        ctmbstr name = tidyNodeGetName(child);
        // No names are probably just noise?
        if (!name)
            continue;

        // Get links and fonts
        // Stucture is <font><a></a></font> or <a><font></font></a>

        // Link <a><font></font></a>
        if (strncmp(name, "a", 1) == 0) {
            item_buffer[item_buffer_size] = node_to_html_link_item(doc, child, yes);
            item_buffer_size++;
            // There is nothing left after single link
            continue;
        }

        // Font <font><a></a></font>
        TidyNode font = tidyGetChild(child);
        name = tidyNodeGetName(font);
        // Some times there are one or two links befre the actual text
        if (name && strncmp(name, "a", 1) == 0) {
            item_buffer[item_buffer_size] = node_to_html_link_item(doc, font, no);
            item_buffer_size++;

            for (TidyNode link = tidyGetNext(font); link; link = tidyGetNext(link)) {
                name = tidyNodeGetName(link);
                if (!name) // name is noise
                    continue;

                item_buffer[item_buffer_size] = node_to_html_link_item(doc, link, no);
                item_buffer_size++;
            }
        } else { // Just text
            item_buffer[item_buffer_size] = node_to_html_text_item(doc, font);
            item_buffer_size++;
        }
    }

    // Set items list from item_buffer
    parser->middle = malloc(sizeof(html_item) * item_buffer_size);
    memcpy(parser->middle, item_buffer, sizeof(html_item) * item_buffer_size);
    parser->middle_size = item_buffer_size;
}

static void parse_nodes(TidyDoc doc, TidyNode tnod, html_parser* parser)
{
    for (TidyNode child = tidyGetChild(tnod); child; child = tidyGetNext(child)) {
        ctmbstr name = tidyNodeGetName(child);
        //printf("name: %s\n", name);
        if (name && !strcmp(name, "center")) {
            centers++;
            switch (centers) {
            case 1:
                parse_title(doc, child, parser);
                break;
            case 2:
                parse_top_navigation(doc, child, parser);
                break;
            case 3:
                parse_middle(doc, child, parser);
                break;
            case 4:
                parse_bottom_navigation(doc, child, parser);
                break;
            default:
                break;
            }
        }

        parse_nodes(doc, child, parser); /* recursive */
    }
}

int parse_html(html_parser* parser, const char* html_text)
{
    TidyBuffer output = { 0 };
    TidyBuffer errbuf = { 0 };
    int rc = -1;
    Bool ok;

    TidyDoc tdoc = tidyCreate(); // Initialize "document"
    //printf( "Tidying:\t%s\n", input );

    ok = tidyOptSetBool(tdoc, TidyXhtmlOut, yes); // Convert to XHTML
    if (ok)
        rc = tidySetErrorBuffer(tdoc, &errbuf); // Capture diagnostics
    if (rc >= 0)
        rc = tidyParseString(tdoc, html_text); // Parse the input
    if (rc >= 0)
        rc = tidyCleanAndRepair(tdoc); // Tidy it up!
    if (rc >= 0)
        rc = tidyRunDiagnostics(tdoc); // Kvetch
    if (rc > 1) // If error, force output.
        rc = (tidyOptSetBool(tdoc, TidyForceOutput, yes) ? rc : -1);
    if (rc >= 0) {
        parse_nodes(tdoc, tidyGetRoot(tdoc), parser);
        rc = tidySaveBuffer(tdoc, &output); // Pretty Print
    }
    if (rc >= 0) {
        if (rc > 0)
            printf("\nDiagnostics:\n\n%s", errbuf.bp);
        //printf("\nAnd here is the result:\n\n%s", output.bp);
    } else
        printf("A severe error (%d) occurred.\n", rc);

    tidyBufFree(&output);
    tidyBufFree(&errbuf);
    tidyRelease(tdoc);
    return rc;
}

void init_html_parser(html_parser* parser)
{
    parser->middle = NULL;
    parser->middle_size = 0;
}

void free_html_parser(html_parser* parser)
{
    if (parser->middle != NULL)
        free(parser->middle);
}
