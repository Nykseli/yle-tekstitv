#include <check.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <tekstitv.h>
#include <unistd.h>

#define top_i(_i) (parser.top_navigation[_i])
#define top_t(_i) (html_item_as_text(top_i(_i)))
#define top_l(_i) (html_item_as_link(top_i(_i)))
#define bot_u(_i) (parser.bottom_navigation[_i].url)
#define bot_t(_i) (parser.bottom_navigation[_i].inner_text)
#define sub_i(_i) (parser.sub_pages.items[_i])
#define sub_t(_i) (html_item_as_text(sub_i(_i)))
#define sub_l(_i) (html_item_as_link(sub_i(_i)))
#define m_empty(_i) ck_assert_int_eq(parser.middle[_i].size, 0)
#define m_size(_i, _s) ck_assert_int_eq(parser.middle[_i].size, _s)
#define m_text(_r, _i, _l, _t)                                            \
    do {                                                                  \
        ck_assert_int_eq(parser.middle[_r].items[_i].type, HTML_TEXT);    \
        ck_assert_int_eq(parser.middle[_r].items[_i].item.text.size, _l); \
        ck_assert_str_eq(parser.middle[_r].items[_i].item.text.text, _t); \
    } while (0)
#define m_link(_r, _i, _lt, _tl, _tt)                                                     \
    do {                                                                                  \
        ck_assert_int_eq(parser.middle[_r].items[_i].type, HTML_LINK);                    \
        ck_assert_int_eq(parser.middle[_r].items[_i].item.link.url.size, HTML_LINK_SIZE); \
        ck_assert_str_eq(parser.middle[_r].items[_i].item.link.url.text, _lt);            \
        ck_assert_int_eq(parser.middle[_r].items[_i].item.link.inner_text.size, _tl);     \
        ck_assert_str_eq(parser.middle[_r].items[_i].item.link.inner_text.text, _tt);     \
    } while (0)

// Helper so we don't have to actually curl the pages every time we run tests
void load_page_helper(html_parser* parser, const char* file_name)
{
    int fd = open(file_name, O_RDONLY);
    struct stat fs;
    fstat(fd, &fs);

    read(fd, parser->_curl_buffer.html, fs.st_size);
    parser->_curl_buffer.html[fs.st_size] = '\0';
    parser->_curl_buffer.current = 0;
    parser->_curl_buffer.size = 0;
    parser->_curl_buffer.size = fs.st_size;
    parser->curl_load_error = false;
    close(fd);
}

START_TEST(parse_html_test_page_100)
{
    html_parser parser;
    init_html_parser(&parser);
    load_page_helper(&parser, "tests/test_html/100.htm");
    parse_html(&parser);

    // Title test
    ck_assert_int_eq(parser.title.size, 27);
    ck_assert_str_eq(parser.title.text, "Yle Teksti-TV | Sivu 100.1 ");
    // Top nav test
    ck_assert_int_eq(top_i(0).type, HTML_TEXT);
    ck_assert_int_eq(top_t(0).size, 14);
    ck_assert_str_eq(top_t(0).text, "Edellinen sivu");
    ck_assert_int_eq(top_i(1).type, HTML_TEXT);
    ck_assert_int_eq(top_t(1).size, 17);
    ck_assert_str_eq(top_t(1).text, "Edellinen alasivu");
    ck_assert_int_eq(top_i(2).type, HTML_LINK);
    ck_assert_int_eq(top_l(2).url.size, HTML_LINK_SIZE);
    ck_assert_str_eq(top_l(2).url.text, "100_0002.htm");
    ck_assert_int_eq(top_l(2).inner_text.size, 16);
    ck_assert_str_eq(top_l(2).inner_text.text, "Seuraava alasivu");
    ck_assert_int_eq(top_i(3).type, HTML_LINK);
    ck_assert_int_eq(top_l(3).url.size, HTML_LINK_SIZE);
    ck_assert_str_eq(top_l(3).url.text, "101_0001.htm");
    ck_assert_int_eq(top_l(3).inner_text.size, 13);
    ck_assert_str_eq(top_l(3).inner_text.text, "Seuraava sivu");
    // Bottom nav test
    ck_assert_int_eq(bot_u(0).size, HTML_LINK_SIZE);
    ck_assert_str_eq(bot_u(0).text, "102_0001.htm");
    ck_assert_int_eq(bot_t(0).size, 7);
    ck_assert_str_eq(bot_t(0).text, "Kotimaa");
    ck_assert_int_eq(bot_u(1).size, HTML_LINK_SIZE);
    ck_assert_str_eq(bot_u(1).text, "130_0001.htm");
    ck_assert_int_eq(bot_t(1).size, 8);
    ck_assert_str_eq(bot_t(1).text, "Ulkomaat");
    ck_assert_int_eq(bot_u(2).size, HTML_LINK_SIZE);
    ck_assert_str_eq(bot_u(2).text, "160_0001.htm");
    ck_assert_int_eq(bot_t(2).size, 6);
    ck_assert_str_eq(bot_t(2).text, "Talous");
    ck_assert_int_eq(bot_u(3).size, HTML_LINK_SIZE);
    ck_assert_str_eq(bot_u(3).text, "201_0001.htm");
    ck_assert_int_eq(bot_t(3).size, 7);
    ck_assert_str_eq(bot_t(3).text, "Urheilu");
    ck_assert_int_eq(bot_u(4).size, HTML_LINK_SIZE);
    ck_assert_str_eq(bot_u(4).text, "700_0001.htm");
    ck_assert_int_eq(bot_t(4).size, 13);
    ck_assert_str_eq(bot_t(4).text, "Svenska sidor");
    ck_assert_int_eq(bot_u(5).size, HTML_LINK_SIZE);
    ck_assert_str_eq(bot_u(5).text, "100_0001.htm");
    ck_assert_int_eq(bot_t(5).size, 9);
    ck_assert_str_eq(bot_t(5).text, "Teksti-TV");
    // Sub page test
    ck_assert_int_eq(parser.sub_pages.size, 7);
    ck_assert_int_eq(sub_i(0).type, HTML_TEXT);
    ck_assert_int_eq(sub_t(0).size, HTML_LINK_SIZE);
    ck_assert_str_eq(sub_t(0).text, "Alasivut: 1,");
    ck_assert_int_eq(sub_i(1).type, HTML_LINK);
    ck_assert_int_eq(sub_l(1).url.size, HTML_LINK_SIZE);
    ck_assert_str_eq(sub_l(1).url.text, "100_0002.htm");
    ck_assert_int_eq(sub_l(1).inner_text.size, 1);
    ck_assert_str_eq(sub_l(1).inner_text.text, "2");
    ck_assert_int_eq(sub_i(2).type, HTML_TEXT);
    ck_assert_int_eq(sub_t(2).size, 1);
    ck_assert_str_eq(sub_t(2).text, ",");
    ck_assert_int_eq(sub_i(3).type, HTML_LINK);
    ck_assert_int_eq(sub_l(3).url.size, HTML_LINK_SIZE);
    ck_assert_str_eq(sub_l(3).url.text, "100_0003.htm");
    ck_assert_int_eq(sub_l(3).inner_text.size, 1);
    ck_assert_str_eq(sub_l(3).inner_text.text, "3");
    ck_assert_int_eq(sub_i(4).type, HTML_TEXT);
    ck_assert_int_eq(sub_t(4).size, 1);
    ck_assert_str_eq(sub_t(4).text, ",");
    ck_assert_int_eq(sub_i(5).type, HTML_LINK);
    ck_assert_int_eq(sub_l(5).url.size, HTML_LINK_SIZE);
    ck_assert_str_eq(sub_l(5).url.text, "100_0004.htm");
    ck_assert_int_eq(sub_l(5).inner_text.size, 1);
    ck_assert_str_eq(sub_l(5).inner_text.text, "4");
    // TODO: should the extra space at the end be considered as a bug?
    ck_assert_int_eq(sub_i(6).type, HTML_TEXT);
    ck_assert_int_eq(sub_t(6).size, 1);
    ck_assert_str_eq(sub_t(6).text, " ");
    // Middle test
    ck_assert_int_eq(parser.middle_rows, 25);
    m_empty(0);
    m_empty(1);
    m_size(2, 1);
    m_text(2, 0, 21, "            Teksti-TV");
    m_empty(3);
    m_size(4, 5);
    m_text(4, 0, 5, "     ");
    m_text(4, 1, 15, "yle.fi/tekstitv");
    m_text(4, 2, 4, "    ");
    m_link(4, 3, "199_0001.htm", 3, "199");
    m_text(4, 4, 15, " PÄÄHAKEMISTO");
    m_empty(5);
    m_size(6, 5);
    m_text(6, 0, 2, "  ");
    m_link(6, 1, "104_0001.htm", 3, "104");
    m_text(6, 2, 10, " Suomessa ");
    m_link(6, 3, "149_0001.htm", 3, "149");
    m_text(6, 4, 22, " uutta koronatartuntaa");
    m_empty(7);
    m_size(8, 5);
    m_text(8, 0, 2, "  ");
    m_link(8, 1, "106_0001.htm", 3, "106");
    m_text(8, 2, 17, " Jyväskylässä ");
    m_link(8, 3, "500_0001.htm", 3, "500");
    m_text(8, 4, 13, " karanteeniin");
    m_empty(9);
    m_size(10, 3);
    m_text(10, 0, 2, "  ");
    m_link(10, 1, "105_0001.htm", 3, "105");
    m_text(10, 2, 34, " Marin: Maskiasiassa ei valehdeltu");
    m_empty(11);
    m_size(12, 3);
    m_text(12, 0, 2, "  ");
    m_link(12, 1, "136_0001.htm", 3, "136");
    m_text(12, 2, 35, " Lukashenka tapasi oppositiovankeja");
    m_empty(13);
    m_empty(14);
    m_size(15, 3);
    m_text(15, 0, 2, "  ");
    m_link(15, 1, "210_0001.htm", 3, "210");
    m_text(15, 2, 35, " Valtteri Bottas keskeytti Saksassa");
    m_empty(16);
    m_empty(17);
    m_size(18, 7);
    m_text(18, 0, 4, "    ");
    m_link(18, 1, "101_0001.htm", 3, "101");
    m_text(18, 2, 10, " UUTISET  ");
    m_link(18, 3, "160_0001.htm", 3, "160");
    m_text(18, 4, 8, " TALOUS ");
    m_link(18, 5, "190_0001.htm", 3, "190");
    m_text(18, 6, 8, " ENGLISH");
    m_size(19, 7);
    m_text(19, 0, 4, "    ");
    m_link(19, 1, "201_0001.htm", 3, "201");
    m_text(19, 2, 10, " URHEILU  ");
    m_link(19, 3, "350_0001.htm", 3, "350");
    m_text(19, 4, 8, " RADIOT ");
    m_link(19, 5, "470_0001.htm", 3, "470");
    m_text(19, 6, 9, " VEIKKAUS");
    m_size(20, 7);
    m_text(20, 0, 4, "    ");
    m_link(20, 1, "300_0001.htm", 3, "300");
    m_text(20, 2, 10, " OHJELMAT ");
    m_link(20, 3, "400_0001.htm", 3, "400");
    m_text(20, 4, 10, " SÄÄ    ");
    m_link(20, 5, "575_0001.htm", 3, "575");
    m_text(20, 6, 10, " TEKSTI-TV");
    m_size(21, 7);
    m_text(21, 0, 4, "    ");
    m_link(21, 1, "799_0001.htm", 3, "799");
    m_text(21, 2, 10, " SVENSKA  ");
    m_link(21, 3, "500_0001.htm", 3, "500");
    m_text(21, 4, 8, " ALUEET ");
    m_link(21, 5, "890_0001.htm", 3, "890");
    m_text(21, 6, 10, " KALENTERI");
    m_size(22, 4);
    m_text(22, 0, 35, "    Sää paikkakunnittain         ");
    m_link(22, 1, "406_0001.htm", 3, "406");
    m_text(22, 2, 1, "-");
    m_link(22, 3, "408_0001.htm", 3, "408");
    m_size(23, 2);
    m_text(23, 0, 37, "    Saksalaiset perunaohukaiset      ");
    m_link(23, 1, "811_0001.htm", 3, "811");
    m_empty(24);

    free_html_parser(&parser);
}
END_TEST

START_TEST(link_from_ints_test)
{
    html_parser parser;
    init_html_parser(&parser);
    link_from_ints(&parser, 100, 1);
    ck_assert_str_eq(parser.link, "100_0001.htm");
    link_from_ints(&parser, 899, 12);
    ck_assert_str_eq(parser.link, "899_0012.htm");
    link_from_ints(&parser, 895, 99);
    ck_assert_str_eq(parser.link, "895_0099.htm");
    link_from_ints(&parser, 255, 85);
    ck_assert_str_eq(parser.link, "255_0085.htm");
    free_html_parser(&parser);
}
END_TEST

START_TEST(link_from_short_link_test)
{
    html_parser parser;
    init_html_parser(&parser);
    link_from_short_link(&parser, "100_0001.htm");
    ck_assert_str_eq(parser.link, "100_0001.htm");
    link_from_short_link(&parser, "899_0012.htm");
    ck_assert_str_eq(parser.link, "899_0012.htm");
    link_from_short_link(&parser, "895_0099.htm");
    ck_assert_str_eq(parser.link, "895_0099.htm");
    link_from_short_link(&parser, "255_0085.htm");
    ck_assert_str_eq(parser.link, "255_0085.htm");
    free_html_parser(&parser);
}
END_TEST

START_TEST(page_number_test)
{
    // Success
    ck_assert_int_eq(page_number("100"), 100);
    ck_assert_int_eq(page_number("123"), 123);
    ck_assert_int_eq(page_number("580"), 580);
    ck_assert_int_eq(page_number("899"), 899);
    // Fails
    ck_assert_int_eq(page_number("asdasdasdasdasdasdasdasdasd"), -1);
    ck_assert_int_eq(page_number("1000"), -1);
    ck_assert_int_eq(page_number("99"), -1);
    ck_assert_int_eq(page_number("-1123"), -1);
}
END_TEST

START_TEST(subpage_number_test)
{
    // Success
    ck_assert_int_eq(subpage_number("1"), 1);
    ck_assert_int_eq(subpage_number("12"), 12);
    ck_assert_int_eq(subpage_number("99"), 99);
    // Fails
    ck_assert_int_eq(subpage_number("123"), -1);
    ck_assert_int_eq(subpage_number("100"), -1);
    ck_assert_int_eq(subpage_number("asd123"), -1);
    ck_assert_int_eq(subpage_number("-1234"), -1);
}
END_TEST

Suite* parser_suite()
{
    Suite* s;
    TCase* tc_core;

    s = suite_create("Html Parser");
    tc_core = tcase_create("Html Parser Core");

    tcase_add_test(tc_core, parse_html_test_page_100);
    tcase_add_test(tc_core, link_from_ints_test);
    tcase_add_test(tc_core, link_from_short_link_test);
    tcase_add_test(tc_core, page_number_test);
    tcase_add_test(tc_core, subpage_number_test);

    suite_add_tcase(s, tc_core);

    return s;
}

int main()
{
    int number_failed;
    Suite* s;
    SRunner* sr;

    s = parser_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
