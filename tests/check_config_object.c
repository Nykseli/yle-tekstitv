#include <check.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../src/config.h"
// Expose the parse_config file function from config.c
bool parse_config_file(char* file_data);

typedef struct {
    char* name;
    char* expected_out;
    char* config;
    // If expected output is empty, it means that there are no errors
    bool success;
} config_test;

typedef struct {
    config_test* configs;
    size_t size;
} config_list;

typedef struct {
    char* str;
    size_t size;
} string;

typedef struct {
    string* str;
    size_t size;
} string2d;

void free_string2d(string2d* list)
{
    for (size_t idx = 0; idx < list->size; idx++) {
        free(list->str[idx].str);
    }

    free(list->str);
}

void free_config_list(config_list* list)
{
    for (size_t idx = 0; idx < list->size; idx++) {
        free(list->configs[idx].name);
        free(list->configs[idx].config);
        if (list->configs[idx].expected_out != NULL) {
            free(list->configs[idx].expected_out);
        }
    }

    free(list->configs);
}

string new_str(char* origin, size_t len)
{
    string str;
    str.size = len;
    str.str = malloc(len + 1);
    memcpy(str.str, origin, len);
    str.str[len] = '\0';

    return str;
}

void string2d_append(string2d* list, string str)
{
    if (list->str == NULL) {
        list->str = malloc(sizeof(string));
    } else {
        list->str = realloc(list->str, sizeof(string) * (list->size + 1));
    }

    list->str[list->size] = str;
    list->size++;
}

string2d str_split(char* origin, const char* splitter)
{
    size_t splitter_len = strlen(splitter);
    string2d list;
    list.str = NULL;
    list.size = 0;

    char* split_start = origin;
    size_t split_len = 0;
    for (; *origin != '\0'; origin++) {
        if (strncmp(origin, splitter, splitter_len) == 0) {
            string str = new_str(split_start, split_len);
            string2d_append(&list, str);
            origin += splitter_len;
            split_start = origin;
            split_len = 0;
        }
        split_len++;
    }

    // Finally append the last part after the delimeter
    if (split_len > 0) {
        string str = new_str(split_start, split_len);
        string2d_append(&list, str);
    }

    return list;
}

/**
 * Null safe comparison checks for nulls.
 * If both are null, they are equal. If only one is null, they are not equal.
 * If neither are null, they are compared with normal strcmp
 */
bool nullsafe_strcmp(const char* s1, const char* s2)
{
    if (s1 == NULL && s2 == NULL)
        return true;

    if (s1 != NULL && s2 != NULL)
        return strcmp(s1, s2) == 0;

    // This means that exactly one of the args are NULL so they cannot be equal
    return false;
}

config gen_default_config(void)
{
    // Copied from src/config.c
    // Remember to update this everytime you update the actual config
    config tmp = {
        .page = 100,
        .subpage = 1,
        .text_only = false,
        .help = false,
        .help_config = false,
        .version = false,
        .navigation = false,
        .long_navigation = false,
        .no_nav = false,
        .no_top_nav = false,
        .no_bottom_nav = false,
        .no_title = false,
        .no_middle = false,
        .no_sub_page = false,
        .default_colors = false,
        .bg_rgb = { -1, -1, -1 },
        .text_rgb = { -1, -1, -1 },
        .link_rgb = { -1, -1, -1 },
        .time_fmt = NULL
    };
    return tmp;
}

void reset_global_config(void)
{
    global_config = gen_default_config();
}

bool equal_configs(config* conf, config* conf2)
{
    if (!nullsafe_strcmp(conf->time_fmt, conf2->time_fmt))
        return false;

    if (memcmp(conf->bg_rgb, conf2->bg_rgb, sizeof(conf->bg_rgb)) != 0)
        return false;
    if (memcmp(conf->text_rgb, conf2->text_rgb, sizeof(conf->text_rgb)) != 0)
        return false;
    if (memcmp(conf->link_rgb, conf2->link_rgb, sizeof(conf->text_rgb)) != 0)
        return false;

    // Test all non pointer members here
    return conf->help == conf2->help
        && conf->page == conf2->page
        && conf->no_nav == conf2->no_nav
        && conf->version == conf2->version
        && conf->subpage == conf2->subpage
        && conf->no_title == conf2->no_title
        && conf->no_middle == conf2->no_middle
        && conf->text_only == conf2->text_only
        && conf->navigation == conf2->navigation
        && conf->no_top_nav == conf2->no_top_nav
        && conf->no_sub_page == conf2->no_sub_page
        && conf->help_config == conf2->help_config
        && conf->no_bottom_nav == conf2->no_bottom_nav
        && conf->default_colors == conf2->default_colors
        && conf->long_navigation == conf2->long_navigation;
}

bool equal_to_global_config(config* conf)
{
    return equal_configs(conf, &global_config);
}

config_test string_to_config(string str)
{
    config_test test;
    char* text = str.str;

    if (strncmp(text, "# Name: ", 8) != 0) {
        printf("First line needs to start with '# Name: '\n");
        exit(1);
    }

    // Skip '# Name: '
    text += 8;
    char* name_start = text;
    size_t name_len = 0;
    while ((*text++) != '\n')
        name_len++;

    test.name = malloc(name_len + 1);
    memcpy(test.name, name_start, name_len);
    test.name[name_len] = '\0';

    if (strncmp(text, "# Expect:\n", 10) != 0) {
        printf("Second line needs to start with '# Expected:\\n'\n");
        exit(1);
    }

    // Skip '# Expect:' Leave the new line for cheching if the arg is empty
    text += 9;
    char* expect_start = text;
    size_t expect_len = 0;
    for (;; text++) {
        if (*text == '\n' && (*(text + 1) == '\n' || *(text + 1) == '\0')) {
            break;
        }

        expect_len++;
    }

    if (expect_len > 0) {
        // Add one to the expected output length to store the extra new line
        expect_len++;
        test.expected_out = malloc(expect_len + 1);
        // Ignore the first '\n'
        memcpy(test.expected_out, expect_start + 1, expect_len - 1);
        test.expected_out[expect_len] = '\0';
        test.success = false;
    } else {
        test.success = true;
        test.expected_out = NULL;
    }

    size_t rest_len = strlen(text);
    test.config = malloc(rest_len + 1);
    memcpy(test.config, text, rest_len);
    test.config[rest_len] = '\0';

    return test;
}

int get_configs(config_list* configs, char* file_name)
{
    char* file_data = NULL;
    int fd = open(file_name, O_RDONLY);
    struct stat fs;
    fstat(fd, &fs);
    file_data = malloc(fs.st_size + 1);
    read(fd, file_data, fs.st_size);
    file_data[fs.st_size] = '\0';
    close(fd);

    const char* delim = "# ----\n";
    string2d config_strings = str_split(file_data, delim);
    for (size_t idx = 0; idx < config_strings.size; idx++) {
        config_test ct = string_to_config(config_strings.str[idx]);
        if (configs->configs == NULL) {
            configs->configs = malloc(sizeof(config_test));
        } else {
            configs->configs = realloc(configs->configs, sizeof(config_test) * (configs->size + 1));
        }

        configs->configs[configs->size] = ct;
        configs->size++;
    }

    free(file_data);
    free_string2d(&config_strings);

    return 1;
}

#define STDERR_BUFF_SIZE 1024

fd_set set;
char stderr_buffer[STDERR_BUFF_SIZE] = { 0 };
int out_pipe[2];
int saved_stderr;

int non_block_read(/* int pipe, */ char* buffer, int buffer_size)
{

    struct timeval timeout;
    timeout.tv_sec = 0;
    // 0.1 seconds
    timeout.tv_usec = 100 * 1000;

    // "hack" to to get the stderr data out. without the "a" print, it doesn't work for some reason
    fprintf(stderr, "%s", "a");
    int rv = select(out_pipe[0] + 1, &set, NULL, NULL, &timeout);
    if (rv == -1)
        perror("select"); /* an error accured */
    else if (rv == 0) {
        return 0;
    }

    int count = read(out_pipe[0], buffer, buffer_size) - 1; /* there was data to read */
    // Remove the last "a"
    buffer[count] = '\0';
    return count;
}

START_TEST(config_parser_tests)
{
    reset_global_config();
    config conf1 = gen_default_config();
    ck_assert_int_eq(equal_to_global_config(&conf1), true);
}
END_TEST

START_TEST(all_cli_args_success_tests)
{
    reset_global_config();
    // don't use --config since it tries to open a file
    // First arg gets ignored since it's the programs name
    char* tmp[] = { "", "--help", "123", "2", "--text-only", "--help-config", "--version", "--bg-color", "ffffff", "--text-color", "ffffff", "--link-color", "ffffff", "--navigation", "--long-navigation", "--no-nav", "--no-top-nav", "--no-bottom-nav", "--no-title", "--no-middle", "--no-sub-page", "--default-colors", "--show-time", "%d.%m. %H:%M:%S" };
    init_config(24, tmp);
    short trbg[3] = { 1000, 1000, 1000 };
    config conf = gen_default_config();
    conf.page = 123;
    conf.subpage = 2;
    conf.text_only = true;
    conf.help = true;
    conf.help_config = true;
    conf.version = true;
    conf.navigation = true;
    conf.long_navigation = true;
    conf.no_nav = true;
    conf.no_top_nav = true;
    conf.no_bottom_nav = true;
    conf.no_title = true;
    conf.no_middle = true;
    conf.no_sub_page = true;
    conf.default_colors = true;
    memcpy(conf.bg_rgb, trbg, sizeof(trbg));
    memcpy(conf.text_rgb, trbg, sizeof(trbg));
    memcpy(conf.link_rgb, trbg, sizeof(trbg));
    conf.time_fmt = "%d.%m. %H:%M:%S";
    ck_assert_int_eq(equal_to_global_config(&conf), true);
}
END_TEST

bool parse_config_exit_fail(void);
bool init_config_exit_fail(void);

START_TEST(cli_arg_errors)
{
#define COLOR_ARG_ERR(arg, color)          \
    do {                                   \
        char* ctmp[] = { "", arg, color }; \
        init_config(3, ctmp);              \
        char* ctmp2[] = { "", arg };       \
        init_config(2, ctmp2);             \
    } while (0)

    reset_global_config();
    char* tmp[] = { "", "1234" };
    init_config(2, tmp);
    reset_global_config();
    char* tmp2[] = { "", "123", "3333" };
    init_config(3, tmp2);
    /*   ck_assert_int_eq(global_config.page, 1); */
    COLOR_ARG_ERR("--bg-color", "b324tar");
    COLOR_ARG_ERR("--text-color", "b123");
    COLOR_ARG_ERR("--link-color", "b1k");
    ck_assert_int_eq(global_config.page, 1);
}
END_TEST

START_TEST(equality_sanity_test)
{
    reset_global_config();
    config conf1 = gen_default_config();
    ck_assert_int_eq(equal_to_global_config(&conf1), true);
    short trbg[3] = { 255, 255, 255 };
    config conf2 = gen_default_config();
    conf2.page = 123;
    conf2.subpage = 2;
    conf2.text_only = true;
    conf2.help = true;
    conf2.help_config = true;
    conf2.version = true;
    conf2.navigation = true;
    conf2.long_navigation = true;
    conf2.no_nav = true;
    conf2.no_top_nav = true;
    conf2.no_bottom_nav = true;
    conf2.no_title = true;
    conf2.no_middle = true;
    conf2.no_sub_page = true;
    conf2.default_colors = true;
    memcpy(conf2.bg_rgb, trbg, sizeof(trbg));
    memcpy(conf2.text_rgb, trbg, sizeof(trbg));
    memcpy(conf2.link_rgb, trbg, sizeof(trbg));
    conf2.time_fmt = "Not NULL";
    ck_assert_int_eq(equal_to_global_config(&conf2), false);
}
END_TEST

START_TEST(config_file_parsing_tests)
{
    config def_conf = gen_default_config();
    config all = gen_default_config();
    all.text_only = true;
    all.no_nav = true;
    all.no_top_nav = true;
    all.no_bottom_nav = true;
    all.no_title = true;
    all.no_middle = true;
    all.no_sub_page = true;
    all.default_colors = true;
    short abg[] = { 1000, 0, 0 }, atext[] = { 0, 1000, 0 }, alink[] = { 0, 0, 1000 };
    memcpy(all.bg_rgb, abg, sizeof(short) * 3);
    memcpy(all.text_rgb, atext, sizeof(short) * 3);
    memcpy(all.link_rgb, alink, sizeof(short) * 3);
    all.time_fmt = "%d.%m. %H:%M";
    // Succesful options should be first in the config file, this way
    // we can ignore config equality in invalid configs
    config result_configs[] = {
        def_conf,
        def_conf,
        all,
        def_conf
    };

    char* test_files[] = {
        "tests/test_config/001_initial_tests.conf",
        NULL,
    };

    ignore_config_read_during_testing = true;
    memset(stderr_buffer, 0, STDERR_BUFF_SIZE);

    saved_stderr = dup(STDERR_FILENO);

    if (pipe(out_pipe) != 0) {
        exit(1);
    }

    dup2(out_pipe[1], STDERR_FILENO);
    close(out_pipe[1]);

    // Setup a timeout to the stderr
    FD_ZERO(&set);
    FD_SET(out_pipe[0], &set);

    char** config_file = test_files;
    while (*config_file != NULL) {
        config_list cl;
        cl.configs = NULL;
        cl.size = 0;

        printf("parsing file %s\n", *config_file);
        get_configs(&cl, *config_file);

        for (size_t idx = 0; idx < cl.size; idx++) {
            reset_global_config();
            memset(stderr_buffer, 0, STDERR_BUFF_SIZE);
            fflush(NULL);
            config_test t_config = cl.configs[idx];
            bool success = parse_config_file(t_config.config);

            if (t_config.expected_out == NULL) {
                ck_assert_int_eq(success, true);
                ck_assert_int_eq(equal_to_global_config(&result_configs[idx]), true);
            } else {
                ck_assert_int_eq(success, false);
            }

            int r = non_block_read(stderr_buffer, STDERR_BUFF_SIZE);
            if (r != 0) {
                stderr_buffer[r] = '\0';
                int comp = strcmp(stderr_buffer, t_config.expected_out);
                if (comp != 0) {
                    printf("Unexpected error message in '%s':\n%s\n", t_config.name, stderr_buffer);
                }
                ck_assert_int_eq(strcmp(stderr_buffer, t_config.expected_out), 0);
            }
        }
        free_config_list(&cl);
        config_file++;
    }
}
END_TEST

Suite* parser_suite(void)
{
    Suite* s;
    TCase* tc_core;

    s = suite_create("Config File parser");
    tc_core = tcase_create("Config File parser Core");

    tcase_add_test(tc_core, equality_sanity_test);
    tcase_add_test(tc_core, config_parser_tests);
    tcase_add_test(tc_core, all_cli_args_success_tests);
    tcase_add_test(tc_core, config_file_parsing_tests);
    tcase_add_exit_test(tc_core, cli_arg_errors, 1);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    ignore_config_read_during_testing = true;
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
