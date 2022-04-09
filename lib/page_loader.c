
#include <curl/curl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <tekstitv.h>

/* curl write callback, to fill html input buffer...  */
size_t write_to_buffer(char* in, size_t size, size_t nmemb, void* out)
{
    html_buffer* buf = (html_buffer*)out;
    size_t r = size * nmemb;
    memcpy(buf->html + buf->size, in, r);
    buf->size += r;

    return r;
}

void load_page(html_parser* parser)
{
    CURL* curl;
    char curl_errbuf[CURL_ERROR_SIZE];
    parser->_curl_buffer.current = 0;
    parser->_curl_buffer.size = 0;
    parser->curl_load_error = false;
    int err;
    char page[] = "https://yle.fi/tekstitv/txt/xxx_xxxx.htm";
    memcpy(page + 28, parser->link, HTML_LINK_SIZE);

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, page);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Yle teletext reader " TEKSTITV_STR_VERSION);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    // TODO: uncomment for verbose mode
    /* curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L); */
    /* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_buffer);
// On windows, we need to provide our own perm file from  from https://curl.se/docs/caextract.html
#ifdef _WIN32
    curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(curl, CURLOPT_CAINFO, "assets/cacert.pem");
#endif

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &parser->_curl_buffer);
    err = curl_easy_perform(curl);
    if (err)
        //fprintf(stderr, "%s\n", curl_errbuf);
        parser->curl_load_error = true;

    /* clean-up */
    curl_easy_cleanup(curl);
}
