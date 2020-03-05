
#include <curl/curl.h>
#include <stdint.h>
#include <stdio.h>

#include "html_parser.h"

/* curl write callback, to fill html input buffer...  */
size_t write_to_buffer(char* in, size_t size, size_t nmemb, void* out)
{
    html_buffer* buf = (html_buffer*)out;
    size_t r = size * nmemb;
    memcpy(buf->html + buf->size, in, r);
    buf->size += r;

    return r;
}

void load_page(html_parser* parser, char* page)
{
    CURL* curl;
    char curl_errbuf[CURL_ERROR_SIZE];
    parser->_curl_buffer.current = 0;
    parser->_curl_buffer.size = 0;
    int err;

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, page);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);
    // TODO: uncomment for verbose mode
    /* curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L); */
    /* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_buffer);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &parser->_curl_buffer);
    err = curl_easy_perform(curl);
    if (err)
        fprintf(stderr, "%s\n", curl_errbuf);

    /* clean-up */
    curl_easy_cleanup(curl);

}
