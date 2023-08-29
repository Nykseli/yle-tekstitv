
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <tekstitv.h>
#include <unistd.h>

typedef enum {
    UNKNOWN,
    P,
    BIG,
    PRE,
    LINK,
    FONT,
    CENTER
} tag_type;

static inline void skip_next_str(html_buffer* buffer, const char* str, size_t len)
{
    // find the string
    for (; buffer->current < buffer->size; buffer->current++) {
        if (strncmp(buffer->html + buffer->current, str, len) == 0)
            break;
    }

    // actually skip the str
    buffer->current += len;
}

static inline void skip_next_char(html_buffer* buffer, const char c)
{
    // find the char
    for (; buffer->current < buffer->size; buffer->current++) {
        if (buffer->html[buffer->current] == c)
            break;
    }

    // actually skip the char
    buffer->current++;
}

static inline void skip_next_tag(html_buffer* buffer, const char* name, size_t name_len, bool closing)
{
    char tag_buff[64] = { '<' };
    int buf_len = 1;

    if (closing) {
        tag_buff[1] = '/';
        buf_len++;
    }

    strncpy(tag_buff + buf_len, name, name_len);
    buf_len += name_len;

    for (; buffer->current < buffer->size; buffer->current++) {
        if (strncmp(buffer->html + buffer->current, tag_buff, buf_len) == 0) {
            buffer->current += buf_len;
            break;
        }
    }

    skip_next_char(buffer, '>');
}

static tag_type get_tag_type(html_buffer* buffer)
{
    char c;
    size_t tag_len = 0;
    char* html = buffer->html + buffer->current;

    // Skip the possible tag opener
    if (*html == '<')
        html++;

    while ((c = html[tag_len]) != '\0' && c != '>' && c != ' ')
        tag_len++;

    if (tag_len == 1) {
        if (strncmp(html, "p", 1) == 0)
            return P;
        if (strncmp(html, "a", 1) == 0)
            return LINK;
    } else if (tag_len == 3) {
        if (strncmp(html, "big", 3) == 0)
            return BIG;
        if (strncmp(html, "pre", 3) == 0)
            return PRE;
    } else if (tag_len == 4) {
        if (strncmp(html, "font", 4) == 0)
            return FONT;
    } else if (tag_len == 6) {
        if (strncmp(html, "center", 6) == 0)
            return CENTER;
    }

    return UNKNOWN;
}

/**
 * How many bytes are in text in the current position
 */
static size_t get_current_text_size(html_buffer* buffer)
{
    size_t text_len = 0;
    for (;; text_len++) {
        char c = buffer->html[buffer->current + text_len];
        if (c == '<' || c == '\r')
            break;
    }

    return text_len;
}

#ifndef DISABLE_UTF_8
static void copy_html_character_utf8(unsigned char* target, char* src, size_t* tpos, size_t* spos)
{
    *spos += 1;
    switch (*(src + (*spos - 1))) {
    case '#':
        if (strncmp(src + *spos, "256;", 4) == 0) {
            // LATIN CAPITAL LETTER A WITH MACRON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x80;
            *spos += 3;
        } else if (strncmp(src + *spos, "257;", 4) == 0) {
            // LATIN SMALL LETTER A WITH MACRON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x81;
            *spos += 3;
        } else if (strncmp(src + *spos, "260;", 4) == 0) {
            // LATIN CAPITAL LETTER A WITH OGONEK
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x84;
            *spos += 3;
        } else if (strncmp(src + *spos, "261;", 4) == 0) {
            // LATIN SMALL LETTER A WITH OGONEK
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x85;
            *spos += 3;
        } else if (strncmp(src + *spos, "263;", 4) == 0) {
            // LATIN SMALL LETTER C WITH ACUTE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x87;
            *spos += 3;
        } else if (strncmp(src + *spos, "264;", 4) == 0) {
            // LATIN CAPITAL LETTER C WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x88;
            *spos += 3;
        } else if (strncmp(src + *spos, "265;", 4) == 0) {
            // LATIN SMALL LETTER C WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x89;
            *spos += 3;
        } else if (strncmp(src + *spos, "266;", 4) == 0) {
            // LATIN CAPITAL LETTER C WITH DOT ABOVE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x8a;
            *spos += 3;
        } else if (strncmp(src + *spos, "267;", 4) == 0) {
            // LATIN SMALL LETTER C WITH DOT ABOVE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x8b;
            *spos += 3;
        } else if (strncmp(src + *spos, "268;", 4) == 0) {
            // LATIN CAPITAL LETTER C WITH CARON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x8c;
            *spos += 3;
        } else if (strncmp(src + *spos, "269;", 4) == 0) {
            // LATIN SMALL LETTER C WITH CARON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x8d;
            *spos += 3;
        } else if (strncmp(src + *spos, "270;", 4) == 0) {
            // LATIN CAPITAL LETTER D WITH CARON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x8e;
            *spos += 3;
        } else if (strncmp(src + *spos, "271;", 4) == 0) {
            // LATIN SMALL LETTER D WITH CARON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x8f;
            *spos += 3;
        } else if (strncmp(src + *spos, "273;", 4) == 0) {
            // LATIN SMALL LETTER D WITH STROKE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x91;
            *spos += 3;
        } else if (strncmp(src + *spos, "274;", 4) == 0) {
            // LATIN CAPITAL LETTER E WITH MACRON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x92;
            *spos += 3;
        } else if (strncmp(src + *spos, "275;", 4) == 0) {
            // LATIN SMALL LETTER E WITH MACRON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x93;
            *spos += 3;
        } else if (strncmp(src + *spos, "278;", 4) == 0) {
            //  LATIN CAPITAL LETTER E WITH DOT ABOVE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x96;
            *spos += 3;
        } else if (strncmp(src + *spos, "279;", 4) == 0) {
            //  LATIN SMALL LETTER E WITH DOT ABOVE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x97;
            *spos += 3;
        } else if (strncmp(src + *spos, "280;", 4) == 0) {
            // LATIN CAPITAL LETTER E WITH OGONEK
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x98;
            *spos += 3;
        } else if (strncmp(src + *spos, "281;", 4) == 0) {
            // LATIN CAPITAL LETTER E WITH OGONEK
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x99;
            *spos += 3;
        } else if (strncmp(src + *spos, "282;", 4) == 0) {
            // LATIN CAPITAL LETTER E WITH CARON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x9a;
            *spos += 3;
        } else if (strncmp(src + *spos, "283;", 4) == 0) {
            // LATIN SMALL LETTER E WITH CARON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x9b;
            *spos += 3;
        } else if (strncmp(src + *spos, "284;", 4) == 0) {
            // LATIN CAPITAL LETTER G WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x9c;
            *spos += 3;
        } else if (strncmp(src + *spos, "285;", 4) == 0) {
            // LATIN SMALL LETTER G WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x9d;
            *spos += 3;
        } else if (strncmp(src + *spos, "286;", 4) == 0) {
            // LATIN CAPITAL LETTER G WITH BREVE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x9e;
            *spos += 3;
        } else if (strncmp(src + *spos, "287;", 4) == 0) {
            // LATIN SMALL LETTER G WITH BREVE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0x9f;
            *spos += 3;
        } else if (strncmp(src + *spos, "288;", 4) == 0) {
            // LATIN CAPITAL LETTER G WITH DOT ABOVE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xa0;
            *spos += 3;
        } else if (strncmp(src + *spos, "289;", 4) == 0) {
            // LATIN SMALL LETTER G WITH DOT ABOVE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xa1;
            *spos += 3;
        } else if (strncmp(src + *spos, "290;", 4) == 0) {
            // LATIN CAPITAL LETTER K WITH CEDILLA
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xa2;
            *spos += 3;
        } else if (strncmp(src + *spos, "291;", 4) == 0) {
            // LATIN SMALL LETTER K WITH CEDILLA
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xa3;
            *spos += 3;
        } else if (strncmp(src + *spos, "292;", 4) == 0) {
            // LATIN CAPITAL LETTER H WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xa4;
            *spos += 3;
        } else if (strncmp(src + *spos, "293;", 4) == 0) {
            // LATIN SMALL LETTER H WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xa5;
            *spos += 3;
        } else if (strncmp(src + *spos, "294;", 4) == 0) {
            // LATIN CAPITAL LETTER H WITH STROKE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xa6;
            *spos += 3;
        } else if (strncmp(src + *spos, "295;", 4) == 0) {
            // LATIN SMALL LETTER H WITH STROKE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xa7;
            *spos += 3;
        } else if (strncmp(src + *spos, "296;", 4) == 0) {
            // LATIN CAPITAL LETTER I WITH TILDE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xa8;
            *spos += 3;
        } else if (strncmp(src + *spos, "297;", 4) == 0) {
            // LATIN SMALL LETTER I WITH TILDE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xa9;
            *spos += 3;
        } else if (strncmp(src + *spos, "298;", 4) == 0) {
            // LATIN CAPITAL LETTER I WITH MACRON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xaa;
            *spos += 3;
        } else if (strncmp(src + *spos, "299;", 4) == 0) {
            // LATIN SMALL LETTER I WITH MACRON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xab;
            *spos += 3;
        } else if (strncmp(src + *spos, "302;", 4) == 0) {
            // LATIN CAPITAL LETTER I WITH OGONEK
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xae;
            *spos += 3;
        } else if (strncmp(src + *spos, "303;", 4) == 0) {
            // LATIN SMALL LETTER I WITH OGONEK
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xaf;
            *spos += 3;
        } else if (strncmp(src + *spos, "304;", 4) == 0) {
            // LATIN CAPITAL LETTER I WITH DOT ABOVE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xb0;
            *spos += 3;
        } else if (strncmp(src + *spos, "305;", 4) == 0) {
            // LATIN SMALL LETTER DOTLESS I
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xb1;
            *spos += 3;
        } else if (strncmp(src + *spos, "308;", 4) == 0) {
            // LATIN CAPITAL LETTER J WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xb4;
            *spos += 3;
        } else if (strncmp(src + *spos, "309;", 4) == 0) {
            // LATIN SMALL LETTER J WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xb5;
            *spos += 3;
        } else if (strncmp(src + *spos, "310;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH CEDILLA
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xb6;
            *spos += 3;
        } else if (strncmp(src + *spos, "311;", 4) == 0) {
            // LATIN SMALL LETTER L WITH CEDILLA
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xb7;
            *spos += 3;
        } else if (strncmp(src + *spos, "312;", 4) == 0) {
            // LATIN SMALL LETTER KRA
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xb8;
            *spos += 3;
        } else if (strncmp(src + *spos, "313;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH ACUTE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xb9;
            *spos += 3;
        } else if (strncmp(src + *spos, "314;", 4) == 0) {
            // LATIN SMALL LETTER L WITH ACUTE
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xba;
            *spos += 3;
        } else if (strncmp(src + *spos, "315;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH CEDILLA
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xbb;
            *spos += 3;
        } else if (strncmp(src + *spos, "316;", 4) == 0) {
            // LATIN SMALL LETTER L WITH CEDILLA
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xbc;
            *spos += 3;
        } else if (strncmp(src + *spos, "317;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH CARON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xbd;
            *spos += 3;
        } else if (strncmp(src + *spos, "318;", 4) == 0) {
            // LATIN SMALL LETTER L WITH CARON
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xbe;
            *spos += 3;
        } else if (strncmp(src + *spos, "319;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH MIDDLE DOT
            target[(*tpos)++] = 0xc4;
            target[(*tpos)++] = 0xbf;
            *spos += 3;
        } else if (strncmp(src + *spos, "320;", 4) == 0) {
            // LATIN SMALL LETTER L WITH MIDDLE DOT
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x80;
            *spos += 3;
        } else if (strncmp(src + *spos, "321;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH STROKE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x81;
            *spos += 3;
        } else if (strncmp(src + *spos, "322;", 4) == 0) {
            // LATIN SMALL LETTER L WITH STROKE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x82;
            *spos += 3;
        } else if (strncmp(src + *spos, "323;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH ACUTE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x83;
            *spos += 3;
        } else if (strncmp(src + *spos, "324;", 4) == 0) {
            // LATIN SMALL LETTER L WITH ACUTE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x84;
            *spos += 3;
        } else if (strncmp(src + *spos, "325;", 4) == 0) {
            // LATIN CAPITAL LETTER N WITH CEDILLA
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x85;
            *spos += 3;
        } else if (strncmp(src + *spos, "326;", 4) == 0) {
            // LATIN SMALL LETTER N WITH CEDILLA
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x86;
            *spos += 3;
        } else if (strncmp(src + *spos, "327;", 4) == 0) {
            // LATIN CAPITAL LETTER N WITH ACUTE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x87;
            *spos += 3;
        } else if (strncmp(src + *spos, "328;", 4) == 0) {
            // LATIN SMALL LETTER N WITH ACUTE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x88;
            *spos += 3;
        } else if (strncmp(src + *spos, "330;", 4) == 0) {
            // LATIN CAPITAL LETTER ENG
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x8a;
            *spos += 3;
        } else if (strncmp(src + *spos, "331;", 4) == 0) {
            // LATIN SMALL LETTER ENG
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x8b;
            *spos += 3;
        } else if (strncmp(src + *spos, "332;", 4) == 0) {
            // LATIN CAPITAL O LETTER WITH MACRON
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x8c;
            *spos += 3;
        } else if (strncmp(src + *spos, "333;", 4) == 0) {
            // LATIN SMALL O LETTER WITH MACRON
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x8d;
            *spos += 3;
        } else if (strncmp(src + *spos, "336;", 4) == 0) {
            // LATIN CAPITAL LETTER O WITH DOUBLE ACUTE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x90;
            *spos += 3;
        } else if (strncmp(src + *spos, "337;", 4) == 0) {
            // LATIN SMALL LETTER O WITH DOUBLE ACUTE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x91;
            *spos += 3;
        } else if (strncmp(src + *spos, "342;", 4) == 0) {
            // LATIN CAPITAL LETTER R WITH CEDILLA
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x96;
            *spos += 3;
        } else if (strncmp(src + *spos, "343;", 4) == 0) {
            // LATIN SMALL LETTER R WITH CEDILLA
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x97;
            *spos += 3;
        } else if (strncmp(src + *spos, "344;", 4) == 0) {
            // LATIN CAPITAL LETTER R WITH CARON
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x98;
            *spos += 3;
        } else if (strncmp(src + *spos, "345;", 4) == 0) {
            // LATIN SMALL LETTER R WITH CARON
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x99;
            *spos += 3;
        } else if (strncmp(src + *spos, "346;", 4) == 0) {
            // LATIN CAPITAL LETTER S WITH ACUTE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x9a;
            *spos += 3;
        } else if (strncmp(src + *spos, "347;", 4) == 0) {
            // LATIN SMALL LETTER S WITH ACUTE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x9b;
            *spos += 3;
        } else if (strncmp(src + *spos, "348;", 4) == 0) {
            // LATIN CAPITAL LETTER S WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x9c;
            *spos += 3;
        } else if (strncmp(src + *spos, "349;", 4) == 0) {
            // LATIN SMALL LETTER S WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x9d;
            *spos += 3;
        } else if (strncmp(src + *spos, "350;", 4) == 0) {
            // LATIN CAPITAL LETTER S WITH CEDILLA
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x9e;
            *spos += 3;
        } else if (strncmp(src + *spos, "351;", 4) == 0) {
            // LATIN SMALL LETTER S WITH CEDILLA
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x9f;
            *spos += 3;
        } else if (strncmp(src + *spos, "356;", 4) == 0) {
            // LATIN CAPITAL LETTER T WITH CARON
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xa4;
            *spos += 3;
        } else if (strncmp(src + *spos, "357;", 4) == 0) {
            // LATIN SMALL LETTER T WITH CARON
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xa5;
            *spos += 3;
        } else if (strncmp(src + *spos, "358;", 4) == 0) {
            // LATIN CAPITAL LETTER T WITH STROKE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xa6;
            *spos += 3;
        } else if (strncmp(src + *spos, "359;", 4) == 0) {
            // LATIN SMALL LETTER T WITH STROKE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xa7;
            *spos += 3;
        } else if (strncmp(src + *spos, "360;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH TILDE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xa8;
            *spos += 3;
        } else if (strncmp(src + *spos, "361;", 4) == 0) {
            // LATIN SMALL LETTER U WITH TILDE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xa9;
            *spos += 3;
        } else if (strncmp(src + *spos, "362;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH MACRON
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xaa;
            *spos += 3;
        } else if (strncmp(src + *spos, "363;", 4) == 0) {
            // LATIN SMALL LETTER U WITH MACRON
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xab;
            *spos += 3;
        } else if (strncmp(src + *spos, "364;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH BREVE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xac;
            *spos += 3;
        } else if (strncmp(src + *spos, "365;", 4) == 0) {
            // LATIN SMALL LETTER U WITH BREVE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xad;
            *spos += 3;
        } else if (strncmp(src + *spos, "366;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH RING ABOVE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xae;
            *spos += 3;
        } else if (strncmp(src + *spos, "367;", 4) == 0) {
            // LATIN SMALL LETTER U WITH RING ABOVE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xaf;
            *spos += 3;
        } else if (strncmp(src + *spos, "368;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH DOUBLE ACUTE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xb0;
            *spos += 3;
        } else if (strncmp(src + *spos, "369;", 4) == 0) {
            // LATIN SMALL LETTER U WITH DOUBLE ACUTE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xb1;
            *spos += 3;
        } else if (strncmp(src + *spos, "370;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH OGONEK
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xb2;
            *spos += 3;
        } else if (strncmp(src + *spos, "371;", 4) == 0) {
            // LATIN SMALL LETTER U WITH OGONEK
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xb3;
            *spos += 3;
        } else if (strncmp(src + *spos, "372;", 4) == 0) {
            // LATIN CAPITAL LETTER W WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xb4;
            *spos += 3;
        } else if (strncmp(src + *spos, "373;", 4) == 0) {
            // LATIN SMALL LETTER W WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xb5;
            *spos += 3;
        } else if (strncmp(src + *spos, "374;", 4) == 0) {
            // LATIN CAPITAL LETTER Y WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xb6;
            *spos += 3;
        } else if (strncmp(src + *spos, "375;", 4) == 0) {
            // LATIN SMALL LETTER Y WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xb7;
            *spos += 3;
        } else if (strncmp(src + *spos, "377;", 4) == 0) {
            // LATIN CAPITAL LETTER Z WITH ACUTE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xb9;
            *spos += 3;
        } else if (strncmp(src + *spos, "378;", 4) == 0) {
            // LATIN SMALL LETTER Z WITH ACUTE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xba;
            *spos += 3;
        } else if (strncmp(src + *spos, "379;", 4) == 0) {
            // LATIN CAPITAL LETTER Z WITH DOT ABOVE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xbb;
            *spos += 3;
        } else if (strncmp(src + *spos, "380;", 4) == 0) {
            // LATIN SMALL LETTER Z WITH DOT ABOVE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xbc;
            *spos += 3;
        } else if (strncmp(src + *spos, "381;", 4) == 0) {
            // LATIN CAPITAL LETTER Z WITH CARON
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xbd;
            *spos += 3;
        } else if (strncmp(src + *spos, "382;", 4) == 0) {
            // LATIN SMALL LETTER Z WITH CARON
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xbe;
            *spos += 3;
        } else if (strncmp(src + *spos, "538;", 4) == 0) {
            // LATIN CAPITAL LETTER T WITH COMMA BELOW
            target[(*tpos)++] = 0xc8;
            target[(*tpos)++] = 0x9a;
            *spos += 3;
        } else if (strncmp(src + *spos, "539;", 4) == 0) {
            // LATIN SMALL LETTER T WITH COMMA BELOW
            target[(*tpos)++] = 0xc8;
            target[(*tpos)++] = 0x9b;
            *spos += 3;
        } else if (strncmp(src + *spos, "552;", 4) == 0) {
            // LATIN CAPITAL LETTER E WITH CEDILLA
            target[(*tpos)++] = 0xc8;
            target[(*tpos)++] = 0xa8;
            *spos += 3;
        } else if (strncmp(src + *spos, "553;", 4) == 0) {
            // LATIN SMALL LETTER E WITH CEDILLA
            target[(*tpos)++] = 0xc8;
            target[(*tpos)++] = 0xa9;
            *spos += 3;
        } else if (strncmp(src + *spos, "9834;", 5) == 0) {
            // EIGHTH NOTE
            target[(*tpos)++] = 0xe2;
            target[(*tpos)++] = 0x99;
            target[(*tpos)++] = 0xaa;
            *spos += 4;
        } else if (strncmp(src + *spos, "8486;", 5) == 0) {
            // OHM SIGN
            target[(*tpos)++] = 0xe2;
            target[(*tpos)++] = 0x84;
            target[(*tpos)++] = 0xa6;
            *spos += 4;
        } else if (strncmp(src + *spos, "9608;", 5) == 0) {
            // FULL BLOCK
            target[(*tpos)++] = 0xe2;
            target[(*tpos)++] = 0x96;
            target[(*tpos)++] = 0x88;
            *spos += 4;
        }
        break;
    case 'a':
        if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN SMALL LETTER A WITH DIAERESIS
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xa4;
            *spos += 3;
        } else if (strncmp(src + *spos, "tilde;", 6) == 0) {
            // LATIN SMALL LETTER A WITH TILDE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xa3;
            *spos += 5;
        } else if (strncmp(src + *spos, "ring;", 5) == 0) {
            // LATIN SMALL LETTER A WITH RING ABOVE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xa5;
            *spos += 4;
        } else if (strncmp(src + *spos, "mp;", 3) == 0) {
            // AMPERSAND
            target[(*tpos)++] = '&';
            *spos += 2;
        } else if (strncmp(src + *spos, "pos;", 4) == 0) {
            // APOSTROPHE
            target[(*tpos)++] = '\'';
            *spos += 3;
        } else if (strncmp(src + *spos, "lpha;", 5) == 0) {
            // LATIN SMALL LETTER ALPHA
            target[(*tpos)++] = 0xc9;
            target[(*tpos)++] = 0x91;
            *spos += 4;
        } else if (strncmp(src + *spos, "elig;", 5) == 0) {
            // LATIN SMALL LETTER AE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xa6;
            *spos += 4;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN SMALL LETTER A WITH ACUTE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xa1;
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN SMALL LETTER A WITH GRAVE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xa0;
            *spos += 5;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN SMALL LETTER A WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xa2;
            *spos += 4;
        }
        break;
    case 'A':
        if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN CAPITAL LETTER A WITH DIAERESIS
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x84;
            *spos += 3;
        } else if (strncmp(src + *spos, "tilde;", 6) == 0) {
            // LATIN CAPITAL LETTER A WITH TILDE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x83;
            *spos += 5;
        } else if (strncmp(src + *spos, "ring;", 5) == 0) {
            // LATIN CAPITAL LETTER A WITH RING ABOVE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x85;
            *spos += 4;
        } else if (strncmp(src + *spos, "Elig;", 5) == 0) {
            // LATIN CAPITAL LETTER AE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x86;
            *spos += 4;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN CAPITAL LETTER A WITH ACUTE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x81;
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN CAPITAL LETTER A WITH GRAVE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x80;
            *spos += 5;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN CAPITAL LETTER A WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x82;
            *spos += 4;
        }
        break;
    case 'c':
        if (strncmp(src + *spos, "urren;", 6) == 0) {
            // CURRENCY SIGN
            target[(*tpos)++] = 0xc2;
            target[(*tpos)++] = 0xa4;
            *spos += 5;
        } else if (strncmp(src + *spos, "opy;", 4) == 0) {
            // COPYRIGHT SIGN
            target[(*tpos)++] = 0xc2;
            target[(*tpos)++] = 0xa9;
            *spos += 3;
        } else if (strncmp(src + *spos, "cedil;", 6) == 0) {
            // LATIN SMALL LETTER C WITH CEDILLA
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xa7;
            *spos += 5;
        }
        break;
    case 'C':
        if (strncmp(src + *spos, "cedil;", 6) == 0) {
            // LATIN CAPITAL LETTER C WITH CEDILLA
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x87;
            *spos += 5;
        }
        break;
    case 'd':
        if (strncmp(src + *spos, "eg;", 3) == 0) {
            // DEGREE SIGN
            target[(*tpos)++] = 0xc2;
            target[(*tpos)++] = 0xb0;
            *spos += 2;
        } else if (strncmp(src + *spos, "arr;", 4) == 0) {
            // DOWNWARDS ARROW
            target[(*tpos)++] = 0xe2;
            target[(*tpos)++] = 0x86;
            target[(*tpos)++] = 0x93;
            *spos += 3;
        }
        break;
    case 'e':
        if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN SMALL LETTER E WITH ACUTE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xa9;
            *spos += 5;
        } else if (strncmp(src + *spos, "uro;", 4) == 0) {
            // EURO SIGN
            target[(*tpos)++] = 0xe2;
            target[(*tpos)++] = 0x82;
            target[(*tpos)++] = 0xac;
            *spos += 3;
        } else if (strncmp(src + *spos, "th;", 3) == 0) {
            // LATIN SMALL LETTER ETH
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xb0;
            *spos += 2;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN SMALL LETTER E WITH GRAVE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xa8;
            *spos += 5;
        } else if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN SMALL LETTER E WITH DIAERESIS
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xab;
            *spos += 3;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN SMALL LETTER E WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xaa;
            *spos += 4;
        }
        break;
    case 'E':
        if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN CAPITAL LETTER E WITH ACUTE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x89;
            *spos += 5;
        } else if (strncmp(src + *spos, "TH;", 3) == 0) {
            // LATIN CAPITAL LETTER ETH
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x90;
            *spos += 2;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN CAPITAL LETTER E WITH GRAVE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x88;
            *spos += 5;
        } else if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN CAPITAL LETTER E WITH DIAERESIS
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x8b;
            *spos += 3;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN CAPITAL LETTER E WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x8a;
            *spos += 4;
        }
        break;
    case 'f':
        if (strncmp(src + *spos, "rac12;", 6) == 0) {
            // VULGAR FRACTION ONE HALF
            target[(*tpos)++] = 0xc2;
            target[(*tpos)++] = 0xbd;
            *spos += 5;
        }
        break;
    case 'g':
        if (strncmp(src + *spos, "t;", 2) == 0) {
            // GREATER-THAN SIGN
            target[(*tpos)++] = '>';
            *spos += 1;
        }
        break;
    case 'i':
        if (strncmp(src + *spos, "excl;", 5) == 0) {
            // INVERTED EXCLAMATION MARK
            target[(*tpos)++] = 0xc2;
            target[(*tpos)++] = 0xa1;
            *spos += 4;
        } else if (strncmp(src + *spos, "quest;", 6) == 0) {
            // INVERTED QUESTION MARK
            target[(*tpos)++] = 0xc2;
            target[(*tpos)++] = 0xbf;
            *spos += 5;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN SMALL LETTER I WITH ACUTE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xad;
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN SMALL LETTER I WITH GRAVE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xac;
            *spos += 5;
        } else if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN SMALL LETTER I WITH DIAERESIS
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xaf;
            *spos += 3;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN SMALL LETTER I WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xae;
            *spos += 4;
        }
        break;
    case 'I':
        if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN CAPITAL LETTER I WITH ACUTE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x8d;
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN CAPITAL LETTER I WITH GRAVE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x8c;
            *spos += 5;
        } else if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN CAPITAL LETTER I WITH DIAERESIS
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x8f;
            *spos += 3;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN CAPITAL LETTER I WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x8e;
            *spos += 4;
        }
        break;
    case 'l':
        if (strncmp(src + *spos, "t;", 2) == 0) {
            // LESS-THAN SIGN
            target[(*tpos)++] = '<';
            *spos += 1;
        } else if (strncmp(src + *spos, "aquo;", 5) == 0) {
            // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
            target[(*tpos)++] = 0xc2;
            target[(*tpos)++] = 0xab;
            *spos += 4;
        } else if (strncmp(src + *spos, "arr;", 4) == 0) {
            // LEFTWARDS ARROW
            target[(*tpos)++] = 0xe2;
            target[(*tpos)++] = 0x86;
            target[(*tpos)++] = 0x90;
            *spos += 3;
        }
        break;
    case 'm':
        if (strncmp(src + *spos, "iddot;", 6) == 0) {
            // MIDDLE DOT
            target[(*tpos)++] = 0xc2;
            target[(*tpos)++] = 0xb7;
            *spos += 5;
        }
        break;
    case 'n':
        if (strncmp(src + *spos, "dash;", 5) == 0) {
            // EN DASH
            target[(*tpos)++] = 0xe2;
            target[(*tpos)++] = 0x80;
            target[(*tpos)++] = 0x93;
            *spos += 4;
        } else if (strncmp(src + *spos, "tilde;", 6) == 0) {
            // LATIN SMALL LETTER N WITH TILDE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xb1;
            *spos += 5;
        }
        break;
    case 'N':
        if (strncmp(src + *spos, "tilde;", 6) == 0) {
            // LATIN CAPITAL LETTER N WITH TILDE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x91;
            *spos += 5;
        }
        break;
    case 'o':
        if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN SMALL LETTER A WITH DIAERESIS
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xb6;
            *spos += 3;
        } else if (strncmp(src + *spos, "slash;", 6) == 0) {
            // LATIN SMALL LETTER O WITH STROKE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xb8;
            *spos += 5;
        } else if (strncmp(src + *spos, "elig;", 5) == 0) {
            // LATIN SMALL LIGATURE OE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x93;
            *spos += 4;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN SMALL LETTER O WITH ACUTE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xb3;
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN SMALL LETTER O WITH GRAVE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xb2;
            *spos += 5;
        } else if (strncmp(src + *spos, "tilde;", 6) == 0) {
            // LATIN SMALL LETTER O WITH TILDE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xb5;
            *spos += 5;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN SMALL LETTER O WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xb4;
            *spos += 4;
        }
        break;
    case 'O':
        if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN CAPITAL LETTER O WITH DIAERESIS
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x96;
            *spos += 3;
        } else if (strncmp(src + *spos, "slash;", 6) == 0) {
            // LATIN CAPITAL LETTER O WITH STROKE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x98;
            *spos += 5;
        } else if (strncmp(src + *spos, "Elig;", 5) == 0) {
            // LATIN CAPITAL LIGATURE OE
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0x92;
            *spos += 4;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN CAPITAL LETTER O WITH ACUTE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x93;
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN CAPITAL LETTER O WITH GRAVE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x92;
            *spos += 5;
        } else if (strncmp(src + *spos, "tilde;", 6) == 0) {
            // LATIN CAPITAL LETTER O WITH TILDE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x95;
            *spos += 5;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN CAPITAL LETTER O WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x94;
            *spos += 4;
        }
        break;
    case 'p':
        if (strncmp(src + *spos, "ound;", 5) == 0) {
            // POUND SIGN
            target[(*tpos)++] = 0xc2;
            target[(*tpos)++] = 0xa3;
            *spos += 4;
        } else if (strncmp(src + *spos, "lusmn;", 6) == 0) {
            // PLUS-MINUS SIGN
            target[(*tpos)++] = 0xc2;
            target[(*tpos)++] = 0xb1;
            *spos += 5;
        } else if (strncmp(src + *spos, "ermil;", 6) == 0) {
            // PER MILLE SIGN
            target[(*tpos)++] = 0xe2;
            target[(*tpos)++] = 0x80;
            target[(*tpos)++] = 0xb0;
            *spos += 5;
        }
        break;
    case 'q':
        if (strncmp(src + *spos, "uot;", 4) == 0) {
            // QUOTATION MARK
            target[(*tpos)++] = '"';
            *spos += 3;
        }
        break;
    case 'r':
        if (strncmp(src + *spos, "aquo;", 5) == 0) {
            // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
            target[(*tpos)++] = 0xc2;
            target[(*tpos)++] = 0xbb;
            *spos += 4;
        } else if (strncmp(src + *spos, "arr;", 4) == 0) {
            // RIGHTWARDS ARROW
            target[(*tpos)++] = 0xe2;
            target[(*tpos)++] = 0x86;
            target[(*tpos)++] = 0x92;
            *spos += 3;
        } else if (strncmp(src + *spos, "dquo;", 5) == 0) {
            // RIGHT DOUBLE QUOTATION MARK
            target[(*tpos)++] = 0xe2;
            target[(*tpos)++] = 0x80;
            target[(*tpos)++] = 0x9d;
            *spos += 4;
        }
        break;
    case 's':
        if (strncmp(src + *spos, "ect;", 4) == 0) {
            // SECTION SIGN
            target[(*tpos)++] = 0xc2;
            target[(*tpos)++] = 0xa7;
            *spos += 3;
        } else if (strncmp(src + *spos, "zlig;", 5) == 0) {
            // LATIN SMALL LETTER SHARP S
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x9f;
            *spos += 4;
        } else if (strncmp(src + *spos, "caron;", 6) == 0) {
            // LATIN SMALL LETTER S WITH CARON
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xa1;
            *spos += 5;
        } else if (strncmp(src + *spos, "up", 2) == 0 && *(src + (*spos + 3)) == ';') {
            // SUPERSCRIPT ONE|TWO|THREE
            target[(*tpos)++] = 0xc2;
            switch (*(src + (*spos + 2))) {
            case '1':
                target[(*tpos)++] = 0xb9;
                break;
            case '2':
                target[(*tpos)++] = 0xb2;
                break;
            case '3':
                target[(*tpos)++] = 0xb3;
                break;
            }
            *spos += 3;
        }
        break;
    case 'S':
        if (strncmp(src + *spos, "caron;", 6) == 0) {
            // LATIN CAPITAL LETTER S WITH CARON
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xa0;
            *spos += 5;
        }
        break;
    case 't':
        if (strncmp(src + *spos, "horn;", 5) == 0) {
            // LATIN SMALL LETTER THORN
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xbe;
            *spos += 4;
        }
        break;
    case 'T':
        if (strncmp(src + *spos, "HORN;", 5) == 0) {
            // LATIN CAPITAL LETTER THORN
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x9e;
            *spos += 4;
        }
        break;
    case 'u':
        if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN SMALL LETTER U WITH DIAERESIS
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xbc;
            *spos += 3;
        } else if (strncmp(src + *spos, "arr;", 4) == 0) {
            // UPWARDS ARROW
            target[(*tpos)++] = 0xe2;
            target[(*tpos)++] = 0x86;
            target[(*tpos)++] = 0x91;
            *spos += 3;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN SMALL LETTER U WITH ACUTE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xba;
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN SMALL LETTER U WITH GRAVE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xb9;
            *spos += 5;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN SMALL LETTER U WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xbb;
            *spos += 4;
        }
        break;
    case 'U':
        if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH DIAERESIS
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x9c;
            *spos += 3;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN CAPITAL LETTER U WITH ACUTE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x9a;
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN CAPITAL LETTER U WITH GRAVE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x99;
            *spos += 5;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN CAPITAL LETTER U WITH CIRCUMFLEX
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x9b;
            *spos += 4;
        }
        break;
    case 'y':
        if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN SMALL LETTER Y WITH ACUTE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xbd;
            *spos += 5;
        } else if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN SMALL LETTER Y WITH DIAERESIS
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0xbf;
            *spos += 3;
        }
        break;
    case 'Y':
        if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN CAPITAL LETTER Y WITH ACUTE
            target[(*tpos)++] = 0xc3;
            target[(*tpos)++] = 0x9d;
            *spos += 5;
        } else if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN CAPITAL LETTER Y WITH DIAERESIS
            target[(*tpos)++] = 0xc5;
            target[(*tpos)++] = 0xb8;
            *spos += 3;
        }
        break;
    default:
        // Nothing was found so return to original position
        *spos -= 1;
        break;
    }
}
#else
static void copy_html_character_ascii(unsigned char* target, char* src, size_t* tpos, size_t* spos)
{
    *spos += 1;
    switch (*(src + (*spos - 1))) {
    case '#':
        if (strncmp(src + *spos, "256;", 4) == 0) {
            // LATIN CAPITAL LETTER A WITH MACRON
            target[(*tpos)++] = 'A';
            *spos += 3;
        } else if (strncmp(src + *spos, "257;", 4) == 0) {
            // LATIN SMALL LETTER A WITH MACRON
            target[(*tpos)++] = 'a';
            *spos += 3;
        } else if (strncmp(src + *spos, "260;", 4) == 0) {
            // LATIN CAPITAL LETTER A WITH OGONEK
            target[(*tpos)++] = 'A';
            *spos += 3;
        } else if (strncmp(src + *spos, "261;", 4) == 0) {
            // LATIN SMALL LETTER A WITH OGONEK
            target[(*tpos)++] = 'a';
            *spos += 3;
        } else if (strncmp(src + *spos, "263;", 4) == 0) {
            // LATIN SMALL LETTER C WITH ACUTE
            target[(*tpos)++] = 'c';
            *spos += 3;
        } else if (strncmp(src + *spos, "264;", 4) == 0) {
            // LATIN CAPITAL LETTER C WITH CIRCUMFLEX
            target[(*tpos)++] = 'C';
            *spos += 3;
        } else if (strncmp(src + *spos, "265;", 4) == 0) {
            // LATIN SMALL LETTER C WITH CIRCUMFLEX
            target[(*tpos)++] = 'c';
            *spos += 3;
        } else if (strncmp(src + *spos, "266;", 4) == 0) {
            // LATIN CAPITAL LETTER C WITH DOT ABOVE
            target[(*tpos)++] = 'C';
            *spos += 3;
        } else if (strncmp(src + *spos, "267;", 4) == 0) {
            // LATIN SMALL LETTER C WITH DOT ABOVE
            target[(*tpos)++] = 'c';
            *spos += 3;
        } else if (strncmp(src + *spos, "268;", 4) == 0) {
            // LATIN CAPITAL LETTER C WITH CARON
            target[(*tpos)++] = 'C';
            *spos += 3;
        } else if (strncmp(src + *spos, "269;", 4) == 0) {
            // LATIN SMALL LETTER C WITH CARON
            target[(*tpos)++] = 'c';
            *spos += 3;
        } else if (strncmp(src + *spos, "270;", 4) == 0) {
            // LATIN CAPITAL LETTER D WITH CARON
            target[(*tpos)++] = 'D';
            *spos += 3;
        } else if (strncmp(src + *spos, "271;", 4) == 0) {
            // LATIN SMALL LETTER D WITH CARON
            target[(*tpos)++] = 'd';
            *spos += 3;
        } else if (strncmp(src + *spos, "273;", 4) == 0) {
            // LATIN SMALL LETTER D WITH STROKE
            target[(*tpos)++] = 'd';
            *spos += 3;
        } else if (strncmp(src + *spos, "274;", 4) == 0) {
            // LATIN CAPITAL LETTER E WITH MACRON
            target[(*tpos)++] = 'E';
            *spos += 3;
        } else if (strncmp(src + *spos, "275;", 4) == 0) {
            // LATIN SMALL LETTER E WITH MACRON
            target[(*tpos)++] = 'e';
            *spos += 3;
        } else if (strncmp(src + *spos, "278;", 4) == 0) {
            // LATIN CAPITAL LETTER E WITH DOT ABOVE
            target[(*tpos)++] = 'E';
            *spos += 3;
        } else if (strncmp(src + *spos, "279;", 4) == 0) {
            // LATIN SMALL LETTER E WITH DOT ABOVE
            target[(*tpos)++] = 'e';
            *spos += 3;
        } else if (strncmp(src + *spos, "280;", 4) == 0) {
            // LATIN CAPITAL LETTER E WITH OGONEK
            target[(*tpos)++] = 'E';
            *spos += 3;
        } else if (strncmp(src + *spos, "281;", 4) == 0) {
            // LATIN SMALL LETTER E WITH OGONEK
            target[(*tpos)++] = 'e';
            *spos += 3;
        } else if (strncmp(src + *spos, "282;", 4) == 0) {
            // LATIN CAPITAL LETTER E WITH CARON
            target[(*tpos)++] = 'E';
            *spos += 3;
        } else if (strncmp(src + *spos, "283;", 4) == 0) {
            // LATIN SMALL LETTER E WITH CARON
            target[(*tpos)++] = 'e';
            *spos += 3;
        } else if (strncmp(src + *spos, "284;", 4) == 0) {
            // LATIN CAPITAL LETTER G WITH CIRCUMFLEX
            target[(*tpos)++] = 'G';
            *spos += 3;
        } else if (strncmp(src + *spos, "285;", 4) == 0) {
            // LATIN SMALL LETTER G WITH CIRCUMFLEX
            target[(*tpos)++] = 'g';
            *spos += 3;
        } else if (strncmp(src + *spos, "286;", 4) == 0) {
            // LATIN CAPITAL LETTER G WITH BREVE
            target[(*tpos)++] = 'G';
            *spos += 3;
        } else if (strncmp(src + *spos, "287;", 4) == 0) {
            // LATIN SMALL LETTER G WITH BREVE
            target[(*tpos)++] = 'g';
            *spos += 3;
        } else if (strncmp(src + *spos, "288;", 4) == 0) {
            // LATIN CAPITAL LETTER G WITH DOT ABOVE
            target[(*tpos)++] = 'G';
            *spos += 3;
        } else if (strncmp(src + *spos, "289;", 4) == 0) {
            // LATIN SMALL LETTER G WITH DOT ABOVE
            target[(*tpos)++] = 'g';
            *spos += 3;
        } else if (strncmp(src + *spos, "290;", 4) == 0) {
            // LATIN CAPITAL LETTER K WITH CEDILLA
            target[(*tpos)++] = 'K';
            *spos += 3;
        } else if (strncmp(src + *spos, "291;", 4) == 0) {
            // LATIN SMALL LETTER K WITH CEDILLA
            target[(*tpos)++] = 'k';
            *spos += 3;
        } else if (strncmp(src + *spos, "292;", 4) == 0) {
            // LATIN CAPITAL LETTER H WITH CIRCUMFLEX
            target[(*tpos)++] = 'H';
            *spos += 3;
        } else if (strncmp(src + *spos, "293;", 4) == 0) {
            // LATIN SMALL LETTER H WITH CIRCUMFLEX
            target[(*tpos)++] = 'h';
            *spos += 3;
        } else if (strncmp(src + *spos, "294;", 4) == 0) {
            // LATIN CAPITAL LETTER H WITH STROKE
            target[(*tpos)++] = 'H';
            *spos += 3;
        } else if (strncmp(src + *spos, "295;", 4) == 0) {
            // LATIN SMALL LETTER H WITH STROKE
            target[(*tpos)++] = 'h';
            *spos += 3;
        } else if (strncmp(src + *spos, "296;", 4) == 0) {
            // LATIN CAPITAL LETTER I WITH TILDE
            target[(*tpos)++] = 'I';
            *spos += 3;
        } else if (strncmp(src + *spos, "297;", 4) == 0) {
            // LATIN SMALL LETTER I WITH TILDE
            target[(*tpos)++] = 'i';
            *spos += 3;
        } else if (strncmp(src + *spos, "298;", 4) == 0) {
            // LATIN CAPITAL LETTER I WITH MACRON
            target[(*tpos)++] = 'I';
            *spos += 3;
        } else if (strncmp(src + *spos, "299;", 4) == 0) {
            // LATIN CAPITAL LETTER I WITH MACRON
            target[(*tpos)++] = 'i';
            *spos += 3;
        } else if (strncmp(src + *spos, "302;", 4) == 0) {
            // LATIN CAPITAL LETTER I WITH OGONEK
            target[(*tpos)++] = 'I';
            *spos += 3;
        } else if (strncmp(src + *spos, "303;", 4) == 0) {
            // LATIN SMALL LETTER I WITH OGONEK
            target[(*tpos)++] = 'i';
            *spos += 3;
        } else if (strncmp(src + *spos, "304;", 4) == 0) {
            // LATIN CAPITAL LETTER I WITH DOT ABOVE
            target[(*tpos)++] = 'I';
            *spos += 3;
        } else if (strncmp(src + *spos, "305;", 4) == 0) {
            // LATIN SMALL LETTER DOTLESS I
            target[(*tpos)++] = 'i';
            *spos += 3;
        } else if (strncmp(src + *spos, "308;", 4) == 0) {
            // LATIN CAPITAL LETTER J WITH CIRCUMFLEX
            target[(*tpos)++] = 'J';
            *spos += 3;
        } else if (strncmp(src + *spos, "309;", 4) == 0) {
            // LATIN SMALL LETTER J WITH CIRCUMFLEX
            target[(*tpos)++] = 'j';
            *spos += 3;
        } else if (strncmp(src + *spos, "310;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH CEDILLA
            target[(*tpos)++] = 'L';
            *spos += 3;
        } else if (strncmp(src + *spos, "311;", 4) == 0) {
            // LATIN SMALL LETTER L WITH CEDILLA
            target[(*tpos)++] = 'l';
            *spos += 3;
        } else if (strncmp(src + *spos, "312;", 4) == 0) {
            // LATIN SMALL LETTER KRA
            target[(*tpos)++] = 'K';
            *spos += 3;
        } else if (strncmp(src + *spos, "313;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH ACUTE
            target[(*tpos)++] = 'L';
            *spos += 3;
        } else if (strncmp(src + *spos, "314;", 4) == 0) {
            // LATIN SMALL LETTER L WITH ACUTE
            target[(*tpos)++] = 'l';
            *spos += 3;
        } else if (strncmp(src + *spos, "315;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH CEDILLA
            target[(*tpos)++] = 'L';
            *spos += 3;
        } else if (strncmp(src + *spos, "316;", 4) == 0) {
            // LATIN SMALL LETTER L WITH CEDILLA
            target[(*tpos)++] = 'l';
            *spos += 3;
        } else if (strncmp(src + *spos, "317;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH CARON
            target[(*tpos)++] = 'L';
            *spos += 3;
        } else if (strncmp(src + *spos, "318;", 4) == 0) {
            // LATIN SMALL LETTER L WITH CARON
            target[(*tpos)++] = 'l';
            *spos += 3;
        } else if (strncmp(src + *spos, "319;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH MIDDLE DOT
            target[(*tpos)++] = 'L';
            *spos += 3;
        } else if (strncmp(src + *spos, "320;", 4) == 0) {
            // LATIN SMALL LETTER L WITH MIDDLE DOT
            target[(*tpos)++] = 'l';
            *spos += 3;

        } else if (strncmp(src + *spos, "321;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH STROKE
            target[(*tpos)++] = 'L';
            *spos += 3;
        } else if (strncmp(src + *spos, "322;", 4) == 0) {
            // LATIN SMALL LETTER L WITH STROKE
            target[(*tpos)++] = 'l';
            *spos += 3;
        } else if (strncmp(src + *spos, "323;", 4) == 0) {
            // LATIN CAPITAL LETTER L WITH ACUTE
            target[(*tpos)++] = 'L';
            *spos += 3;
        } else if (strncmp(src + *spos, "324;", 4) == 0) {
            // LATIN SMALL LETTER L WITH ACUTE
            target[(*tpos)++] = 'l';
            *spos += 3;
        } else if (strncmp(src + *spos, "325;", 4) == 0) {
            // LATIN CAPITAL LETTER N WITH CEDILLA
            target[(*tpos)++] = 'N';
            *spos += 3;
        } else if (strncmp(src + *spos, "326;", 4) == 0) {
            // LATIN SMALL LETTER N WITH CEDILLA
            target[(*tpos)++] = 'n';
            *spos += 3;
        } else if (strncmp(src + *spos, "327;", 4) == 0) {
            // LATIN CAPITAL LETTER N WITH ACUTE
            target[(*tpos)++] = 'N';
            *spos += 3;
        } else if (strncmp(src + *spos, "328;", 4) == 0) {
            // LATIN SMALL LETTER N WITH ACUTE
            target[(*tpos)++] = 'n';
            *spos += 3;
        } else if (strncmp(src + *spos, "330;", 4) == 0) {
            // LATIN CAPITAL LETTER ENG
            target[(*tpos)++] = 'N';
            *spos += 3;
        } else if (strncmp(src + *spos, "331;", 4) == 0) {
            // LATIN SMALL LETTER ENG
            target[(*tpos)++] = 'n';
            *spos += 3;
        } else if (strncmp(src + *spos, "332;", 4) == 0) {
            // LATIN CAPITAL O LETTER WITH MACRON
            target[(*tpos)++] = 'O';
            *spos += 3;
        } else if (strncmp(src + *spos, "333;", 4) == 0) {
            // LATIN SMALL O LETTER WITH MACRON
            target[(*tpos)++] = 'o';
            *spos += 3;
        } else if (strncmp(src + *spos, "336;", 4) == 0) {
            // LATIN CAPITAL LETTER O WITH DOUBLE ACUTE
            target[(*tpos)++] = 'O';
            *spos += 3;
        } else if (strncmp(src + *spos, "337;", 4) == 0) {
            // LATIN SMALL LETTER O WITH DOUBLE ACUTE
            target[(*tpos)++] = 'o';
            *spos += 3;
        } else if (strncmp(src + *spos, "342;", 4) == 0) {
            // LATIN CAPITAL LETTER R WITH CEDILLA
            target[(*tpos)++] = 'R';
            *spos += 3;
        } else if (strncmp(src + *spos, "343;", 4) == 0) {
            // LATIN SMALL LETTER R WITH CEDILLA
            target[(*tpos)++] = 'r';
            *spos += 3;
        } else if (strncmp(src + *spos, "344;", 4) == 0) {
            // LATIN CAPITAL LETTER R WITH CARON
            target[(*tpos)++] = 'R';
            *spos += 3;
        } else if (strncmp(src + *spos, "345;", 4) == 0) {
            // LATIN SMALL LETTER R WITH CARON
            target[(*tpos)++] = 'r';
            *spos += 3;
        } else if (strncmp(src + *spos, "346;", 4) == 0) {
            // LATIN CAPITAL LETTER S WITH ACUTE
            target[(*tpos)++] = 'S';
            *spos += 3;
        } else if (strncmp(src + *spos, "347;", 4) == 0) {
            // LATIN SMALL LETTER S WITH ACUTE
            target[(*tpos)++] = 's';
            *spos += 3;
        } else if (strncmp(src + *spos, "348;", 4) == 0) {
            // LATIN CAPITAL LETTER S WITH CIRCUMFLEX
            target[(*tpos)++] = 'S';
            *spos += 3;
        } else if (strncmp(src + *spos, "349;", 4) == 0) {
            // LATIN SMALL LETTER S WITH CIRCUMFLEX
            target[(*tpos)++] = 's';
            *spos += 3;
        } else if (strncmp(src + *spos, "350;", 4) == 0) {
            // LATIN CAPITAL LETTER S WITH CEDILLA
            target[(*tpos)++] = 'S';
            *spos += 3;
        } else if (strncmp(src + *spos, "351;", 4) == 0) {
            // LATIN SMALL LETTER S WITH CEDILLA
            target[(*tpos)++] = 's';
            *spos += 3;
        } else if (strncmp(src + *spos, "356;", 4) == 0) {
            // LATIN CAPITAL LETTER T WITH CARON
            target[(*tpos)++] = 'T';
            *spos += 3;
        } else if (strncmp(src + *spos, "357;", 4) == 0) {
            // LATIN SMALL LETTER T WITH CARON
            target[(*tpos)++] = 't';
            *spos += 3;
        } else if (strncmp(src + *spos, "358;", 4) == 0) {
            // LATIN CAPITAL LETTER T WITH STROKE
            target[(*tpos)++] = 'T';
            *spos += 3;
        } else if (strncmp(src + *spos, "359;", 4) == 0) {
            // LATIN SMALL LETTER T WITH STROKE
            target[(*tpos)++] = 't';
            *spos += 3;
        } else if (strncmp(src + *spos, "360;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH TILDE
            target[(*tpos)++] = 'U';
            *spos += 3;
        } else if (strncmp(src + *spos, "361;", 4) == 0) {
            // LATIN SMALL LETTER U WITH TILDE
            target[(*tpos)++] = 'u';
            *spos += 3;
        } else if (strncmp(src + *spos, "362;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH MACRON
            target[(*tpos)++] = 'U';
            *spos += 3;
        } else if (strncmp(src + *spos, "363;", 4) == 0) {
            // LATIN SMALL LETTER U WITH MACRON
            target[(*tpos)++] = 'u';
            *spos += 3;
        } else if (strncmp(src + *spos, "364;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH BREVE
            target[(*tpos)++] = 'U';
            *spos += 3;
        } else if (strncmp(src + *spos, "365;", 4) == 0) {
            // LATIN SMALL LETTER U WITH BREVE
            target[(*tpos)++] = 'u';
            *spos += 3;
        } else if (strncmp(src + *spos, "366;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH RING ABOVE
            target[(*tpos)++] = 'U';
            *spos += 3;
        } else if (strncmp(src + *spos, "367;", 4) == 0) {
            // LATIN SMALL LETTER U WITH RING ABOVE
            target[(*tpos)++] = 'u';
            *spos += 3;
        } else if (strncmp(src + *spos, "368;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH DOUBLE ACUTE
            target[(*tpos)++] = 'U';
            *spos += 3;
        } else if (strncmp(src + *spos, "369;", 4) == 0) {
            // LATIN SMALL LETTER U WITH DOUBLE ACUTE
            target[(*tpos)++] = 'u';
            *spos += 3;
        } else if (strncmp(src + *spos, "370;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH OGONEK
            target[(*tpos)++] = 'U';
            *spos += 3;
        } else if (strncmp(src + *spos, "371;", 4) == 0) {
            // LATIN SMALL LETTER U WITH OGONEK
            target[(*tpos)++] = 'u';
            *spos += 3;
        } else if (strncmp(src + *spos, "372;", 4) == 0) {
            // LATIN CAPITAL LETTER W WITH CIRCUMFLEX
            target[(*tpos)++] = 'W';
            *spos += 3;
        } else if (strncmp(src + *spos, "373;", 4) == 0) {
            // LATIN SMALL LETTER W WITH CIRCUMFLEX
            target[(*tpos)++] = 'w';
            *spos += 3;
        } else if (strncmp(src + *spos, "374;", 4) == 0) {
            // LATIN CAPITAL LETTER Y WITH CIRCUMFLEX
            target[(*tpos)++] = 'Y';
            *spos += 3;
        } else if (strncmp(src + *spos, "375;", 4) == 0) {
            // LATIN SMALL LETTER Y WITH CIRCUMFLEX
            target[(*tpos)++] = 'y';
            *spos += 3;
        } else if (strncmp(src + *spos, "377;", 4) == 0) {
            // LATIN CAPITAL LETTER Z WITH ACUTE
            target[(*tpos)++] = 'Z';
            *spos += 3;
        } else if (strncmp(src + *spos, "378;", 4) == 0) {
            // LATIN SMALL LETTER Z WITH ACUTE
            target[(*tpos)++] = 'z';
            *spos += 3;
        } else if (strncmp(src + *spos, "379;", 4) == 0) {
            // LATIN CAPITAL LETTER Z WITH DOT ABOVE
            target[(*tpos)++] = 'Z';
            *spos += 3;
        } else if (strncmp(src + *spos, "380;", 4) == 0) {
            // LATIN SMALL LETTER Z WITH DOT ABOVE
            target[(*tpos)++] = 'z';
            *spos += 3;
        } else if (strncmp(src + *spos, "381;", 4) == 0) {
            // LATIN CAPITAL LETTER Z WITH CARON
            target[(*tpos)++] = 'Z';
            *spos += 3;
        } else if (strncmp(src + *spos, "382;", 4) == 0) {
            // LATIN SMALL LETTER Z WITH CARON
            target[(*tpos)++] = 'z';
            *spos += 3;
        } else if (strncmp(src + *spos, "538;", 4) == 0) {
            // LATIN CAPITAL LETTER T WITH COMMA BELOW
            target[(*tpos)++] = 'T';
            *spos += 3;
        } else if (strncmp(src + *spos, "539;", 4) == 0) {
            // LATIN SMALL LETTER T WITH COMMA BELOW
            target[(*tpos)++] = 't';
            *spos += 3;
        } else if (strncmp(src + *spos, "552;", 4) == 0) {
            // LATIN CAPITAL LETTER E WITH CEDILLA
            target[(*tpos)++] = 'E';
            *spos += 3;
        } else if (strncmp(src + *spos, "553;", 4) == 0) {
            // LATIN SMALL LETTER E WITH CEDILLA
            target[(*tpos)++] = 'e';
            *spos += 3;
        } else if (strncmp(src + *spos, "9834;", 5) == 0) {
            // EIGHTH NOTE
            *spos += 4;
        } else if (strncmp(src + *spos, "8486;", 5) == 0) {
            // OHM SIGN
            target[(*tpos)++] = 'O';
            *spos += 4;
        } else if (strncmp(src + *spos, "9608;", 5) == 0) {
            // FULL BLOCK
            target[(*tpos)++] = ' ';
            *spos += 4;
        }
        break;
    case 'a':
        if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN SMALL LETTER A WITH DIAERESIS
            target[(*tpos)++] = 'a';
            *spos += 3;
        } else if (strncmp(src + *spos, "tilde;", 6) == 0) {
            // LATIN SMALL LETTER A WITH TILDE
            target[(*tpos)++] = 'a';
            *spos += 5;
        } else if (strncmp(src + *spos, "ring;", 5) == 0) {
            // LATIN SMALL LETTER A WITH RING ABOVE
            target[(*tpos)++] = 'a';
            *spos += 4;
        } else if (strncmp(src + *spos, "mp;", 3) == 0) {
            // AMPERSAND
            target[(*tpos)++] = '&';
            *spos += 2;
        } else if (strncmp(src + *spos, "pos;", 4) == 0) {
            // APOSTROPHE
            target[(*tpos)++] = '\'';
            *spos += 3;
        } else if (strncmp(src + *spos, "lpha;", 5) == 0) {
            // LATIN SMALL LETTER ALPHA
            target[(*tpos)++] = 'a';
            *spos += 4;
        } else if (strncmp(src + *spos, "elig;", 5) == 0) {
            // LATIN SMALL LETTER AE
            target[(*tpos)++] = 'a';
            *spos += 4;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN SMALL LETTER A WITH ACUTE
            target[(*tpos)++] = 'a';
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN SMALL LETTER A WITH GRAVE
            target[(*tpos)++] = 'a';
            *spos += 5;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN SMALL LETTER A WITH CIRCUMFLEX
            target[(*tpos)++] = 'a';
            *spos += 4;
        }
        break;
    case 'A':
        if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN CAPITAL LETTER A WITH DIAERESIS
            target[(*tpos)++] = 'A';
            *spos += 3;
        } else if (strncmp(src + *spos, "tilde;", 6) == 0) {
            // LATIN CAPITAL LETTER A WITH TILDE
            target[(*tpos)++] = 'A';
            *spos += 5;
        } else if (strncmp(src + *spos, "ring;", 5) == 0) {
            // LATIN CAPITAL LETTER A WITH RING ABOVE
            target[(*tpos)++] = 'A';
            *spos += 4;
        } else if (strncmp(src + *spos, "Elig;", 5) == 0) {
            // LATIN CAPITAL LETTER AE
            target[(*tpos)++] = 'A';
            *spos += 4;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN CAPITAL LETTER A WITH ACUTE
            target[(*tpos)++] = 'A';
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN CAPITAL LETTER A WITH GRAVE
            target[(*tpos)++] = 'A';
            *spos += 5;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN CAPITAL LETTER A WITH CIRCUMFLEX
            target[(*tpos)++] = 'A';
            *spos += 4;
        }
        break;
    case 'c':
        if (strncmp(src + *spos, "urren;", 6) == 0) {
            // CURRENCY SIGN
            target[(*tpos)++] = 'c';
            *spos += 5;
        } else if (strncmp(src + *spos, "opy;", 4) == 0) {
            // COPYRIGHT SIGN
            target[(*tpos)++] = 'c';
            *spos += 3;
        } else if (strncmp(src + *spos, "cedil;", 6) == 0) {
            // LATIN SMALL LETTER C WITH CEDILLA
            target[(*tpos)++] = 'c';
            *spos += 5;
        }
        break;
    case 'C':
        if (strncmp(src + *spos, "cedil;", 6) == 0) {
            // LATIN CAPITAL LETTER C WITH CEDILLA
            target[(*tpos)++] = 'C';
            *spos += 5;
        }
        break;
    case 'd':
        if (strncmp(src + *spos, "eg;", 3) == 0) {
            // DEGREE SIGN
            target[(*tpos)++] = 'o';
            *spos += 2;
        } else if (strncmp(src + *spos, "arr;", 4) == 0) {
            // DOWNWARDS ARROW
            target[(*tpos)++] = 'Y';
            *spos += 3;
        }
        break;
    case 'e':
        if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN SMALL LETTER E WITH ACUTE
            target[(*tpos)++] = 'e';
            *spos += 5;
        } else if (strncmp(src + *spos, "uro;", 4) == 0) {
            // EURO SIGN
            target[(*tpos)++] = 'e';
            *spos += 3;
        } else if (strncmp(src + *spos, "th;", 3) == 0) {
            // LATIN SMALL LETTER ETH
            target[(*tpos)++] = 'e';
            *spos += 2;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN SMALL LETTER E WITH GRAVE
            target[(*tpos)++] = 'e';
            *spos += 5;
        } else if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN SMALL LETTER E WITH DIAERESIS
            target[(*tpos)++] = 'e';
            *spos += 3;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN SMALL LETTER E WITH CIRCUMFLEX
            target[(*tpos)++] = 'e';
            *spos += 4;
        }
        break;
    case 'E':
        if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN CAPITAL LETTER E WITH ACUTE
            target[(*tpos)++] = 'E';
            *spos += 5;
        } else if (strncmp(src + *spos, "TH;", 3) == 0) {
            // LATIN CAPITAL LETTER ETH
            target[(*tpos)++] = 'E';
            *spos += 2;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN CAPITAL LETTER E WITH GRAVE
            target[(*tpos)++] = 'E';
            *spos += 5;
        } else if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN CAPITAL LETTER E WITH DIAERESIS
            target[(*tpos)++] = 'E';
            *spos += 3;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN CAPITAL LETTER E WITH CIRCUMFLEX
            target[(*tpos)++] = 'E';
            *spos += 4;
        }
        break;
    case 'f':
        if (strncmp(src + *spos, "rac12;", 6) == 0) {
            // VULGAR FRACTION ONE HALF
            target[(*tpos)++] = '1';
            target[(*tpos)++] = '/';
            target[(*tpos)++] = '2';
            *spos += 5;
        }
        break;
    case 'g':
        if (strncmp(src + *spos, "t;", 2) == 0) {
            // GREATER-THAN SIGN
            target[(*tpos)++] = '>';
            *spos += 1;
        }
        break;
    case 'i':
        if (strncmp(src + *spos, "excl;", 5) == 0) {
            // INVERTED EXCLAMATION MARK
            target[(*tpos)++] = '!';
            *spos += 4;
        } else if (strncmp(src + *spos, "quest;", 6) == 0) {
            // INVERTED QUESTION MARK
            target[(*tpos)++] = '?';
            *spos += 5;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN SMALL LETTER I WITH ACUTE
            target[(*tpos)++] = 'i';
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN SMALL LETTER I WITH GRAVE
            target[(*tpos)++] = 'i';
            *spos += 5;
        } else if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN SMALL LETTER I WITH DIAERESIS
            target[(*tpos)++] = 'i';
            *spos += 3;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN SMALL LETTER I WITH CIRCUMFLEX
            target[(*tpos)++] = 'i';
            *spos += 4;
        }
        break;
    case 'I':
        if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN CAPITAL LETTER I WITH ACUTE
            target[(*tpos)++] = 'I';
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN CAPITAL LETTER I WITH GRAVE
            target[(*tpos)++] = 'I';
            *spos += 5;
        } else if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN CAPITAL LETTER I WITH DIAERESIS
            target[(*tpos)++] = 'I';
            *spos += 3;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN CAPITAL LETTER I WITH CIRCUMFLEX
            target[(*tpos)++] = 'I';
            *spos += 4;
        }
        break;
    case 'l':
        if (strncmp(src + *spos, "t;", 2) == 0) {
            // LESS-THAN SIGN
            target[(*tpos)++] = '<';
            *spos += 1;
        } else if (strncmp(src + *spos, "aquo;", 5) == 0) {
            // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
            target[(*tpos)++] = '<';
            *spos += 4;
        } else if (strncmp(src + *spos, "arr;", 4) == 0) {
            // LEFTWARDS ARROW
            target[(*tpos)++] = '<';
            *spos += 3;
        }
        break;
    case 'm':
        if (strncmp(src + *spos, "iddot;", 6) == 0) {
            // MIDDLE DOT
            target[(*tpos)++] = '.';
            *spos += 5;
        }
        break;
    case 'n':
        if (strncmp(src + *spos, "dash;", 5) == 0) {
            // EN DASH
            target[(*tpos)++] = '-';
            *spos += 4;
        } else if (strncmp(src + *spos, "tilde;", 6) == 0) {
            // LATIN SMALL LETTER N WITH TILDE
            target[(*tpos)++] = 'n';
            *spos += 5;
        }
        break;
    case 'N':
        if (strncmp(src + *spos, "tilde;", 6) == 0) {
            // LATIN CAPITAL LETTER N WITH TILDE
            target[(*tpos)++] = 'N';
            *spos += 5;
        }
        break;
    case 'o':
        if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN SMALL LETTER O WITH DIAERESIS
            target[(*tpos)++] = 'o';
            *spos += 3;
        } else if (strncmp(src + *spos, "slash;", 6) == 0) {
            // LATIN SMALL LETTER O WITH STROKE
            target[(*tpos)++] = 'o';
            *spos += 5;
        } else if (strncmp(src + *spos, "elig;", 5) == 0) {
            // LATIN SMALL LIGATURE OE
            target[(*tpos)++] = 'o';
            target[(*tpos)++] = 'e';
            *spos += 4;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN SMALL LETTER O WITH ACUTE
            target[(*tpos)++] = 'o';
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN SMALL LETTER O WITH GRAVE
            target[(*tpos)++] = 'o';
            *spos += 5;
        } else if (strncmp(src + *spos, "tilde;", 6) == 0) {
            // LATIN SMALL LETTER O WITH TILDE
            target[(*tpos)++] = 'o';
            *spos += 5;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN SMALL LETTER O WITH CIRCUMFLEX
            target[(*tpos)++] = 'o';
            *spos += 4;
        }
        break;
    case 'O':
        if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN CAPITAL LETTER O WITH DIAERESIS
            target[(*tpos)++] = 'O';
            *spos += 3;
        } else if (strncmp(src + *spos, "slash;", 6) == 0) {
            // LATIN CAPITAL LETTER O WITH STROKE
            target[(*tpos)++] = 'O';
            *spos += 5;
        } else if (strncmp(src + *spos, "Elig;", 5) == 0) {
            // LATIN CAPITAL LIGATURE OE
            target[(*tpos)++] = 'O';
            target[(*tpos)++] = 'E';
            *spos += 4;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN CAPITAL LETTER O WITH ACUTE
            target[(*tpos)++] = 'O';
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN CAPITAL LETTER O WITH GRAVE
            target[(*tpos)++] = 'O';
            *spos += 5;
        } else if (strncmp(src + *spos, "tilde;", 6) == 0) {
            // LATIN CAPITAL LETTER O WITH TILDE
            target[(*tpos)++] = 'O';
            *spos += 5;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN CAPITAL LETTER O WITH CIRCUMFLEX
            target[(*tpos)++] = 'O';
            *spos += 4;
        }
        break;
    case 'p':
        if (strncmp(src + *spos, "ound;", 5) == 0) {
            // POUND SIGN
            target[(*tpos)++] = 'p';
            *spos += 4;
        } else if (strncmp(src + *spos, "lusmn;", 6) == 0) {
            // PLUS-MINUS SIGN
            target[(*tpos)++] = '+';
            target[(*tpos)++] = '/';
            target[(*tpos)++] = '-';
            *spos += 5;
        } else if (strncmp(src + *spos, "ermil;", 6) == 0) {
            // PER MILLE SIGN
            target[(*tpos)++] = 'p';
            *spos += 5;
        }
        break;
    case 'q':
        if (strncmp(src + *spos, "uot;", 4) == 0) {
            // QUOTATION MARK
            target[(*tpos)++] = '"';
            *spos += 3;
        }
        break;
    case 'r':
        if (strncmp(src + *spos, "aquo;", 5) == 0) {
            // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
            target[(*tpos)++] = '>';
            *spos += 4;
        } else if (strncmp(src + *spos, "arr;", 4) == 0) {
            // RIGHTWARDS ARROW
            target[(*tpos)++] = '>';
            *spos += 3;
        } else if (strncmp(src + *spos, "dquo;", 5) == 0) {
            // RIGHT DOUBLE QUOTATION MARK
            target[(*tpos)++] = '"';
            *spos += 4;
        }
        break;
    case 's':
        if (strncmp(src + *spos, "ect;", 4) == 0) {
            // SECTION SIGN
            target[(*tpos)++] = 's';
            *spos += 3;
        } else if (strncmp(src + *spos, "zlig;", 5) == 0) {
            // LATIN SMALL LETTER SHARP S
            target[(*tpos)++] = 'B';
            *spos += 4;
        } else if (strncmp(src + *spos, "caron;", 6) == 0) {
            // LATIN SMALL LETTER S WITH CARON
            target[(*tpos)++] = 's';
            *spos += 5;
        } else if (strncmp(src + *spos, "up", 2) == 0 && *(src + (*spos + 3)) == ';') {
            // SUPERSCRIPT ONE|TWO|THREE
            switch (*(src + (*spos + 2))) {
            case '1':
                target[(*tpos)++] = '1';
                break;
            case '2':
                target[(*tpos)++] = '2';
                break;
            case '3':
                target[(*tpos)++] = '3';
                break;
            }
            *spos += 3;
        }
        break;
    case 'S':
        if (strncmp(src + *spos, "caron;", 6) == 0) {
            // LATIN CAPITAL LETTER S WITH CARON
            target[(*tpos)++] = 'S';
            *spos += 5;
        }
        break;
    case 't':
        if (strncmp(src + *spos, "horn;", 5) == 0) {
            // LATIN SMALL LETTER THORN
            target[(*tpos)++] = 't';
            *spos += 4;
        }
        break;
    case 'T':
        if (strncmp(src + *spos, "HORN;", 5) == 0) {
            // LATIN CAPITAL LETTER THORN
            target[(*tpos)++] = 'T';
            *spos += 4;
        }
        break;
    case 'u':
        if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN SMALL LETTER U WITH DIAERESIS
            target[(*tpos)++] = 'u';
            *spos += 3;
        } else if (strncmp(src + *spos, "arr;", 4) == 0) {
            // UPWARDS ARROW
            target[(*tpos)++] = '^';
            *spos += 3;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN SMALL LETTER U WITH ACUTE
            target[(*tpos)++] = 'u';
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN SMALL LETTER U WITH GRAVE
            target[(*tpos)++] = 'u';
            *spos += 5;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN SMALL LETTER U WITH CIRCUMFLEX
            target[(*tpos)++] = 'u';
            *spos += 4;
        }
        break;
    case 'U':
        if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN CAPITAL LETTER U WITH DIAERESIS
            target[(*tpos)++] = 'U';
            *spos += 3;
        } else if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN CAPITAL LETTER U WITH ACUTE
            target[(*tpos)++] = 'U';
            *spos += 5;
        } else if (strncmp(src + *spos, "grave;", 6) == 0) {
            // LATIN CAPITAL LETTER U WITH GRAVE
            target[(*tpos)++] = 'U';
            *spos += 5;
        } else if (strncmp(src + *spos, "circ;", 5) == 0) {
            // LATIN CAPITAL LETTER U WITH CIRCUMFLEX
            target[(*tpos)++] = 'U';
            *spos += 4;
        }
        break;
    case 'y':
        if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN SMALL LETTER Y WITH ACUTE
            target[(*tpos)++] = 'y';
            *spos += 5;
        } else if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN SMALL LETTER Y WITH DIAERESIS
            target[(*tpos)++] = 'y';
            *spos += 3;
        }
        break;
    case 'Y':
        if (strncmp(src + *spos, "acute;", 6) == 0) {
            // LATIN CAPITAL LETTER Y WITH ACUTE
            target[(*tpos)++] = 'Y';
            *spos += 5;
        } else if (strncmp(src + *spos, "uml;", 4) == 0) {
            // LATIN CAPITAL LETTER Y WITH DIAERESIS
            target[(*tpos)++] = 'Y';
            *spos += 3;
        }
        break;
    default:
        // Nothing was found so return to original position
        *spos -= 1;
        break;
    }
}
#endif
// Copy and filter html encoded characters
static size_t copy_html_text(char* target, char* src, size_t len)
{
    // filtered text
    unsigned char filter_buf[1024] = { 0 };
    size_t filter_len = 0;

    for (size_t i = 0; i < len; i++) {

        if (src[i] != '&') {
            filter_buf[filter_len] = src[i];
            filter_len++;
            continue;
        }

        // Consume the & character
        i++;
#ifndef DISABLE_UTF_8
        copy_html_character_utf8(filter_buf, src, &filter_len, &i);
#else
        copy_html_character_ascii(filter_buf, src, &filter_len, &i);
#endif
    }

    strncpy(target, (char*)filter_buf, filter_len);
    return filter_len;
}

// parse current link in buffer to html_link
// inner_pre appends spaces before inner_text
static void parse_current_link(html_buffer* buffer, html_link* linkbuf, size_t inner_pre_space)
{
    // find the start of the link string
    skip_next_str(buffer, "href", 4);
    skip_next_char(buffer, '=');
    skip_next_char(buffer, '"');

    size_t link_len = 0;
    for (;; link_len++) {
        if (buffer->html[buffer->current + link_len] == '"')
            break;
    }

    strncpy(linkbuf->url.text, buffer->html + buffer->current, link_len);
    linkbuf->url.size = link_len;
    linkbuf->url.text[link_len] = '\0';

    // go to end of the opening a tag
    skip_next_char(buffer, '>');

    // find the length of the text
    size_t text_len = get_current_text_size(buffer);

    // copy the inner text
    // First add the possible pre_spaces
    for (size_t i = 0; i < inner_pre_space; i++)
        *(linkbuf->inner_text.text + i) = ' ';

    // Then copy the actual text
    size_t filter_len = copy_html_text(linkbuf->inner_text.text + inner_pre_space, buffer->html + buffer->current, text_len);
    linkbuf->inner_text.size = filter_len + inner_pre_space;
    linkbuf->inner_text.text[filter_len + inner_pre_space] = '\0';

    // go to end of the closing a tag
    skip_next_tag(buffer, "a", 1, true);
}

static void parse_title(html_parser* parser, html_buffer* buffer)
{
    skip_next_tag(buffer, "big", 3, false);
    size_t title_len = 0;
    for (;; title_len++) {
        if (buffer->html[buffer->current + title_len] == '<')
            break;
    }

    // Copy the title string
    strncpy(html_text_text(parser->title), buffer->html + buffer->current, title_len);
    parser->title.size = title_len;
    parser->title.text[title_len] = '\0';
    buffer->current += title_len;
    skip_next_tag(buffer, "big", 3, true);
}

static void parse_navigation_text(html_buffer* buffer, html_text* text, bool last_link)
{
    char endchar = last_link ? '<' : '&';
    size_t text_len = 0;
    for (;; text_len++) {
        if (buffer->html[buffer->current + text_len] == endchar)
            break;
    }

    strncpy(text->text, buffer->html + buffer->current, text_len);
    text->text[text_len] = '\0';
    text->size = text_len;
    buffer->current += text_len;
}

// Parse navigations for between next and previous (sub)pages
static void parse_top_navigation(html_parser* parser, html_buffer* buffer)
{
    for (size_t i = 0; i < TOP_NAVIGATION_SIZE; i++) {
        bool last_link = i == TOP_NAVIGATION_SIZE - 1;
        if (i != 0)
            skip_next_str(buffer, "&nbsp;", 6);

        tag_type tag = get_tag_type(buffer);
        if (tag == LINK) {
            parser->top_navigation[i].type = HTML_LINK;
            parse_current_link(buffer, &html_item_as_link(parser->top_navigation[i]), 0);
        } else {
            parser->top_navigation[i].type = HTML_TEXT;
            parse_navigation_text(buffer, &html_item_as_text(parser->top_navigation[i]), last_link);
        }

        if (!last_link)
            skip_next_str(buffer, "&nbsp;|", 7);
    }
}

static void parse_bottom_navigation(html_parser* parser, html_buffer* buffer)
{
    skip_next_tag(buffer, "p", 1, false);
    for (size_t i = 0; i < BOTTOM_NAVIGATION_SIZE; i++) {
        parse_current_link(buffer, &parser->bottom_navigation[i], 0);
    }
}
static void parse_middle_link(html_parser* parser, html_buffer* buffer, size_t spaces)
{
    // create new item
    html_item item;
    item.type = HTML_LINK;
    parse_current_link(buffer, &html_item_as_link(item), spaces);
    // Turn invalid links to text items
    if (item.item.link.url.size != HTML_LINK_SIZE) {
        item.type = HTML_TEXT;
        item.item.text.size = item.item.link.inner_text.size;
        strcpy(item.item.text.text, item.item.link.inner_text.text);
    }

    // append item to row
    size_t row_index = parser->middle[parser->middle_rows].size;
    parser->middle[parser->middle_rows].items[row_index] = item;
    parser->middle[parser->middle_rows].size++;
}

static void parse_middle_text(html_parser* parser, html_buffer* buffer, size_t spaces)
{

    size_t text_len = get_current_text_size(buffer);

    // create new item
    html_item item;
    item.type = HTML_TEXT;

    // copy the text
    // First add the possible pre_spaces
    for (size_t i = 0; i < spaces; i++)
        item.item.text.text[i] = ' ';

    // Then copy the actual text
    size_t filter_len = copy_html_text(item.item.text.text + spaces, buffer->html + buffer->current, text_len);
    // Ignore empty texts
    if (filter_len == 0)
        return;
    item.item.text.size = filter_len + spaces;
    item.item.text.text[item.item.text.size] = '\0';
    buffer->current += text_len;

    // append item to row
    size_t row_index = parser->middle[parser->middle_rows].size;
    parser->middle[parser->middle_rows].items[row_index] = item;
    parser->middle[parser->middle_rows].size++;
}

static void parse_middle(html_parser* parser, html_buffer* buffer)
{

    skip_next_tag(buffer, "pre", 3, false);
    // Loop until the closing pre tag is found
    for (; strncmp(buffer->html + buffer->current, "</pre>", 6) != 0; buffer->current++) {
        html_buffer line_buf;
        line_buf.current = 0;
        line_buf.size = 0;
        memset(line_buf.html, 0, 1028);

        for (; buffer->html[buffer->current] != '\n'; buffer->current++) {
            line_buf.html[line_buf.size] = buffer->html[buffer->current];
            line_buf.size++;
        }
        line_buf.html[line_buf.size] = '\0';

        if (line_buf.size == 0 || line_buf.html[0] == '&') {
            parser->middle_rows++;
            continue;
        }

        size_t pre_space = 0;
        for (; line_buf.current < line_buf.size && line_buf.html[line_buf.current] == ' '; line_buf.current++) {
            pre_space++;
        }

        line_buf.current--;
        for (int i = 0; line_buf.current < line_buf.size; line_buf.current++, i++) {
            tag_type type = get_tag_type(&line_buf);
            if (type == LINK) {
                parse_middle_link(parser, &line_buf, pre_space);
                line_buf.current--; // Don't miss the next starting character
            } else {
                parse_middle_text(parser, &line_buf, pre_space);
            }
            pre_space = 0;
        }

        parser->middle_rows++;
    }
}

static void parse_sub_pages(html_parser* parser, html_buffer* buffer)
{
    skip_next_tag(buffer, "p", 1, false);
    while (strncmp(buffer->html + buffer->current, "</p>", 3) != 0) {

        tag_type type = get_tag_type(buffer);
        html_item item;
        size_t text_size = 0;
        switch (type) {
        case UNKNOWN: {
            // In this context UNKNOWN means regular text
            text_size = get_current_text_size(buffer);
            item.type = HTML_TEXT;
            memcpy(html_item_as_text(item).text, buffer->html + buffer->current, text_size);
            item.item.text.size = text_size;
            item.item.text.text[item.item.text.size] = '\0';
        } break;
        case LINK: {
            item.type = HTML_LINK;
            parse_current_link(buffer, &html_item_as_link(item), 0);
        } break;
        case FONT: {
            skip_next_tag(buffer, "font", 4, true);
        } break;
        default:
            break;
        }

        if (type == FONT) {
            continue;
        }

        if (parser->sub_pages.items == NULL) {
            parser->sub_pages.items = malloc(sizeof(html_buffer));
        } else {
            parser->sub_pages.items = realloc(parser->sub_pages.items, sizeof(html_buffer) * parser->sub_pages.size + 1);
        }

        parser->sub_pages.items[parser->sub_pages.size++] = item;
        buffer->current += text_size;
    }
}

bool check_valid_page(html_buffer* buffer)
{
    // Title tag is in CAPS if the page is valid.
    // In invalid page, the title is in lower case.
    // Therfore if we try find the TITLE tag and it's not found before
    // reaching end of the page, we know that the page is invalid.
    skip_next_tag(buffer, "TITLE", 5, false);
    return buffer->current < buffer->size;
}

void parse_html(html_parser* parser)
{
    html_buffer* buffer = &parser->_curl_buffer;
    buffer->current = 0;

    // Check if the loaded page actually exist
    // Yle tekstitv doesn't return 404 if page is not found.
    // Tekstitv returns page with a title "YLE Teleport"
    if (!check_valid_page(buffer)) {
        parser->curl_load_error = true;
        return;
    }

    // Title
    skip_next_tag(buffer, "p", 1, false);
    parse_title(parser, buffer);
    skip_next_tag(buffer, "p", 1, true);

    // Top nav
    skip_next_tag(buffer, "SPAN", 4, false);
    parse_top_navigation(parser, buffer);
    skip_next_tag(buffer, "SPAN", 4, true);

    // Middle
    skip_next_tag(buffer, "DIV", 3, false);
    parse_middle(parser, buffer);
    skip_next_tag(buffer, "DIV", 3, true);

    // Sub pages
    skip_next_tag(buffer, "DIV", 3, false);
    parse_sub_pages(parser, buffer);
    skip_next_tag(buffer, "DIV", 3, true);

    // Bottom nav
    skip_next_tag(buffer, "DIV", 3, true);
    parse_bottom_navigation(parser, buffer);
}

void link_from_ints(html_parser* parser, int page, int subpage)
{
    assert(page >= 100 && page <= 999);
    assert(subpage >= 1 && subpage <= 99);

    char tmp_link[] = "xxx_00xx.htm";

    tmp_link[0] = (page / 100) + '0';
    tmp_link[1] = (page % 100 / 10) + '0';
    tmp_link[2] = (page % 10) + '0';
    tmp_link[6] = subpage > 9 ? (subpage % 100 / 10) + '0' : '0';
    tmp_link[7] = (subpage % 10) + '0';

    memcpy(parser->link, tmp_link, HTML_LINK_SIZE);
    parser->link[HTML_LINK_SIZE] = '\0';
}

void link_from_short_link(html_parser* parser, char* shortlink)
{
    memcpy(parser->link, shortlink, HTML_LINK_SIZE);
    parser->link[HTML_LINK_SIZE] = '\0';
}

int page_number(const char* page)
{
    assert(page);

    size_t len = strlen(page);
    if (strlen(page) > 3)
        return -1;

    for (size_t i = 0; i < len; i++) {
        if (page[i] < '0' || page[i] > '9')
            return -1;
    }

    int result = atoi(page);
    return result >= 100 && result < 1000 ? result : -1;
}

int subpage_number(const char* subpage)
{
    assert(subpage);

    size_t len = strlen(subpage);
    if (len > 2)
        return -1;

    for (size_t i = 0; i < len; i++) {
        if (subpage[i] < '0' || subpage[i] > '9')
            return -1;
    }

    int result = atoi(subpage);
    return result >= 1 && result < 100 ? result : -1;
}

void init_html_parser(html_parser* parser)
{
    parser->middle_rows = 0;
    parser->middle = calloc(MIDDLE_HTML_ROWS_MAX, sizeof(html_row));
    parser->sub_pages.size = 0;
    parser->sub_pages.items = NULL;
    memset(parser->title.text, 0, HTML_TEXT_MAX);
    memset(parser->bottom_navigation, 0, sizeof(html_link) * BOTTOM_NAVIGATION_SIZE);
    memset(parser->top_navigation, 0, sizeof(html_item) * TOP_NAVIGATION_SIZE);
    memset(parser->_curl_buffer.html, 0, 1024 * 32);
}

void free_html_parser(html_parser* parser)
{
    if (parser->middle != NULL)
        free(parser->middle);
    if (parser->sub_pages.items != NULL)
        free(parser->sub_pages.items);
}
