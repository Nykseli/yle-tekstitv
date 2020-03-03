#include "tidy_std99.h"

#include <curl/curl.h>
#include <stdint.h>
#include <stdio.h>
#include <tidy.h>
#include <tidybuffio.h>

#include "html_parser.h"

/* curl write callback, to fill tidy's input buffer...  */
size_t write_to_buffer(char* in, size_t size, size_t nmemb, TidyBuffer* out)
{
    size_t r;
    r = size * nmemb;
    tidyBufAppend(out, in, r);
    return r;
}

void load_page(html_parser* parser, char* page)
{
    CURL* curl;
    char curl_errbuf[CURL_ERROR_SIZE];
    TidyBuffer docbuf = { 0 };
    int err;

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, page);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);
    // TODO: uncomment for verbose mode
    /* curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L); */
    /* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_buffer);

    tidyBufInit(&docbuf);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &docbuf);
    err = curl_easy_perform(curl);
    if (err)
        fprintf(stderr, "%s\n", curl_errbuf);

    /* clean-up */
    curl_easy_cleanup(curl);

    parser->_curl_buffer = docbuf;
}
