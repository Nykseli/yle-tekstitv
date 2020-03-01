
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <tidy.h>
#include <tidybuffio.h>
#include <unistd.h>

volatile int centers = 0;

static char* open_html(char* file)
{
    int fd = open(file, O_RDONLY);
    if (fd < 0)
        printf("Cannot open textitv_sivu.html\n");

    struct stat fs;
    fstat(fd, &fs);

    char* buff = malloc(fs.st_size);
    read(fd, buff, fs.st_size);

    return buff;
}

void parse_title(TidyDoc doc, TidyNode tnod)
{
    printf("Parse title:\n");
    TidyNode pre = tidyGetChild(tnod);
    TidyNode big = tidyGetChild(pre);
    TidyNode title_text = tidyGetChild(big);

    TidyBuffer buf;
    tidyBufInit(&buf);
    tidyNodeGetText(doc, title_text, &buf);
    printf("Title text: %s\n", buf.bp ? (char*)buf.bp : "");
    tidyBufFree(&buf);
}

void parse_top_navigation(TidyDoc doc, TidyNode tnod)
{
    // First tag is is always p tag that contains the navigation information
    TidyNode ptag = tidyGetChild(tnod);

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
    }
}

// inFont = is text under font node
void middle_parse_link(TidyDoc doc, TidyNode tnod, Bool inFont)
{
    TidyAttr atag = tidyAttrFirst(tnod);
    printf("Nav link: %s\n", tidyAttrValue(atag));
    TidyNode atext = tidyGetChild(tnod);
    if (inFont)
        atext = tidyGetChild(atext);
    TidyBuffer buf;
    tidyBufInit(&buf);
    tidyNodeGetText(doc, atext, &buf);
    printf("Nav link text: %s\n", buf.bp ? (char*)buf.bp : "");
    tidyBufFree(&buf);
}

void parse_middle(TidyDoc doc, TidyNode tnod)
{
    TidyBuffer buf;
    // Infos are under pre tag
    TidyNode pre = tidyGetChild(tnod);

    for (TidyNode child = tidyGetChild(pre); child; child = tidyGetNext(child)) {
        ctmbstr name = tidyNodeGetName(child);
        // No names are probably just noise?
        if (!name)
            continue;

        // Get links and fonts
        // Stucture is <font><a></a></font> or <a><font></font></a>
        printf("Middle name: %s\n", name);

        // Link (<a><font></font></a>)
        if (strncmp(name, "a", 1) == 0) {
            printf("Pure:\n");
            middle_parse_link(doc, child, yes);
            // There is nothing left after single link
            continue;
        }

        // Font (<font><a></a></font>)
        TidyNode font = tidyGetChild(child);
        name = tidyNodeGetName(font);
        // Some times there are one or two links befre the actual text
        if (name && strncmp(name, "a", 1) == 0) {

            printf("First link under font\n");
            middle_parse_link(doc, font, no);
            for (TidyNode link = tidyGetNext(font); link; link = tidyGetNext(link)) {
                name = tidyNodeGetName(link);
                if (!name) // name is noice
                    continue;

                printf("Second link under font\n");
                middle_parse_link(doc, link, no);
            }
        } else { // Just text
            tidyBufInit(&buf);
            tidyNodeGetText(doc, font, &buf);
            printf("Text in font: %s\n", buf.bp ? (char*)buf.bp : "");
            tidyBufFree(&buf);
        }
    }
}

void parse_bottom_navigation(TidyDoc doc, TidyNode tnod)
{
    printf("Bottom navigation:\n");
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
        }
    }
}

void dumpNode(TidyDoc doc, TidyNode tnod, int indent)
{
    TidyNode child;
    for (child = tidyGetChild(tnod); child; child = tidyGetNext(child)) {
        ctmbstr name = tidyNodeGetName(child);
        //printf("name: %s\n", name);
        if (name && !strcmp(name, "center")) {
            centers++;
            switch (centers) {
            case 1:
                parse_title(doc, child);
                break;
            case 2:
                parse_top_navigation(doc, child);
                break;
            case 3:
                parse_middle(doc, child);
                break;
            case 4:
                parse_bottom_navigation(doc, child);
                break;
            default:
                break;
            }
        }

        dumpNode(doc, child, indent + 4); /* recursive */
    }
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("Usage: %s <html file>\n", argv[0]);
        return 1;
    }

    const char* input = open_html(argv[1]);
    TidyBuffer output = { 0 };
    TidyBuffer errbuf = { 0 };
    int rc = -1;
    Bool ok;

    TidyNode node;

    TidyDoc tdoc = tidyCreate(); // Initialize "document"
    //printf( "Tidying:\t%s\n", input );

    ok = tidyOptSetBool(tdoc, TidyXhtmlOut, yes); // Convert to XHTML
    if (ok)
        rc = tidySetErrorBuffer(tdoc, &errbuf); // Capture diagnostics
    if (rc >= 0)
        rc = tidyParseString(tdoc, input); // Parse the input
    if (rc >= 0)
        rc = tidyCleanAndRepair(tdoc); // Tidy it up!
    if (rc >= 0)
        rc = tidyRunDiagnostics(tdoc); // Kvetch
    if (rc > 1) // If error, force output.
        rc = (tidyOptSetBool(tdoc, TidyForceOutput, yes) ? rc : -1);
    if (rc >= 0) {
        dumpNode(tdoc, tidyGetRoot(tdoc), 0);
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

    printf("centers found: %d\n", centers);

    return rc;
}
