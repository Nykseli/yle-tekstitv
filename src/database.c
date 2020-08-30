
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "database.h"

#define DB_VERSION 0x0001

#define DB_MAG0 0x00 /* File identification byte 0 index */
#define TELMAG0 0x7f /* Magic number byte 0 */

#define DB_MAG1 0x01 /* File identification byte 1 index */
#define TELMAG1 0x74 /* Magic number byte 1 */

#define DB_MAG2 0x02 /* File identification byte 2 index */
#define TELMAG2 0x65 /* Magic number byte 2 */

#define DB_MAG3 0x03 /* File identification byte 3 index */
#define TELMAG3 0x6c /* Magic number byte 3 */

#define DB_MAG4 0x04 /* File identification byte 4 index */
#define TELMAG4 0x65 /* Magic number byte 4 */

#define DB_MAG5 0x05 /* File identification byte 5 index */
#define TELMAG5 0x64 /* Magic number byte 5 */

#define DB_MAG6 0x06 /* File identification byte 6 index */
#define TELMAG6 0x62 /* Magic number byte 6 */

/* Conglomeration of the identification bytes, for easy testing as a word. */
#define TELMAG "\177teledb"
#define SELFMAG 7

#define DB_CLASS 0x07
#define DB_DATA 0x08

typedef struct {
    uint8_t db_ident[DB_DATA + 1];
    short db_version;
    int db_count;
} tele_header;

static tele_header header = {
    .db_ident = { TELMAG0, TELMAG1, TELMAG2, TELMAG3, TELMAG4, TELMAG5, TELMAG6, 2, 1 },
    .db_version = DB_VERSION,
    .db_count = 0,
};

static tele_entries entries;

/* Is the newest data from file loaded */
static bool newest_data = false;

static size_t get_home_dir(char* buff)
{
    char* homedir;
    // Try to find the home path from $HOME variable
    // If and if not found, use the uid
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }

    size_t path_len = strlen(homedir);
    // Let's hope the file path is under 1024 characters
    if (path_len + 34 >= 1024) {
        fprintf(stderr, "Cannot open database. File path is over 1024 characters.");
        exit(1);
    }

    memcpy(buff, homedir, path_len);
    return path_len;
}

/**
 * Make sure that all the directories and files exist and can be accessed(?)
 * Also add the tele_header to the file if a new db file is created.
 */
static void init_db_file()
{
    char db_path[1024];
    struct stat folder_stat;

    // Try to find the config path for the database.
    // Try to create the folder path if it's not found
    size_t path_len = get_home_dir(db_path);
    memcpy(db_path + path_len, "/.config/tekstitv/", 19);
    path_len += 18;

    int res = stat(db_path, &folder_stat);
    if (res == -1) {
        // read/write/search permissions for owner and group, and with read/search permissions for others.
        // Basically the default values when creating directory with mkdir command
        res = mkdir(db_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (res == -1) {
            fprintf(stderr, "Error creating folder '~/.config/tekstitv/': '%s'\n", strerror(errno));
            exit(1);
        }
    }

    memcpy(db_path + path_len, "database.teledb", 16);
    res = stat(db_path, &folder_stat);
    // No need to do anything if the file exists and we can access it
    if (res != -1)
        return;

    int fd = open(db_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        fprintf(stderr, "Error opening database '~/.config/tekstitv/database.teledb': '%s'\n", strerror(errno));
        exit(1);
    }

    // We want to add the header to the new db
    write(fd, &header, sizeof(tele_header));
    close(fd);
}

static int get_db_fd()
{
    char db_path[1024];
    size_t path_len = get_home_dir(db_path);
    memcpy(db_path + path_len, "/.config/tekstitv/database.teledb", 34);

    int fd = open(db_path, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        fprintf(stderr, "Error opening database '~/.config/tekstitv/database.teledb': '%s'\n", strerror(errno));
        exit(1);
    }

    return fd;
}

/* Load data from database */
tele_entries teledb_load_data()
{
    // Only load data from the file if we need to.
    // Newest data is set to true in teledb_commit_data()
    if (newest_data)
        goto return_data;

    int file = get_db_fd();
    struct stat fs;
    fstat(file, &fs);
    uint8_t* file_buffer = (uint8_t*)mmap(NULL, fs.st_size, PROT_READ, MAP_PRIVATE, file, 0);
    memcpy(&header, file_buffer, sizeof(tele_header));

    // TODO: make sure that the file header is valid

    // Free the old entries if there is any
    if (entries.db_entries != NULL)
        free(entries.db_entries);

    entries.db_count = header.db_count;
    entries.db_entries = malloc(sizeof(tele_entry) * entries.db_count);

    uint32_t offset = sizeof(tele_header);
    for (int i = 0; i < entries.db_count; i++) {
        memmove(entries.db_entries + i, file_buffer + offset, sizeof(tele_entry));
        offset += sizeof(tele_entry);
    }

    munmap(file_buffer, fs.st_size);
    close(file);
    newest_data = true;

return_data:
    return entries;
}

/*
 * Delete database entry based on index. Starting from 0.
 * Set commit parameter to true if you want to autocommit.
 *
 * Return -1 if delete failed, 0 if successfull
 */
int teledb_delete_entry(int index)
{
    // Cannot delete an entry if it doesn't exist
    if (index > entries.db_count - 1)
        return -1;

    // "Remove" the entry by overriding it with the entries after it
    // We don't need actually need to remove or free it.
    // We will make the list size smaller so it can stay as a "dead" entry
    for (int i = index; i < entries.db_count - 1; i++) {
        entries.db_entries[i] = entries.db_entries[i + 1];
    }
    entries.db_count--;
    return 0;
}

// TODO: fail error?
void teledb_add_page(char* page, char* desc)
{
    size_t page_len = strlen(page);
    size_t desc_len = strlen(desc);
    assert(page_len == DB_PAGE_NUM_LEN);
    assert(desc_len <= DB_DESCRIPT_LEN);

    // Make sure that we are adding the page to the newest db data
    entries = teledb_load_data();

    tele_entry te;
    te.page_num_size = page_len;
    te.descript_size = desc_len;
    memcpy(te.page_num, page, page_len);
    te.page_num[page_len] = '\0';
    memcpy(te.descript, desc, desc_len);
    te.descript[desc_len] = '\0';

    entries.db_entries = realloc(entries.db_entries, sizeof(tele_entry) * (entries.db_count + 1));
    entries.db_entries[entries.db_count] = te;
    entries.db_count++;
}

/**
 * Move the data entry up or down.
 * First entry cannot be moved up and last cannot be moved down.
 *
 * Returns -1 on fail, 0 on success
 */
int teledb_move_entry(int index, tele_move_direction direction)
{
    // No negativity allowed here
    if (index < 0)
        return -1;

    // Cannot move anything if there is 0 or 1 entries
    if (entries.db_count < 2)
        return -1;

    if (direction == TELE_MOVE_UP) {
        // First cannot be moved up
        if (index == 0)
            return -1;

        tele_entry tmp = entries.db_entries[index];
        entries.db_entries[index] = entries.db_entries[index - 1];
        entries.db_entries[index - 1] = tmp;

    } else if (direction == TELE_MOVE_DOWN) {
        // Last cannot be moved down
        if (index == entries.db_count - 1)
            return -1;

        tele_entry tmp = entries.db_entries[index];
        entries.db_entries[index] = entries.db_entries[index + 1];
        entries.db_entries[index + 1] = tmp;
    }

    return 0;
}

/* Commit data to the databse */
void teledb_commit_data()
{
    int file = get_db_fd();
    uint8_t* buffer = malloc(sizeof(tele_header) + sizeof(tele_entry) * entries.db_count);
    header.db_count = entries.db_count;
    memcpy(buffer, &header, sizeof(tele_header));

    uint32_t offset = sizeof(tele_header);
    for (int i = 0; i < entries.db_count; i++) {
        tele_entry ent = entries.db_entries[i];
        ent.id = i + 1;
        memcpy(buffer + offset, &ent, sizeof(tele_entry));
        offset += sizeof(tele_entry);
    }

    write(file, buffer, offset);
    free(buffer);
    close(file);
    // Data needs to be reloaded after commiting the data
    newest_data = false;
}

void teledb_init_database()
{
    init_db_file();
    entries.db_count = 0;
    entries.db_entries = NULL;
    entries = teledb_load_data();
}

void teledb_free_database()
{
    // Free the old entries if there is any
    if (entries.db_entries != NULL)
        free(entries.db_entries);
}
