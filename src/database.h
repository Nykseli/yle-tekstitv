/**
 * yle-tekstitv has it's own buildin database structure for
 * storing user's favourite sites.
 * See DATABASE.md for more information of the general structre etc.
 */

#include <stdint.h>

/* Maximum length of the description db field*/
#define DB_DESCRIPT_LEN 40
/* Maximum length of the page_num db field*/
#define DB_PAGE_NUM_LEN 3

typedef struct {
    uint16_t id;
    uint8_t page_num_size;
    uint8_t descript_size;
    // + 1 to make sure there are room for null character
    char page_num[DB_PAGE_NUM_LEN + 1];
    char descript[DB_DESCRIPT_LEN + 1];
} tele_entry;

typedef struct {
    tele_entry* db_entries;
    int db_count;
} tele_entries;

typedef enum {
    TELE_MOVE_UP,
    TELE_MOVE_DOWN
} tele_move_direction;

/* Load data from database */
tele_entries teledb_load_data();

/*
 * Delete database entry based on index. Starting from 0.
 *
 * Return -1 if delete failed, 0 if successfull
 */
int teledb_delete_entry(int index);

/**
 * Move the data entry up or down.
 * First entry cannot be moved up and last cannot be moved down.
 *
 * Returns -1 on fail, 0 on success
 */
int teledb_move_entry(int index, tele_move_direction direction);

/* Commit data to the databse */
void teledb_commit_data();

void teledb_init_database();
void teledb_free_database();
