#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

/* ============================================================
   CONSTANTS & DATA STRUCTURE
   ============================================================ */
#define MAX_ROWS   500
#define MAX_COLS   30
#define MAX_LEN    200
#define MAX_PATH   512

/* Generic Table - works for ANY dataset with ANY headers */
typedef struct {
    char folder[MAX_PATH];            /* source folder path  */
    char keys[MAX_COLS][MAX_LEN];     /* short keys  e.g. "RollNo" */
    char headers[MAX_COLS][MAX_LEN];  /* full names  e.g. "Roll No" */
    char data[MAX_ROWS][MAX_COLS][MAX_LEN]; /* cell values */
    int  num_rows;
    int  num_cols;
    int  modified;                    /* dirty flag */
} Table;

/* ============================================================
   UTILITY: trim trailing comma, space, newline
   ============================================================ */
void trim(char *s) {
    int len = strlen(s);
    while (len > 0 &&
           (s[len-1]=='\n' || s[len-1]=='\r' ||
            s[len-1]==' '  || s[len-1]==',')) {
        s[--len] = '\0';
    }
}

/* ============================================================
   LOAD: read all .txt files from folder into Table
   ============================================================ */
int load_table(Table *t, const char *folder) {
    char path[MAX_PATH];
    char line[MAX_LEN];
    FILE *f;

    memset(t, 0, sizeof(Table));
    strncpy(t->folder, folder, MAX_PATH-1);

    /* --- read 0.txt for schema --- */
    snprintf(path, MAX_PATH, "%s/0.txt", folder);
    f = fopen(path, "r");
    if (!f) { printf("ERROR: Cannot open %s\n", path); return -1; }

    t->num_cols = 0;
    while (fgets(line, MAX_LEN, f)) {
        trim(line);
        if (strlen(line) == 0) continue;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        strncpy(t->keys[t->num_cols],    line,      MAX_LEN-1);
        strncpy(t->headers[t->num_cols], colon + 1, MAX_LEN-1);
        t->num_cols++;
    }
    fclose(f);

    /* --- read 1.txt, 2.txt ... for data rows --- */
    t->num_rows = 0;
    int file_num = 1;
    while (1) {
        snprintf(path, MAX_PATH, "%s/%d.txt", folder, file_num);
        f = fopen(path, "r");
        if (!f) break;   /* no more files */

        int row = t->num_rows;
        while (fgets(line, MAX_LEN, f)) {
            trim(line);
            if (strlen(line) == 0) continue;
            char *colon = strchr(line, ':');
            if (!colon) continue;
            *colon = '\0';
            char *key = line;
            char *val = colon + 1;
            /* find matching column */
            for (int c = 0; c < t->num_cols; c++) {
                if (strcmp(t->keys[c], key) == 0) {
                    strncpy(t->data[row][c], val, MAX_LEN-1);
                    break;
                }
            }
        }
        fclose(f);
        t->num_rows++;
        file_num++;
    }

    printf("Loaded %d rows, %d columns from '%s'\n",
           t->num_rows, t->num_cols, folder);
    return 0;
}

/* ============================================================
   DISPLAY: print table nicely
   ============================================================ */
void display_table(Table *t) {
    if (t->num_rows == 0) { printf("(empty table)\n"); return; }

    /* print header row */
    printf("\n%-5s", "#");
    for (int c = 0; c < t->num_cols; c++)
        printf("| %-18s", t->headers[c]);
    printf("\n");

    /* separator */
    for (int c = 0; c <= t->num_cols; c++) printf("%-20s", "--------------------");
    printf("\n");

    /* data rows */
    for (int r = 0; r < t->num_rows; r++) {
        printf("%-5d", r+1);
        for (int c = 0; c < t->num_cols; c++)
            printf("| %-18s", t->data[r][c]);
        printf("\n");
    }
    printf("\n");
}

/* ============================================================
   SORT: bubble sort w.r.t any column (by header name)
   ============================================================ */
void sort_table(Table *t) {
    clock_t start = clock();

    printf("Available columns:\n");
    for (int c = 0; c < t->num_cols; c++)
        printf("  %d. %s\n", c+1, t->headers[c]);
    printf("Enter column number to sort by: ");
    int choice; scanf("%d", &choice);
    if (choice < 1 || choice > t->num_cols) {
        printf("Invalid choice.\n"); return;
    }
    int col = choice - 1;

    /* bubble sort */
    char tmp[MAX_COLS][MAX_LEN];
    for (int i = 0; i < t->num_rows - 1; i++) {
        for (int j = 0; j < t->num_rows - i - 1; j++) {
            if (strcmp(t->data[j][col], t->data[j+1][col]) > 0) {
                memcpy(tmp,           t->data[j],   sizeof(tmp));
                memcpy(t->data[j],    t->data[j+1], sizeof(tmp));
                memcpy(t->data[j+1],  tmp,          sizeof(tmp));
            }
        }
    }

    clock_t end = clock();
    printf("Sorted by '%s'  [Time: %.6f sec]\n",
           t->headers[col],
           (double)(end - start) / CLOCKS_PER_SEC);
    t->modified = 1;
    display_table(t);
}

/* ============================================================
   INSERT: add a new record into RAM
   ============================================================ */
void insert_record(Table *t) {
    clock_t start = clock();

    if (t->num_rows >= MAX_ROWS) {
        printf("Table full.\n"); return;
    }
    int row = t->num_rows;
    printf("Enter values for new record:\n");
    for (int c = 0; c < t->num_cols; c++) {
        printf("  %s: ", t->headers[c]);
        scanf(" %[^\n]", t->data[row][c]);
    }
    t->num_rows++;
    t->modified = 1;

    clock_t end = clock();
    printf("Record inserted.  [Time: %.6f sec]\n",
           (double)(end - start) / CLOCKS_PER_SEC);
}

/* ============================================================
   DELETE: remove a record by row number
   ============================================================ */
void delete_record(Table *t) {
    clock_t start = clock();

    display_table(t);
    printf("Enter row number to delete: ");
    int row; scanf("%d", &row);
    if (row < 1 || row > t->num_rows) {
        printf("Invalid row.\n"); return;
    }
    row--; /* 0-indexed */

    /* shift rows up */
    for (int r = row; r < t->num_rows - 1; r++)
        memcpy(t->data[r], t->data[r+1], sizeof(t->data[r]));
    t->num_rows--;
    t->modified = 1;

    clock_t end = clock();
    printf("Record deleted.  [Time: %.6f sec]\n",
           (double)(end - start) / CLOCKS_PER_SEC);
}

/* ============================================================
   UPDATE: change a specific cell
   ============================================================ */
void update_record(Table *t) {
    clock_t start = clock();

    display_table(t);
    printf("Enter row number to update: ");
    int row; scanf("%d", &row);
    if (row < 1 || row > t->num_rows) { printf("Invalid.\n"); return; }
    row--;

    printf("Available columns:\n");
    for (int c = 0; c < t->num_cols; c++)
        printf("  %d. %s\n", c+1, t->headers[c]);
    printf("Enter column number to update: ");
    int col; scanf("%d", &col);
    if (col < 1 || col > t->num_cols) { printf("Invalid.\n"); return; }
    col--;

    printf("Current value: %s\n", t->data[row][col]);
    printf("New value: ");
    scanf(" %[^\n]", t->data[row][col]);
    t->modified = 1;

    clock_t end = clock();
    printf("Record updated.  [Time: %.6f sec]\n",
           (double)(end - start) / CLOCKS_PER_SEC);
}

/* ============================================================
   SAVE: write RAM data back to files
   ============================================================ */
void save_table(Table *t) {
    clock_t start = clock();
    char path[MAX_PATH];

    for (int r = 0; r < t->num_rows; r++) {
        snprintf(path, MAX_PATH, "%s/%d.txt", t->folder, r+1);
        FILE *f = fopen(path, "w");
        if (!f) { printf("Cannot write %s\n", path); continue; }
        for (int c = 0; c < t->num_cols; c++)
            fprintf(f, "%s:%s,\n", t->keys[c], t->data[r][c]);
        fclose(f);
    }
    t->modified = 0;
    clock_t end = clock();
    printf("Saved to '%s'.  [Time: %.6f sec]\n",
           t->folder,
           (double)(end - start) / CLOCKS_PER_SEC);
}

/* ============================================================
   JOIN: inner join two tables on a common key
   ============================================================ */
void inner_join(Table *t1, Table *t2) {
    clock_t start = clock();

    /* ask which column from each table to join on */
    printf("\n-- Table 1 columns --\n");
    for (int c = 0; c < t1->num_cols; c++)
        printf("  %d. %s\n", c+1, t1->headers[c]);
    printf("Join column from Table 1: ");
    int c1; scanf("%d", &c1); c1--;

    printf("\n-- Table 2 columns --\n");
    for (int c = 0; c < t2->num_cols; c++)
        printf("  %d. %s\n", c+1, t2->headers[c]);
    printf("Join column from Table 2: ");
    int c2; scanf("%d", &c2); c2--;

    /* print header */
    printf("\n[INNER JOIN result]\n");
    for (int c = 0; c < t1->num_cols; c++) printf("| %-15s", t1->headers[c]);
    for (int c = 0; c < t2->num_cols; c++) {
        if (c == c2) continue;
        printf("| %-15s", t2->headers[c]);
    }
    printf("\n");

    int count = 0;
    for (int r1 = 0; r1 < t1->num_rows; r1++) {
        for (int r2 = 0; r2 < t2->num_rows; r2++) {
            if (strcmp(t1->data[r1][c1], t2->data[r2][c2]) == 0) {
                for (int c = 0; c < t1->num_cols; c++)
                    printf("| %-15s", t1->data[r1][c]);
                for (int c = 0; c < t2->num_cols; c++) {
                    if (c == c2) continue;
                    printf("| %-15s", t2->data[r2][c]);
                }
                printf("\n");
                count++;
            }
        }
    }
    clock_t end = clock();
    printf("\n%d matching rows.  [Time: %.6f sec]\n",
           count, (double)(end - start) / CLOCKS_PER_SEC);
}

/* ============================================================
   LEFT OUTER JOIN
   ============================================================ */
void left_join(Table *t1, Table *t2) {
    clock_t start = clock();

    printf("\n-- Table 1 columns --\n");
    for (int c = 0; c < t1->num_cols; c++)
        printf("  %d. %s\n", c+1, t1->headers[c]);
    printf("Join column from Table 1: ");
    int c1; scanf("%d", &c1); c1--;

    printf("\n-- Table 2 columns --\n");
    for (int c = 0; c < t2->num_cols; c++)
        printf("  %d. %s\n", c+1, t2->headers[c]);
    printf("Join column from Table 2: ");
    int c2; scanf("%d", &c2); c2--;

    printf("\n[LEFT JOIN result]\n");
    for (int c = 0; c < t1->num_cols; c++) printf("| %-15s", t1->headers[c]);
    for (int c = 0; c < t2->num_cols; c++) {
        if (c == c2) continue;
        printf("| %-15s", t2->headers[c]);
    }
    printf("\n");

    for (int r1 = 0; r1 < t1->num_rows; r1++) {
        int matched = 0;
        for (int r2 = 0; r2 < t2->num_rows; r2++) {
            if (strcmp(t1->data[r1][c1], t2->data[r2][c2]) == 0) {
                for (int c = 0; c < t1->num_cols; c++)
                    printf("| %-15s", t1->data[r1][c]);
                for (int c = 0; c < t2->num_cols; c++) {
                    if (c == c2) continue;
                    printf("| %-15s", t2->data[r2][c]);
                }
                printf("\n");
                matched = 1;
            }
        }
        if (!matched) {
            for (int c = 0; c < t1->num_cols; c++)
                printf("| %-15s", t1->data[r1][c]);
            for (int c = 0; c < t2->num_cols; c++) {
                if (c == c2) continue;
                printf("| %-15s", "NULL");
            }
            printf("\n");
        }
    }
    clock_t end = clock();
    printf("[Time: %.6f sec]\n", (double)(end - start) / CLOCKS_PER_SEC);
}

/* ============================================================
   MINI QUERY LANGUAGE (Q2b)
   Syntax:
     SELECT <col1,col2,...|*> FROM <1|2>
     SELECT <col1,...|*> FROM <1|2> WHERE <col>=<val>
     JOIN INNER|LEFT <col_t1> <col_t2>
   ============================================================ */
void run_query(Table *t1, Table *t2) {
    clock_t start = clock();
    char query[512];
    printf("\nEnter query: ");
    scanf(" %[^\n]", query);

    /* --- SELECT --- */
    if (strncmp(query, "SELECT", 6) == 0) {
        /* parse: SELECT cols FROM tnum [WHERE col=val] */
        char cols_str[256], tnum_str[4], where_col[64], where_val[64];
        int has_where = 0;
        cols_str[0] = tnum_str[0] = where_col[0] = where_val[0] = '\0';

        /* check for WHERE */
        char *where_ptr = strstr(query, " WHERE ");
        if (where_ptr) {
            has_where = 1;
            sscanf(where_ptr + 7, "%[^=]=%s", where_col, where_val);
            where_ptr = '\0'; / cut query before WHERE */
        }

        sscanf(query, "SELECT %s FROM %s", cols_str, tnum_str);
        Table *t = (atoi(tnum_str) == 1) ? t1 : t2;

        /* resolve columns to print */
        int print_cols[MAX_COLS], num_print = 0;
        if (strcmp(cols_str, "*") == 0) {
            for (int c = 0; c < t->num_cols; c++)
                print_cols[num_print++] = c;
        } else {
            char *tok = strtok(cols_str, ",");
            while (tok) {
                for (int c = 0; c < t->num_cols; c++) {
                    if (strcmp(t->keys[c], tok) == 0 ||
                        strcmp(t->headers[c], tok) == 0) {
                        print_cols[num_print++] = c;
                        break;
                    }
                }
                tok = strtok(NULL, ",");
            }
        }

        /* find WHERE column index */
        int wcol = -1;
        if (has_where) {
            for (int c = 0; c < t->num_cols; c++) {
                if (strcmp(t->keys[c], where_col) == 0 ||
                    strcmp(t->headers[c], where_col) == 0) {
                    wcol = c; break;
                }
            }
        }

        /* print header */
        for (int i = 0; i < num_print; i++)
            printf("| %-18s", t->headers[print_cols[i]]);
        printf("\n");

        /* print rows */
        int count = 0;
        for (int r = 0; r < t->num_rows; r++) {
            if (has_where && wcol >= 0 &&
                strcmp(t->data[r][wcol], where_val) != 0)
                continue;
            for (int i = 0; i < num_print; i++)
                printf("| %-18s", t->data[r][print_cols[i]]);
            printf("\n");
            count++;
        }
        clock_t end = clock();
        printf("\n%d row(s) returned.  [Time: %.6f sec]\n",
               count, (double)(end - start) / CLOCKS_PER_SEC);

    /* --- JOIN shortcut --- */
    } else if (strncmp(query, "JOIN", 4) == 0) {
        char type[16];
        sscanf(query, "JOIN %s", type);
        if (strcmp(type, "INNER") == 0) inner_join(t1, t2);
        else if (strcmp(type, "LEFT") == 0)  left_join(t1, t2);
        else printf("Unknown join type. Use INNER or LEFT.\n");

    } else {
        printf("Unknown query. Supported:\n");
        printf("  SELECT * FROM 1\n");
        printf("  SELECT Name,Maths FROM 1 WHERE RollNo=250231048\n");
        printf("  JOIN INNER\n  JOIN LEFT\n");
    }
}

/* ============================================================
   MAIN MENU
   ============================================================ */
int main() {
    Table t1, t2;
    int loaded1 = 0, loaded2 = 0;
    int choice;

    printf("===========================================\n");
    printf("   Generic Data System - DSUC+DBMS Assign \n");
    printf("===========================================\n");

    while (1) {
        printf("\n--- MAIN MENU ---\n");
        printf(" 1. Load MCASampleData1\n");
        printf(" 2. Load MCASampleData2\n");
        printf(" 3. Display Table 1\n");
        printf(" 4. Display Table 2\n");
        printf(" 5. Sort Table 1\n");
        printf(" 6. Sort Table 2\n");
        printf(" 7. Insert into Table 1\n");
        printf(" 8. Insert into Table 2\n");
        printf(" 9. Delete from Table 1\n");
        printf("10. Delete from Table 2\n");
        printf("11. Update Table 1\n");
        printf("12. Update Table 2\n");
        printf("13. Save Table 1 to files\n");
        printf("14. Save Table 2 to files\n");
        printf("15. Inner Join\n");
        printf("16. Left Join\n");
        printf("17. Run Query (mini SQL)\n");
        printf(" 0. Exit\n");
        printf("Choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:  load_table(&t1, "MCASampleData1"); loaded1=1; break;
            case 2:  load_table(&t2, "MCASampleData2"); loaded2=1; break;
            case 3:  if(loaded1) display_table(&t1); else printf("Load first.\n"); break;
            case 4:  if(loaded2) display_table(&t2); else printf("Load first.\n"); break;
            case 5:  if(loaded1) sort_table(&t1);    else printf("Load first.\n"); break;
            case 6:  if(loaded2) sort_table(&t2);    else printf("Load first.\n"); break;
            case 7:  if(loaded1) insert_record(&t1); else printf("Load first.\n"); break;
            case 8:  if(loaded2) insert_record(&t2); else printf("Load first.\n"); break;
            case 9:  if(loaded1) delete_record(&t1); else printf("Load first.\n"); break;
            case 10: if(loaded2) delete_record(&t2); else printf("Load first.\n"); break;
            case 11: if(loaded1) update_record(&t1); else printf("Load first.\n"); break;
            case 12: if(loaded2) update_record(&t2); else printf("Load first.\n"); break;
            case 13: if(loaded1) save_table(&t1);    else printf("Load first.\n"); break;
            case 14: if(loaded2) save_table(&t2);    else printf("Load first.\n"); break;
            case 15: if(loaded1&&loaded2) inner_join(&t1,&t2); else printf("Load both.\n"); break;
            case 16: if(loaded1&&loaded2) left_join(&t1,&t2);  else printf("Load both.\n"); break;
            case 17: if(loaded1&&loaded2) run_query(&t1,&t2);  else printf("Load both.\n"); break;
            case 0:
                if (t1.modified || t2.modified)
                    printf("Warning: unsaved changes exist!\n");
                printf("Goodbye.\n"); return 0;
            default: printf("Invalid choice.\n");
        }
    }
}
