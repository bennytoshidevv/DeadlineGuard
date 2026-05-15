#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 *  CONSTANTS
 * ============================================================ */
#define MAX_TITLE_LEN       100
#define MAX_SUBJECT_LEN      50
#define MAX_TYPE_LEN         20
#define MAX_DATE_LEN         15
#define MAX_EXPR_LEN        200
#define MAX_DAYS              7
#define MAX_SCHED_PER_DAY    10   /* max subjects per day in weekly schedule */
#define URGENT_DAYS           3

/* ============================================================
 *  STRUCTS
 * ============================================================ */

/* --- Task: represents one assignment, exam, project, quiz, or activity --- */
typedef struct Task {
    int  id;
    char title[MAX_TITLE_LEN];
    char subject[MAX_SUBJECT_LEN];
    char type[MAX_TYPE_LEN];      /* assignment/exam/project/quiz/activity */
    char dueDate[MAX_DATE_LEN];   /* format: YYYY-MM-DD                    */
    int  priority;                /* 1=Urgent  2=Upcoming  3=Safe          */
    int  isDone;                  /* 0=pending  1=done                     */
} Task;

/* --- Doubly Linked List node for the main task list --- */
typedef struct DLLNode {
    Task data;
    struct DLLNode *prev;
    struct DLLNode *next;
} DLLNode;

/* --- Stack node for the undo system --- */
typedef struct StackNode {
    char actionType[10];   /* "ADD", "DELETE", "DONE" */
    Task snapshot;         /* copy of the task before the action */
    struct StackNode *next;
} StackNode;

/* --- Queue node for pending submissions --- */
typedef struct QueueNode {
    Task data;
    struct QueueNode *next;
} QueueNode;

/* --- Queue structure (front and rear pointers) --- */
typedef struct {
    QueueNode *front;
    QueueNode *rear;
    int size;
} Queue;

/*
 * ScheduleEntry: one subject/time slot for a given day.
 * A day can now hold multiple entries (up to MAX_SCHED_PER_DAY).
 */
typedef struct {
    char subject[MAX_SUBJECT_LEN];
    char time[30];   /* holds range format e.g. "7:30 AM - 9:30 AM" */
} ScheduleEntry;

/* --- Circular Doubly Linked List node for weekly schedule --- */
typedef struct SchedNode {
    char          day[15];
    ScheduleEntry entries[MAX_SCHED_PER_DAY];   /* multiple subjects per day */
    int           entryCount;                    /* how many subjects today   */
    struct SchedNode *prev;
    struct SchedNode *next;
} SchedNode;

/* --- Priority Queue node (sorted linked list, min = nearest date) --- */
typedef struct PQNode {
    Task data;
    struct PQNode *next;
} PQNode;

/* --- Binary Search Tree node (keyed by subject name) --- */
typedef struct BSTNode {
    Task data;
    struct BSTNode *left;
    struct BSTNode *right;
} BSTNode;

/* --- General Tree node for Subject → Topic → Subtopic --- */
typedef struct TreeNode {
    char name[MAX_TITLE_LEN];
    struct TreeNode *firstChild;   /* first child node  */
    struct TreeNode *nextSibling;  /* next sibling node */
} TreeNode;

/* ============================================================
 *  GLOBAL VARIABLES
 * ============================================================ */
DLLNode   *taskHead    = NULL;   /* head of the main task doubly linked list */
StackNode *undoTop     = NULL;   /* top of the undo stack                    */
Queue      pendingQ;             /* queue for pending submissions             */
PQNode    *pqHead      = NULL;   /* head of the priority queue               */
BSTNode   *bstRoot     = NULL;   /* root of the binary search tree           */
TreeNode  *subjectRoot = NULL;   /* root of the subject hierarchy tree       */
SchedNode *schedHead   = NULL;   /* head of the circular schedule list       */
int        nextID      = 1;      /* auto-increment task ID                   */

/* ============================================================
 *  FORWARD DECLARATIONS
 *  These are needed because printTaskRow (defined early, inside
 *  the table-helper block) calls daysUntilDue and truncate, which
 *  are defined later in the file.
 * ============================================================ */
int  daysUntilDue(const char *dueDate);
void truncate(char *dst, const char *src, int maxLen);

/* ============================================================
 *  UTILITY FUNCTIONS
 * ============================================================ */

/* Clear the input buffer to avoid leftover newlines */
void clearInputBuffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}


/* Narrow divider — used only for non-table boxes (header, menu, etc.) */
void printDivider() {
    printf("  +----------------------------------------------------------+\n");
}

/* Thick divider for the banner */
void printThickDivider() {
    printf("  ============================================================\n");
}

/* Wide table rules — computed to exactly match printTaskRow column widths */
/* Row: '  | ID(4) | Priority(13) | Title(22) | Subject(12) | Type(10) | DueDate(10) | Days(8) | Status(9) |' */
#define TBL_RULE \
    "  +------+---------------+------------------------+--------------+------------+------------+----------+-----------+\n"

void printTableTop()    { printf(TBL_RULE); }
void printTableSep()    { printf(TBL_RULE); }
void printTableBottom() { printf(TBL_RULE); }

/*
 * printTableHeader: column labels aligned to the same widths as printTaskRow.
 *   | ID   | Priority      | Title                  | Subject      | Type       | Due Date   | Days Left | Status    |
 */
void printTableHeader() {
    printTableTop();
    printf("  | %-4s | %-13s | %-22s | %-12s | %-10s | %-10s | %-8s | %-9s |\n",
           "ID", "Priority", "Title", "Subject", "Type", "Due Date", "Days", "Status");
    printTableSep();
}

/*
 * getPriorityLabel: badge text exactly 13 chars wide (fits the Priority column).
 */
const char *getPriorityLabel(int priority) {
    if (priority == 1) return "[!! URGENT   ]";
    if (priority == 2) return "[ ! UPCOMING ]";
    if (priority == 0) return "[    DONE    ]";
    return  "[    SAFE    ]";
}

/*
 * printTaskRow: one data row — columns match printTableHeader exactly.
 *   | ID   | Priority      | Title                  | Subject      | Type       | Due Date   | Days Left | Status    |
 */
void printTaskRow(Task *t) {
    int  days = daysUntilDue(t->dueDate);
    char title[23], subject[13], type[11];

    truncate(title,   t->title,   22);
    truncate(subject, t->subject, 12);
    truncate(type,    t->type,    10);

    char daysBuf[20];
    if (t->isDone)
        snprintf(daysBuf, sizeof(daysBuf), "done");
    else if (days < 0)
        snprintf(daysBuf, sizeof(daysBuf), "OVR %dd", -days);
    else
        snprintf(daysBuf, sizeof(daysBuf), "%d day(s)", days);

    const char *statusStr = t->isDone ? "DONE     " : "PENDING  ";
    const char *priLabel  = getPriorityLabel(t->isDone ? 0 : t->priority);

    printf("  | %-4d | %-13s | %-22s | %-12s | %-10s | %-10s | %-8s | %-9s |\n",
           t->id, priLabel, title, subject, type,
           t->dueDate, daysBuf, statusStr);
}

/*
 * printTaskSummary: footer summary line inside the same wide table border.
 * Inner width = 111. Fixed prefix = "  Total: XXX   Pending: XXX   Done: XXX" = 39 chars.
 * Trailing pipe needs 71 spaces of padding: 111 - 39 - 1 = 71.
 */
void printTaskSummary(int total, int pending, int done) {
    printTableBottom();
    printf("  |  Total: %-3d   Pending: %-3d   Done: %-3d"
           "                                                                       |\n",
           total, pending, done);
    printTableBottom();
}

/*
 * printHeader: section title using the same wide rule as the task table,
 * so the header border always aligns with the table below it.
 * Inner content = 105 chars between '|  >> ' and '|'.
 */
void printHeader(const char *title) {
    char buf[106];
    int len = (int)strlen(title);
    if (len > 105) {
        strncpy(buf, title, 103);
        buf[103] = '.'; buf[104] = '.'; buf[105] = '\0';
    } else {
        strncpy(buf, title, 105);
        buf[len] = '\0';
    }
    printf("\n");
    printf(TBL_RULE);
    printf("  |  >> %-105s|\n", buf);
    printf(TBL_RULE);
}

/*
 * printBanner: Pure ASCII art banner — no Unicode, works on any terminal.
 * Letters are built using only # and space characters.
 * Each letter is 5 rows tall with clear distinct shapes.
 * Every line is exactly 62 characters wide to match printDivider().
 */
void printBanner() {
    printf("\n");
    printThickDivider();
    printf("  |                                                          |\n");
    printf("  |  ####   #####   ###   ####   #      ###    #   #  #####  |\n");
    printf("  |  #   #  #      #   #  #   #  #       #     ##  #  #      |\n");
    printf("  |  #   #  ###    #####  #   #  #       #     # # #  ###    |\n");
    printf("  |  #   #  #      #   #  #   #  #       #     #  ##  #      |\n");
    printf("  |  ####   #####  #   #  ####   #####  ###    #   #  #####  |\n");
    printf("  |                                                          |\n");
    printf("  |   ####  #   #   ###   ####   ####                        |\n");
    printf("  |  #      #   #  #   #  #   #  #   #                       |\n");
    printf("  |  # ###  #   #  #####  ####   #   #                       |\n");
    printf("  |  #   #  #   #  #   #  # #    #   #                       |\n");
    printf("  |   ####   ###   #   #  #  ##  ####                        |\n");
    printf("  |                                                          |\n");
    printf("  |    Student Schedule & Deadline Tracker System            |\n");
    printf("  |                                                          |\n");
    printThickDivider();
    printf("  |  CC104: Data Structures & Algorithms  |  Final Project  |\n");
    printf("  |  DSA: DLL | Stack | Queue | Circ-DLL | PQ | BST | Tree |\n");
    printf("  |       Bubble Sort | Insertion Sort | Binary Search      |\n");
    printf("  |       Linear Search | Infix-to-Postfix Conversion       |\n");
    printThickDivider();
}

/*
 * compareDates: compares two date strings in "YYYY-MM-DD" format.
 * Returns negative if a < b, 0 if equal, positive if a > b.
 */
int compareDates(const char *a, const char *b) {
    return strcmp(a, b);
}

/*
 * getTodayDate: fills the buffer with today's date as "YYYY-MM-DD".
 */
void getTodayDate(char *buffer) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buffer, MAX_DATE_LEN, "%Y-%m-%d", tm_info);
}

/*
 * daysUntilDue: counts how many days from today until the given dueDate.
 * Returns a negative number if the date has already passed.
 */
int daysUntilDue(const char *dueDate) {
    char today[MAX_DATE_LEN];
    getTodayDate(today);

    int ty, tm2, td;
    sscanf(today,   "%d-%d-%d", &ty, &tm2, &td);

    int dy, dm, dd;
    sscanf(dueDate, "%d-%d-%d", &dy, &dm,  &dd);

    long todayDays = ty * 365L + tm2 * 30 + td;
    long dueDays   = dy * 365L + dm  * 30 + dd;

    return (int)(dueDays - todayDays);
}

/*
 * computePriority: determines the priority of a task based on days left.
 */
int computePriority(const char *dueDate) {
    int days = daysUntilDue(dueDate);
    if (days <= URGENT_DAYS) return 1;
    if (days <= 7)           return 2;
    return 3;
}

/*
 * truncate: copies src into dst, capping at maxLen chars (adds '\0').
 * Used so long field values never break column alignment.
 */
void truncate(char *dst, const char *src, int maxLen) {
    int len = (int)strlen(src);
    if (len <= maxLen) {
        strncpy(dst, src, maxLen);
        dst[len] = '\0';
    } else {
        strncpy(dst, src, maxLen - 2);
        dst[maxLen - 2] = '.';
        dst[maxLen - 1] = '.';
        dst[maxLen]     = '\0';
    }
}

/*
 * printTask: thin wrapper — calls printTaskRow for backward compatibility.
 * All callers that used printTask() now get the wide table row.
 */
void printTask(Task *t) {
    printTaskRow(t);
}

/*
 * isValidDate: checks if a date string is in "YYYY-MM-DD" format.
 * Returns 1 if valid, 0 if not.
 */
int isValidDate(const char *date) {
    if (strlen(date) != 10) return 0;
    if (date[4] != '-' || date[7] != '-') return 0;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (date[i] < '0' || date[i] > '9') return 0;
    }
    int y, m, d;
    sscanf(date, "%d-%d-%d", &y, &m, &d);
    if (m < 1 || m > 12) return 0;
    if (d < 1 || d > 31) return 0;
    return 1;
}

/*
 * readIntInput: safely reads an integer from stdin.
 * Returns 1 on success, 0 on invalid input.
 * Sets *value to the parsed integer.
 */
int readIntInput(int *value) {
    char buf[20];
    if (fgets(buf, sizeof(buf), stdin) == NULL) return 0;
    buf[strcspn(buf, "\n")] = '\0';
    /* Check that every character is a digit (allow leading minus) */
    int start = 0;
    if (buf[0] == '-') start = 1;
    if (buf[start] == '\0') return 0;
    for (int i = start; buf[i] != '\0'; i++) {
        if (buf[i] < '0' || buf[i] > '9') return 0;
    }
    *value = atoi(buf);
    return 1;
}

/* ============================================================
 *  1. DOUBLY LINKED LIST – Main Task List
 * ============================================================ */

/*
 * createDLLNode: allocates and initializes a new DLL node.
 */
DLLNode *createDLLNode(Task t) {
    DLLNode *node = (DLLNode *)malloc(sizeof(DLLNode));
    if (node == NULL) {
        printf("ERROR: Memory allocation failed.\n");
        exit(1);
    }
    node->data = t;
    node->prev = NULL;
    node->next = NULL;
    return node;
}

/*
 * dllInsertEnd: inserts a new task at the end of the doubly linked list.
 */
void dllInsertEnd(DLLNode **head, Task t) {
    DLLNode *newNode = createDLLNode(t);
    if (*head == NULL) {
        *head = newNode;
        return;
    }
    DLLNode *current = *head;
    while (current->next != NULL)
        current = current->next;
    current->next = newNode;
    newNode->prev = current;
}

/*
 * dllDeleteByID: removes the node with the matching task ID.
 * Returns 1 if found and deleted, 0 if not found.
 */
int dllDeleteByID(DLLNode **head, int id, Task *deletedTask) {
    DLLNode *current = *head;
    while (current != NULL) {
        if (current->data.id == id) {
            *deletedTask = current->data;

            if (current->prev != NULL)
                current->prev->next = current->next;
            else
                *head = current->next;

            if (current->next != NULL)
                current->next->prev = current->prev;

            free(current);
            return 1;
        }
        current = current->next;
    }
    return 0;
}

/*
 * dllFindByID: returns a pointer to the node with the given ID, or NULL.
 */
DLLNode *dllFindByID(DLLNode *head, int id) {
    DLLNode *current = head;
    while (current != NULL) {
        if (current->data.id == id)
            return current;
        current = current->next;
    }
    return NULL;
}

/*
 * dllCount: counts how many nodes are in the list.
 */
int dllCount(DLLNode *head) {
    int count = 0;
    DLLNode *current = head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

/* ============================================================
 *  2. STACK – Undo Last Action
 * ============================================================ */

/*
 * stackPush: pushes a snapshot of an action onto the undo stack.
 */
void stackPush(StackNode **top, const char *actionType, Task snapshot) {
    StackNode *newNode = (StackNode *)malloc(sizeof(StackNode));
    if (newNode == NULL) {
        printf("ERROR: Memory allocation failed.\n");
        exit(1);
    }
    strncpy(newNode->actionType, actionType, sizeof(newNode->actionType) - 1);
    newNode->snapshot = snapshot;
    newNode->next     = *top;
    *top = newNode;
}

/*
 * stackPop: pops the top action off the undo stack.
 * Returns 1 if successful, 0 if stack is empty.
 */
int stackPop(StackNode **top, char *actionType, Task *snapshot) {
    if (*top == NULL) return 0;

    StackNode *temp = *top;
    strncpy(actionType, temp->actionType, 10);
    *snapshot = temp->snapshot;
    *top = temp->next;
    free(temp);
    return 1;
}

/*
 * stackIsEmpty: returns 1 if the undo stack is empty, 0 otherwise.
 */
int stackIsEmpty(StackNode *top) {
    return top == NULL;
}

/* ============================================================
 *  3. QUEUE – Pending Submissions
 * ============================================================ */

/*
 * initQueue: initializes the queue to empty state.
 */
void initQueue(Queue *q) {
    q->front = NULL;
    q->rear  = NULL;
    q->size  = 0;
}

/*
 * enqueue: adds a task to the rear of the pending submissions queue.
 */
void enqueue(Queue *q, Task t) {
    QueueNode *newNode = (QueueNode *)malloc(sizeof(QueueNode));
    if (newNode == NULL) {
        printf("ERROR: Memory allocation failed.\n");
        exit(1);
    }
    newNode->data = t;
    newNode->next = NULL;

    if (q->rear == NULL) {
        q->front = newNode;
        q->rear  = newNode;
    } else {
        q->rear->next = newNode;
        q->rear       = newNode;
    }
    q->size++;
}

/*
 * dequeue: removes and returns the task at the front of the queue.
 * Returns 1 if successful, 0 if queue is empty.
 */
int dequeue(Queue *q, Task *t) {
    if (q->front == NULL) return 0;

    QueueNode *temp = q->front;
    *t = temp->data;
    q->front = temp->next;

    if (q->front == NULL)
        q->rear = NULL;

    free(temp);
    q->size--;
    return 1;
}

/*
 * queueIsEmpty: returns 1 if the queue is empty.
 */
int queueIsEmpty(Queue *q) {
    return q->front == NULL;
}

/*
 * queueRebuild: clears and rebuilds the pending queue from the DLL.
 * Only tasks that are still pending (isDone == 0) and still exist
 * in the DLL are added. This fixes the undo sync bug.
 */
void queueRebuild() {
    /* Free all existing queue nodes */
    while (!queueIsEmpty(&pendingQ)) {
        Task dummy;
        dequeue(&pendingQ, &dummy);
    }
    /* Re-add only tasks currently in the DLL that are still pending */
    DLLNode *current = taskHead;
    while (current != NULL) {
        if (!current->data.isDone)
            enqueue(&pendingQ, current->data);
        current = current->next;
    }
}

/* ============================================================
 *  4. CIRCULAR DOUBLY LINKED LIST – Weekly Schedule
 *     Now supports multiple subjects per day
 * ============================================================ */

/*
 * buildWeeklySchedule: creates 7 nodes for Mon-Sun and links them
 * circularly — Sunday's next points back to Monday.
 */
void buildWeeklySchedule() {
    const char *days[] = {
        "Monday","Tuesday","Wednesday","Thursday",
        "Friday","Saturday","Sunday"
    };

    SchedNode *first = NULL, *prev = NULL;

    for (int i = 0; i < MAX_DAYS; i++) {
        SchedNode *node = (SchedNode *)malloc(sizeof(SchedNode));
        if (node == NULL) { printf("ERROR: Memory allocation failed.\n"); exit(1); }

        strncpy(node->day, days[i], sizeof(node->day) - 1);
        node->entryCount = 0;   /* start with zero subjects for this day */
        node->prev = NULL;
        node->next = NULL;

        if (first == NULL) {
            first = node;
        } else {
            prev->next = node;
            node->prev = prev;
        }
        prev = node;
    }

    /* Make it circular: last node links back to first */
    prev->next  = first;
    first->prev = prev;
    schedHead   = first;
}

/*
 * addScheduleToDay: finds the node for the given day and APPENDS
 * a new subject/time entry (instead of overwriting).
 * Returns 1 on success, 0 if day not found or day is full.
 */
int addScheduleToDay(const char *dayName, const char *subject, const char *timeSlot) {
    SchedNode *current = schedHead;
    for (int i = 0; i < MAX_DAYS; i++) {
        if (strcmp(current->day, dayName) == 0) {
            if (current->entryCount >= MAX_SCHED_PER_DAY) {
                printf("  This day already has the maximum of %d subjects.\n", MAX_SCHED_PER_DAY);
                return 0;
            }
            int idx = current->entryCount;
            strncpy(current->entries[idx].subject, subject,  MAX_SUBJECT_LEN - 1);
            strncpy(current->entries[idx].time,    timeSlot, 29);
            current->entryCount++;
            return 1;
        }
        current = current->next;
    }
    return 0;   /* day not found */
}

/*
 * removeScheduleFromDay: removes a specific entry from a day by index.
 */
int removeScheduleEntry(const char *dayName, int entryIndex) {
    SchedNode *current = schedHead;
    for (int i = 0; i < MAX_DAYS; i++) {
        if (strcmp(current->day, dayName) == 0) {
            if (entryIndex < 1 || entryIndex > current->entryCount) {
                printf("  Invalid entry number.\n");
                return 0;
            }
            int idx = entryIndex - 1;
            /* Shift remaining entries left */
            for (int j = idx; j < current->entryCount - 1; j++)
                current->entries[j] = current->entries[j + 1];
            current->entryCount--;
            return 1;
        }
        current = current->next;
    }
    return 0;
}

/*
 * displayWeeklySchedule: prints all 7 days with all their subjects.
 */
void displayWeeklySchedule() {
    printHeader("Weekly Class Schedule  [ Circular Doubly Linked List ]");

    /* Columns: Day(9) | #(1) | Subject(44) | Time(46) — total row = 115 chars */
    #define SCHED_RULE \
        "  +-----------+---+----------------------------------------------+------------------------------------------------+\n"

    printf(SCHED_RULE);
    printf("  | %-9s | # | %-44s | %-46s |\n", "Day", "Subject", "Time Slot");
    printf(SCHED_RULE);

    SchedNode *current = schedHead;
    for (int i = 0; i < MAX_DAYS; i++) {
        if (current->entryCount == 0) {
            printf("  | %-9s |   | %-44s | %-46s |\n",
                   current->day, "-- No class scheduled --", "");
        } else {
            for (int j = 0; j < current->entryCount; j++) {
                char subjBuf[45], timeBuf[47];
                truncate(subjBuf, current->entries[j].subject, 44);
                truncate(timeBuf, current->entries[j].time,    46);
                printf("  | %-9s | %d | %-44s | %-46s |\n",
                       j == 0 ? current->day : "",
                       j + 1,
                       subjBuf,
                       timeBuf);
            }
        }
        printf(SCHED_RULE);
        current = current->next;
    }
}

/* ============================================================
 *  5. PRIORITY QUEUE – Deadline Urgency Alerts
 *     Implemented as a sorted linked list (nearest due date first)
 * ============================================================ */

/*
 * pqEnqueue: inserts a task into the priority queue in sorted order.
 * Lower priority number = higher urgency = goes to the front.
 */
void pqEnqueue(PQNode **head, Task t) {
    PQNode *newNode = (PQNode *)malloc(sizeof(PQNode));
    if (newNode == NULL) { printf("ERROR: Memory allocation failed.\n"); exit(1); }
    newNode->data = t;
    newNode->next = NULL;

    if (*head == NULL ||
        compareDates(t.dueDate, (*head)->data.dueDate) <= 0) {
        newNode->next = *head;
        *head = newNode;
        return;
    }

    PQNode *current = *head;
    while (current->next != NULL &&
           compareDates(current->next->data.dueDate, t.dueDate) <= 0) {
        current = current->next;
    }
    newNode->next = current->next;
    current->next = newNode;
}

/*
 * pqRebuild: clears and rebuilds the priority queue from the task DLL.
 */
void pqRebuild() {
    while (pqHead != NULL) {
        PQNode *temp = pqHead;
        pqHead = pqHead->next;
        free(temp);
    }
    DLLNode *current = taskHead;
    while (current != NULL) {
        if (!current->data.isDone)
            pqEnqueue(&pqHead, current->data);
        current = current->next;
    }
}

/*
 * displayPriorityAlerts: shows tasks due within URGENT_DAYS days.
 */
void displayPriorityAlerts() {
    printHeader("Priority Alerts  [ Tasks Due Within 3 Days ]");
    PQNode *current = pqHead;
    int found = 0;

    /* Count first so we can print header only when needed */
    PQNode *scan = pqHead;
    while (scan != NULL) {
        if (daysUntilDue(scan->data.dueDate) <= URGENT_DAYS) { found = 1; break; }
        scan = scan->next;
    }

    if (!found) {
        printf("  |                                                                                                            |\n");
        printf("  |   [+] No urgent deadlines right now. Keep it up!                                                          |\n");
        printf("  |                                                                                                            |\n");
        printf(TBL_RULE);
        return;
    }

    /* Alert table — same total width as the main task table (115 chars) */
    /* Columns: Urgcy(5) | Title(56) | Subject(29) | Due Date(10) */
    printf("  +-------+----------------------------------------------------------+-------------------------------+------------+\n");
    printf("  | %-5s | %-56s | %-29s | %-10s |\n",
           "Urgcy", "Task Title", "Subject", "Due Date");
    printf("  +-------+----------------------------------------------------------+-------------------------------+------------+\n");

    while (current != NULL) {
        int days = daysUntilDue(current->data.dueDate);
        if (days <= URGENT_DAYS) {
            char urgBuf[20], titleBuf[57], subjBuf[30];
            truncate(titleBuf, current->data.title,   56);
            truncate(subjBuf,  current->data.subject, 29);

            if (days < 0)
                snprintf(urgBuf, sizeof(urgBuf), "OVR%d", -days);
            else if (days == 0)
                snprintf(urgBuf, sizeof(urgBuf), "TODAY!");
            else
                snprintf(urgBuf, sizeof(urgBuf), "%2dday%s", days, days == 1 ? " " : "s");

            printf("  | %-5s | %-56s | %-29s | %-10s |\n",
                   urgBuf, titleBuf, subjBuf, current->data.dueDate);
        }
        current = current->next;
    }
    printf("  +-------+----------------------------------------------------------+-------------------------------+------------+\n");
    printf(TBL_RULE);
}

/* ============================================================
 *  6. BINARY SEARCH TREE – Search Tasks by Subject
 * ============================================================ */

/*
 * bstInsert: inserts a task into the BST, keyed by subject name.
 */
BSTNode *bstInsert(BSTNode *root, Task t) {
    if (root == NULL) {
        BSTNode *newNode = (BSTNode *)malloc(sizeof(BSTNode));
        if (newNode == NULL) { printf("ERROR: Memory allocation failed.\n"); exit(1); }
        newNode->data  = t;
        newNode->left  = NULL;
        newNode->right = NULL;
        return newNode;
    }

    int cmp = strcmp(t.subject, root->data.subject);
    if (cmp < 0)
        root->left  = bstInsert(root->left,  t);
    else
        root->right = bstInsert(root->right, t);

    return root;
}

/*
 * bstSearchBySubject: prints all tasks whose subject matches the keyword.
 * Uses in-order traversal (alphabetical order).
 */
void bstSearchBySubject(BSTNode *root, const char *keyword) {
    if (root == NULL) return;

    bstSearchBySubject(root->left, keyword);

    char subjectLower[MAX_SUBJECT_LEN];
    char keyLower[MAX_SUBJECT_LEN];
    strncpy(subjectLower, root->data.subject, MAX_SUBJECT_LEN - 1);
    strncpy(keyLower, keyword, MAX_SUBJECT_LEN - 1);
    subjectLower[MAX_SUBJECT_LEN - 1] = '\0';
    keyLower[MAX_SUBJECT_LEN - 1]     = '\0';

    for (int i = 0; subjectLower[i]; i++)
        if (subjectLower[i] >= 'A' && subjectLower[i] <= 'Z')
            subjectLower[i] += 32;
    for (int i = 0; keyLower[i]; i++)
        if (keyLower[i] >= 'A' && keyLower[i] <= 'Z')
            keyLower[i] += 32;

    if (strstr(subjectLower, keyLower) != NULL)
        printTask(&root->data);

    bstSearchBySubject(root->right, keyword);
}

/*
 * bstFreeAll / bstRebuild: clear and rebuild BST from the DLL.
 */
void bstFreeAll(BSTNode *root) {
    if (root == NULL) return;
    bstFreeAll(root->left);
    bstFreeAll(root->right);
    free(root);
}

void bstRebuild() {
    bstFreeAll(bstRoot);
    bstRoot = NULL;
    DLLNode *current = taskHead;
    while (current != NULL) {
        bstRoot = bstInsert(bstRoot, current->data);
        current = current->next;
    }
}

/* ============================================================
 *  7. GENERAL TREE – Subject → Topic → Subtopic
 * ============================================================ */

/*
 * createTreeNode: allocates a new tree node with the given name.
 */
TreeNode *createTreeNode(const char *name) {
    TreeNode *node = (TreeNode *)malloc(sizeof(TreeNode));
    if (node == NULL) { printf("ERROR: Memory allocation failed.\n"); exit(1); }
    strncpy(node->name, name, MAX_TITLE_LEN - 1);
    node->name[MAX_TITLE_LEN - 1] = '\0';
    node->firstChild  = NULL;
    node->nextSibling = NULL;
    return node;
}

/*
 * treeAddChild: adds a child node to a parent using the
 * first-child / next-sibling representation.
 */
void treeAddChild(TreeNode *parent, TreeNode *child) {
    if (parent->firstChild == NULL) {
        parent->firstChild = child;
    } else {
        TreeNode *current = parent->firstChild;
        while (current->nextSibling != NULL)
            current = current->nextSibling;
        current->nextSibling = child;
    }
}

/*
 * treeFind: searches for a node with the given name (DFS).
 */
TreeNode *treeFind(TreeNode *root, const char *name) {
    if (root == NULL) return NULL;
    if (strcmp(root->name, name) == 0) return root;

    TreeNode *found = treeFind(root->firstChild, name);
    if (found != NULL) return found;

    return treeFind(root->nextSibling, name);
}

/*
 * displayTree: recursively prints the subject hierarchy with indentation.
 */
void displayTree(TreeNode *node, int level) {
    if (node == NULL) return;

    for (int i = 0; i < level; i++) printf("   ");

    if (level == 0)      printf("[SEMESTER]\n");
    else if (level == 1) printf("|-- [Subject] %s\n",  node->name);
    else if (level == 2) printf("|---- [Topic] %s\n",  node->name);
    else                 printf("|------ [Subtopic] %s\n", node->name);

    displayTree(node->firstChild,  level + 1);
    displayTree(node->nextSibling, level);
}

/*
 * initSubjectTree: creates the root node for the semester.
 */
void initSubjectTree() {
    subjectRoot = createTreeNode("My Semester");
}

/* ============================================================
 *  8. SORTING ALGORITHMS
 * ============================================================ */

/*
 * copyDLLToArray: copies task data from the DLL into a Task array.
 */
int copyDLLToArray(DLLNode *head, Task arr[], int maxSize) {
    int i = 0;
    DLLNode *current = head;
    while (current != NULL && i < maxSize) {
        arr[i++] = current->data;
        current  = current->next;
    }
    return i;
}

/*
 * bubbleSortByDate: sorts Task array by due date using Bubble Sort.
 * Time complexity: O(n^2)
 */
void bubbleSortByDate(Task arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (compareDates(arr[j].dueDate, arr[j+1].dueDate) > 0) {
                Task temp = arr[j];
                arr[j]    = arr[j+1];
                arr[j+1]  = temp;
            }
        }
    }
}

/*
 * insertionSortBySubject: sorts Task array by subject name using Insertion Sort.
 * Time complexity: O(n^2) worst, O(n) best
 */
void insertionSortBySubject(Task arr[], int n) {
    for (int i = 1; i < n; i++) {
        Task key = arr[i];
        int  j   = i - 1;
        while (j >= 0 && strcmp(arr[j].subject, key.subject) > 0) {
            arr[j+1] = arr[j];
            j--;
        }
        arr[j+1] = key;
    }
}

/* ============================================================
 *  9. SEARCH ALGORITHMS
 * ============================================================ */

/*
 * linearSearchByTitle: scans every task in the DLL for a keyword.
 * Time complexity: O(n)
 */
void linearSearchByTitle(DLLNode *head, const char *keyword) {
    DLLNode *current = head;
    int found = 0;

    while (current != NULL) {
        char titleLower[MAX_TITLE_LEN];
        char keyLower[MAX_TITLE_LEN];
        strncpy(titleLower, current->data.title, MAX_TITLE_LEN - 1);
        strncpy(keyLower,   keyword,             MAX_TITLE_LEN - 1);
        titleLower[MAX_TITLE_LEN - 1] = '\0';
        keyLower[MAX_TITLE_LEN - 1]   = '\0';

        for (int i = 0; titleLower[i]; i++)
            if (titleLower[i] >= 'A' && titleLower[i] <= 'Z') titleLower[i] += 32;
        for (int i = 0; keyLower[i]; i++)
            if (keyLower[i] >= 'A' && keyLower[i] <= 'Z') keyLower[i] += 32;

        if (strstr(titleLower, keyLower) != NULL) {
            printTask(&current->data);
            found = 1;
        }
        current = current->next;
    }
    if (!found)
        printf("  No tasks found with keyword: %s\n", keyword);
}

/*
 * binarySearchByDate: binary search on a SORTED Task array by due date.
 * Returns the first index found, or -1 if not found.
 * Time complexity: O(log n)
 */
int binarySearchByDate(Task arr[], int n, const char *targetDate) {
    int low = 0, high = n - 1;

    while (low <= high) {
        int mid = (low + high) / 2;
        int cmp = compareDates(arr[mid].dueDate, targetDate);

        if (cmp == 0)     return mid;
        else if (cmp < 0) low  = mid + 1;
        else              high = mid - 1;
    }
    return -1;
}

/* ============================================================
 *  10. INFIX TO POSTFIX + EVALUATION – Grade Calculator
 * ============================================================ */

/*
 * precedence: returns the operator precedence level.
 */
int precedence(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    return 0;
}

/*
 * isOperator: returns 1 if the character is a math operator.
 */
int isOperator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/';
}

/*
 * infixToPostfix: converts an infix expression string to postfix.
 * Uses a character stack.
 *
 * Algorithm:
 *   1. If operand (digit or dot), copy directly to output
 *   2. If '(', push to stack
 *   3. If ')', pop until '(' is found
 *   4. If operator, pop higher/equal precedence ops, then push current
 *   5. At end, pop remaining operators to output
 */
void infixToPostfix(const char *infix, char *postfix) {
    char stack[MAX_EXPR_LEN];
    int  top    = -1;
    int  outIdx = 0;

    for (int i = 0; infix[i] != '\0'; i++) {
        char c = infix[i];

        if (c == ' ') continue;

        if ((c >= '0' && c <= '9') || c == '.') {
            postfix[outIdx++] = c;
            while (infix[i+1] != '\0' &&
                   ((infix[i+1] >= '0' && infix[i+1] <= '9') || infix[i+1] == '.')) {
                i++;
                postfix[outIdx++] = infix[i];
            }
            postfix[outIdx++] = ' ';
        }
        else if (c == '(') {
            stack[++top] = c;
        }
        else if (c == ')') {
            while (top >= 0 && stack[top] != '(') {
                postfix[outIdx++] = stack[top--];
                postfix[outIdx++] = ' ';
            }
            if (top >= 0) top--;
        }
        else if (isOperator(c)) {
            while (top >= 0 && isOperator(stack[top]) &&
                   precedence(stack[top]) >= precedence(c)) {
                postfix[outIdx++] = stack[top--];
                postfix[outIdx++] = ' ';
            }
            stack[++top] = c;
        }
    }

    while (top >= 0) {
        postfix[outIdx++] = stack[top--];
        postfix[outIdx++] = ' ';
    }
    postfix[outIdx] = '\0';
}

/*
 * evaluatePostfix: evaluates a postfix expression string.
 * Uses a float stack. Returns the computed result.
 */
float evaluatePostfix(const char *postfix) {
    float stack[MAX_EXPR_LEN];
    int   top = -1;
    int   i   = 0;

    while (postfix[i] != '\0') {
        if (postfix[i] == ' ') { i++; continue; }

        if ((postfix[i] >= '0' && postfix[i] <= '9') || postfix[i] == '.') {
            char numBuf[20];
            int  j = 0;
            while (postfix[i] != ' ' && postfix[i] != '\0')
                numBuf[j++] = postfix[i++];
            numBuf[j] = '\0';
            stack[++top] = (float)atof(numBuf);
        }
        else if (isOperator(postfix[i])) {
            if (top < 1) { printf("ERROR: Invalid expression.\n"); return 0; }
            float b = stack[top--];
            float a = stack[top--];
            switch (postfix[i]) {
                case '+': stack[++top] = a + b; break;
                case '-': stack[++top] = a - b; break;
                case '*': stack[++top] = a * b; break;
                case '/':
                    if (b == 0) { printf("ERROR: Division by zero.\n"); return 0; }
                    stack[++top] = a / b;
                    break;
            }
            i++;
        }
        else { i++; }
    }
    return (top >= 0) ? stack[top] : 0;
}

/* ============================================================
 *  FEATURE FUNCTIONS (Menu Actions)
 * ============================================================ */

/*
 * addTask: prompts the user for task details and adds to the system.
 * User can type "cancel" in the title field to abort.
 */
void addTask() {
    Task t;
    t.isDone = 0;

    printHeader("Add New Task  [ Doubly Linked List + Queue + BST ]");
    printf("  |  TIP: Type 'cancel' in the Title field to go back.      |\n");
    printDivider();
    printf("\n");

    /* --- Title --- */
    printf("  Title        : ");
    fgets(t.title, MAX_TITLE_LEN, stdin);
    t.title[strcspn(t.title, "\n")] = '\0';
    if (strcmp(t.title, "cancel") == 0) {
        printf("\n  [-] Action cancelled. Returning to menu.\n");
        return;
    }

    /* --- Subject --- */
    printf("  Subject      : ");
    fgets(t.subject, MAX_SUBJECT_LEN, stdin);
    t.subject[strcspn(t.subject, "\n")] = '\0';

    /* --- Type --- */
    printf("\n");
    printDivider();
    printf("  |  Select Task Type:                                       |\n");
    printDivider();
    printf("  |   [1] Assignment    [2] Exam    [3] Project              |\n");
    printf("  |   [4] Quiz          [5] Activity                         |\n");
    printf("  |   [0] Cancel                                             |\n");
    printDivider();
    printf("  Choice       : ");

    int typeChoice;
    if (!readIntInput(&typeChoice)) {
        printf("\n  [-] Invalid input. Action cancelled.\n");
        return;
    }
    switch (typeChoice) {
        case 1: strcpy(t.type, "Assignment"); break;
        case 2: strcpy(t.type, "Exam");       break;
        case 3: strcpy(t.type, "Project");    break;
        case 4: strcpy(t.type, "Quiz");       break;
        case 5: strcpy(t.type, "Activity");   break;
        case 0:
            printf("\n  [-] Action cancelled. Returning to menu.\n");
            return;
        default:
            printf("\n  [-] Invalid type. Action cancelled.\n");
            return;
    }

    /* --- Due Date --- */
    printf("\n");
    int dateOk = 0;
    while (!dateOk) {
        printf("  Due Date     : (YYYY-MM-DD, e.g. 2025-05-30 or 'cancel') ");
        fgets(t.dueDate, MAX_DATE_LEN, stdin);
        t.dueDate[strcspn(t.dueDate, "\n")] = '\0';

        if (strcmp(t.dueDate, "cancel") == 0) {
            printf("\n  [-] Action cancelled. Returning to menu.\n");
            return;
        }
        if (!isValidDate(t.dueDate)) {
            printf("  [-] Invalid date. Please use YYYY-MM-DD format.\n");
        } else {
            dateOk = 1;
        }
    }

    /* Assign ID and compute priority */
    t.id       = nextID++;
    t.priority = computePriority(t.dueDate);

    /* Add to all data structures */
    dllInsertEnd(&taskHead, t);
    enqueue(&pendingQ, t);
    stackPush(&undoTop, "ADD", t);
    bstRebuild();
    pqRebuild();

    printf("\n");
    printDivider();
    printf("  |  [+] Task successfully added!                           |\n");
    printDivider();

    char tTitle[43], tSubj[43], tType[43], tDue[43], tPri[43];
    truncate(tTitle, t.title,              42);
    truncate(tSubj,  t.subject,            42);
    truncate(tType,  t.type,               42);
    truncate(tDue,   t.dueDate,            42);
    truncate(tPri,   getPriorityLabel(t.priority), 42);

    printf("  |  ID       : %-42d|\n", t.id);
    printf("  |  Title    : %-42s|\n", tTitle);
    printf("  |  Subject  : %-42s|\n", tSubj);
    printf("  |  Type     : %-42s|\n", tType);
    printf("  |  Due Date : %-42s|\n", tDue);
    printf("  |  Priority : %-42s|\n", tPri);
    printDivider();
}

/*
 * viewAllTasksSorted: copies tasks to array, sorts by date, then displays.
 */
void viewAllTasksSorted() {
    int count = dllCount(taskHead);
    if (count == 0) {
        printHeader("All Tasks");
        printf("  |  [!] No tasks found. Use option [1] to add a task.      |\n");
        printDivider();
        return;
    }

    Task arr[500];
    int  n = copyDLLToArray(taskHead, arr, 500);
    bubbleSortByDate(arr, n);

    /* Count stats */
    int pending = 0, done = 0;
    for (int i = 0; i < n; i++) {
        if (arr[i].isDone) done++;
        else               pending++;
    }

    printHeader("All Tasks  [ Sorted by Due Date  |  Bubble Sort ]");
    printTableHeader();

    for (int i = 0; i < n; i++) {
        printTaskRow(&arr[i]);
        if (i < n - 1) printTableSep();
    }

    printTaskSummary(n, pending, done);
    printf("\n");
    displayPriorityAlerts();
}

/*
 * viewTodaySchedule: shows today's day name and its schedule entries.
 */
void viewTodaySchedule() {
    char today[MAX_DATE_LEN];
    getTodayDate(today);

    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    const char *dayNames[] = {
        "Sunday","Monday","Tuesday","Wednesday",
        "Thursday","Friday","Saturday"
    };
    const char *todayName = dayNames[tm_info->tm_wday];

    printHeader("Today's Schedule  [ Circular Doubly Linked List ]");
    printf("  |  Today  : %-47s|\n", todayName);
    printf("  |  Date   : %-47s|\n", today);
    printDivider();

    SchedNode *current = schedHead;
    for (int i = 0; i < MAX_DAYS; i++) {
        if (strcmp(current->day, todayName) == 0) {
            if (current->entryCount == 0) {
                printf("  |  [!] No classes scheduled for today.                    |\n");
            } else {
                for (int j = 0; j < current->entryCount; j++) {
                    printf("  |  [%d] %-28s  %s\n",
                           j + 1,
                           current->entries[j].subject,
                           current->entries[j].time);
                }
            }
            printDivider();
            return;
        }
        current = current->next;
    }
}

/*
 * manageWeeklySchedule: sub-menu for viewing, adding, and removing
 * schedule entries. Now supports multiple subjects per day.
 */
void manageWeeklySchedule() {
    int running = 1;
    while (running) {
        printHeader("Weekly Schedule Manager  [ Circular Doubly Linked List ]");
        printf("  |   [1] View Full Weekly Schedule                          |\n");
        printf("  |   [2] Add Subject to a Day                               |\n");
        printf("  |   [3] Remove a Subject from a Day                        |\n");
        printf("  |   [0] Back to Main Menu                                  |\n");
        printDivider();
        printf("  Choice : ");

        int choice;
        if (!readIntInput(&choice)) {
            printf("  [-] Invalid input. Please enter a number.\n");
            continue;
        }

        if (choice == 0) {
            running = 0;
        }
        else if (choice == 1) {
            displayWeeklySchedule();
        }
        else if (choice == 2) {
            printHeader("Add Subject to a Day");
            printf("  |  Days: Monday  Tuesday  Wednesday  Thursday             |\n");
            printf("  |         Friday  Saturday  Sunday                        |\n");
            printDivider();
            printf("  Day name (or 'cancel') : ");

            char day[15];
            fgets(day, sizeof(day), stdin);
            day[strcspn(day, "\n")] = '\0';

            if (strcmp(day, "cancel") == 0) {
                printf("  [-] Action cancelled.\n");
                continue;
            }

            printf("  Subject name           : ");
            char subject[MAX_SUBJECT_LEN];
            fgets(subject, MAX_SUBJECT_LEN, stdin);
            subject[strcspn(subject, "\n")] = '\0';

            printf("\n");
            printDivider();
            printf("  |  Enter class time range:                                |\n");
            printDivider();

            char startTime[15], endTime[15], timeSlot[35];
            printf("  Start time (e.g. 7:30 AM) : ");
            fgets(startTime, sizeof(startTime), stdin);
            startTime[strcspn(startTime, "\n")] = '\0';

            printf("  End time   (e.g. 9:30 AM) : ");
            fgets(endTime, sizeof(endTime), stdin);
            endTime[strcspn(endTime, "\n")] = '\0';

            snprintf(timeSlot, sizeof(timeSlot), "%s - %s", startTime, endTime);

            if (addScheduleToDay(day, subject, timeSlot))
                printf("\n  [+] '%s' added to %s  (%s)\n", subject, day, timeSlot);
            else
                printf("\n  [-] Day '%s' not found. Check spelling.\n", day);
        }
        else if (choice == 3) {
            displayWeeklySchedule();
            printf("  Day name (or 'cancel') : ");

            char day[15];
            fgets(day, sizeof(day), stdin);
            day[strcspn(day, "\n")] = '\0';

            if (strcmp(day, "cancel") == 0) {
                printf("  [-] Action cancelled.\n");
                continue;
            }

            printf("  Entry number to remove : ");
            int entryNum;
            if (!readIntInput(&entryNum)) {
                printf("  [-] Invalid input.\n");
                continue;
            }

            if (removeScheduleEntry(day, entryNum))
                printf("  [+] Entry removed from %s.\n", day);
        }
        else {
            printf("  [-] Invalid choice. Please enter 0, 1, 2, or 3.\n");
        }
    }
}

/*
 * markTaskDone: shows the pending task list first, then asks for ID.
 * Includes a cancel option before asking for ID.
 */
void markTaskDone() {
    printHeader("Mark Task as Done  [ Stack + Queue ]");

    int pendingCount = 0;
    DLLNode *current = taskHead;

    /* Count pending first to decide whether to print header */
    DLLNode *scan = taskHead;
    while (scan != NULL) { if (!scan->data.isDone) pendingCount++; scan = scan->next; }

    if (pendingCount == 0) {
        printf("  |  [!] No pending tasks to mark as done.                  |\n");
        printDivider();
        return;
    }

    printf("  |  Pending Tasks:                                          |\n");
    printTableHeader();
    current = taskHead;
    int first = 1;
    while (current != NULL) {
        if (!current->data.isDone) {
            if (!first) printTableSep();
            printTaskRow(&current->data);
            first = 0;
        }
        current = current->next;
    }
    printTableBottom();

    printf("  Enter Task ID to mark done (0 to cancel) : ");

    int id;
    if (!readIntInput(&id)) {
        printf("  [-] Invalid input. Action cancelled.\n");
        return;
    }
    if (id == 0) {
        printf("  [-] Action cancelled.\n");
        return;
    }

    DLLNode *node = dllFindByID(taskHead, id);
    if (node == NULL) {
        printf("  [-] Task ID %d not found.\n", id);
        return;
    }
    if (node->data.isDone) {
        printf("  [!] Task '%s' is already marked as done.\n", node->data.title);
        return;
    }

    stackPush(&undoTop, "DONE", node->data);
    node->data.isDone   = 1;
    node->data.priority = 0;

    pqRebuild();
    queueRebuild();
    printf("\n  [+] Task '%s' marked as done! (Undo available via option [5])\n",
           node->data.title);
}

/*
 * undoLastAction: peeks at the top of the stack to show the user
 * what will be undone, asks for confirmation, then reverses it.
 */
void undoLastAction() {
    if (stackIsEmpty(undoTop)) {
        printHeader("Undo Last Action  [ Stack ]");
        printf("  |  [!] Nothing to undo. Stack is empty.                   |\n");
        printDivider();
        return;
    }

    StackNode *top = undoTop;

    printHeader("Undo Last Action  [ Stack ]");
    printf("  |  Last recorded action:                                   |\n");
    printDivider();
    if (strcmp(top->actionType, "ADD") == 0)
        printf("  |  [+] Added task     : %-35s|\n", top->snapshot.title);
    else if (strcmp(top->actionType, "DELETE") == 0)
        printf("  |  [-] Deleted task   : %-35s|\n", top->snapshot.title);
    else if (strcmp(top->actionType, "DONE") == 0)
        printf("  |  [*] Marked done    : %-35s|\n", top->snapshot.title);
    printf("  |      ID: %-3d  Subject: %-20s  Due: %-10s|\n",
           top->snapshot.id, top->snapshot.subject, top->snapshot.dueDate);
    printDivider();
    printf("  Undo this action? (1 = Yes / 0 = No) : ");

    int confirm;
    if (!readIntInput(&confirm) || confirm != 1) {
        printf("  [-] Undo cancelled.\n");
        return;
    }

    char actionType[10];
    Task snapshot;
    stackPop(&undoTop, actionType, &snapshot);

    if (strcmp(actionType, "ADD") == 0) {
        Task deleted;
        dllDeleteByID(&taskHead, snapshot.id, &deleted);
        printf("\n  [+] Undo successful: Task '%s' removed.\n", snapshot.title);
    }
    else if (strcmp(actionType, "DELETE") == 0) {
        dllInsertEnd(&taskHead, snapshot);
        printf("\n  [+] Undo successful: Task '%s' restored.\n", snapshot.title);
    }
    else if (strcmp(actionType, "DONE") == 0) {
        DLLNode *node = dllFindByID(taskHead, snapshot.id);
        if (node != NULL) {
            node->data.isDone   = 0;
            node->data.priority = computePriority(node->data.dueDate);
        }
        printf("\n  [+] Undo successful: Task '%s' is pending again.\n", snapshot.title);
    }

    bstRebuild();
    pqRebuild();
    queueRebuild();
}

/*
 * searchTask: lets the user search by title keyword, subject, or date.
 */
void searchTask() {
    int running = 1;
    while (running) {
        printHeader("Search Task");
        printf("  |   [1] By Title Keyword   -- Linear Search  O(n)         |\n");
        printf("  |   [2] By Subject Name    -- BST In-Order   O(log n)     |\n");
        printf("  |   [3] By Due Date        -- Binary Search  O(log n)     |\n");
        printf("  |   [0] Back to Main Menu                                  |\n");
        printDivider();
        printf("  Choice : ");

        int choice;
        if (!readIntInput(&choice)) {
            printf("  [-] Invalid input. Please enter a number.\n");
            continue;
        }

        if (choice == 0) {
            running = 0;
        }
        else if (choice == 1) {
            printf("  Keyword to search in title : ");
            char keyword[MAX_TITLE_LEN];
            fgets(keyword, MAX_TITLE_LEN, stdin);
            keyword[strcspn(keyword, "\n")] = '\0';
            printHeader("Search Results  [ Linear Search by Title ]");
            printTableHeader();
            linearSearchByTitle(taskHead, keyword);
            printTableBottom();
        }
        else if (choice == 2) {
            printf("  Subject keyword : ");
            char keyword[MAX_SUBJECT_LEN];
            fgets(keyword, MAX_SUBJECT_LEN, stdin);
            keyword[strcspn(keyword, "\n")] = '\0';
            printHeader("Search Results  [ BST In-Order Search by Subject ]");
            printTableHeader();
            bstSearchBySubject(bstRoot, keyword);
            printTableBottom();
        }
        else if (choice == 3) {
            printf("  Due date to search (YYYY-MM-DD) : ");
            char keyword[MAX_DATE_LEN];
            fgets(keyword, MAX_DATE_LEN, stdin);
            keyword[strcspn(keyword, "\n")] = '\0';

            Task arr[500];
            int  n = copyDLLToArray(taskHead, arr, 500);
            bubbleSortByDate(arr, n);

            printHeader("Search Results  [ Binary Search by Due Date ]");
            int idx = binarySearchByDate(arr, n, keyword);
            if (idx == -1) {
                printf("  [-] No task found with due date: %s\n", keyword);
            } else {
                printTableHeader();
                int start = idx;
                while (start > 0 && strcmp(arr[start-1].dueDate, keyword) == 0)
                    start--;
                int first = 1;
                for (int i = start; i < n && strcmp(arr[i].dueDate, keyword) == 0; i++) {
                    if (!first) printTableSep();
                    printTaskRow(&arr[i]);
                    first = 0;
                }
                printTableBottom();
            }
        }
        else {
            printf("  [-] Invalid choice. Please enter 0, 1, 2, or 3.\n");
        }
    }
}

/*
 * viewPendingQueue: displays all tasks currently in the pending queue.
 * The queue is synced with the DLL via queueRebuild(), so this is accurate.
 */
void viewPendingQueue() {
    printHeader("Pending Submissions Queue  [ FIFO Queue ]");

    if (queueIsEmpty(&pendingQ)) {
        printf("  |  [!] No pending submissions. All caught up!             |\n");
        printDivider();
        return;
    }

    printTableHeader();
    QueueNode *current = pendingQ.front;
    int pos = 1;
    int total = 0;
    while (current != NULL) {
        printTaskRow(&current->data);
        if (current->next != NULL) printTableSep();
        current = current->next;
        pos++;
        total++;
    }
    printTableBottom();
    printf("  |  Queue size: %-3d  (FIFO — first added is first submitted)"
           "                        |\n", pendingQ.size);
    printDivider();
}

/*
 * browseSubjectTree: menu for adding subjects/topics/subtopics and displaying.
 */
void browseSubjectTree() {
    int running = 1;
    while (running) {
        printHeader("Subject Hierarchy Browser  [ General Tree ]");
        printf("  |   [1] Add Subject                                        |\n");
        printf("  |   [2] Add Topic under a Subject                          |\n");
        printf("  |   [3] Add Subtopic under a Topic                         |\n");
        printf("  |   [4] Display Full Tree                                  |\n");
        printf("  |   [0] Back to Main Menu                                  |\n");
        printDivider();
        printf("  Choice : ");

        int choice;
        if (!readIntInput(&choice)) {
            printf("  [-] Invalid input. Please enter a number.\n");
            continue;
        }

        char name[MAX_TITLE_LEN], parentName[MAX_TITLE_LEN];

        if (choice == 0) {
            running = 0;
        }
        else if (choice == 1) {
            printf("  Subject name (or 'cancel') : ");
            fgets(name, MAX_TITLE_LEN, stdin);
            name[strcspn(name, "\n")] = '\0';
            if (strcmp(name, "cancel") == 0) { printf("  [-] Cancelled.\n"); continue; }
            treeAddChild(subjectRoot, createTreeNode(name));
            printf("  [+] Subject '%s' added.\n", name);
        }
        else if (choice == 2) {
            printf("  Subject name (parent)      : ");
            fgets(parentName, MAX_TITLE_LEN, stdin);
            parentName[strcspn(parentName, "\n")] = '\0';
            if (strcmp(parentName, "cancel") == 0) { printf("  [-] Cancelled.\n"); continue; }

            printf("  Topic name                 : ");
            fgets(name, MAX_TITLE_LEN, stdin);
            name[strcspn(name, "\n")] = '\0';

            TreeNode *parent = treeFind(subjectRoot, parentName);
            if (parent == NULL)
                printf("  [-] Subject '%s' not found. Add it first with [1].\n", parentName);
            else {
                treeAddChild(parent, createTreeNode(name));
                printf("  [+] Topic '%s' added under '%s'.\n", name, parentName);
            }
        }
        else if (choice == 3) {
            printf("  Topic name (parent)        : ");
            fgets(parentName, MAX_TITLE_LEN, stdin);
            parentName[strcspn(parentName, "\n")] = '\0';
            if (strcmp(parentName, "cancel") == 0) { printf("  [-] Cancelled.\n"); continue; }

            printf("  Subtopic name              : ");
            fgets(name, MAX_TITLE_LEN, stdin);
            name[strcspn(name, "\n")] = '\0';

            TreeNode *parent = treeFind(subjectRoot, parentName);
            if (parent == NULL)
                printf("  [-] Topic '%s' not found. Add it first with [2].\n", parentName);
            else {
                treeAddChild(parent, createTreeNode(name));
                printf("  [+] Subtopic '%s' added under '%s'.\n", name, parentName);
            }
        }
        else if (choice == 4) {
            printHeader("Subject Hierarchy Tree");
            printf("\n");
            displayTree(subjectRoot, 0);
            printf("\n");
            printDivider();
        }
        else {
            printf("  [-] Invalid choice. Please enter 0, 1, 2, 3, or 4.\n");
        }
    }
}

/*
 * gradeCalculator: takes a weighted grade formula as infix,
 * converts to postfix, then evaluates the result.
 *
 * IMPROVED: Shows a step-by-step guide before asking for input.
 */
void gradeCalculator() {
    printHeader("Grade Calculator  [ Infix to Postfix Conversion ]");
    printf("  |  HOW TO USE:                                             |\n");
    printf("  |  Enter a weighted grade formula using scores (0-100)     |\n");
    printf("  |  and weights (decimals that add up to 1.0).              |\n");
    printf("  |                                                          |\n");
    printf("  |  Operators: +  -  *  /  and parentheses ( )             |\n");
    printf("  |                                                          |\n");
    printf("  |  EXAMPLES:                                               |\n");
    printf("  |    0.3 * 85 + 0.4 * 90 + 0.3 * 78  = weighted grade    |\n");
    printf("  |    (70 + 80 + 90) / 3               = simple average    |\n");
    printf("  |    0.6 * 88 + 0.4 * 75              = 2-component       |\n");
    printf("  |                                                          |\n");
    printf("  |  NOTE: Weights must total 1.0  (e.g. 0.3+0.4+0.3=1.0)  |\n");
    printDivider();
    printf("  Formula : ");

    char infix[MAX_EXPR_LEN];
    fgets(infix, MAX_EXPR_LEN, stdin);
    infix[strcspn(infix, "\n")] = '\0';

    if (strlen(infix) == 0) {
        printf("  [-] No formula entered. Returning to menu.\n");
        return;
    }

    /* Convert infix to postfix, then evaluate */
    char postfix[MAX_EXPR_LEN * 2];
    infixToPostfix(infix, postfix);
    float result = evaluatePostfix(postfix);

    /* Determine grade equivalent using CBSUA grading scale */
    const char *equiv = "";
    if      (result >= 97) equiv = "1.00  -- Excellent!";
    else if (result >= 94) equiv = "1.25";
    else if (result >= 91) equiv = "1.50";
    else if (result >= 88) equiv = "1.75";
    else if (result >= 85) equiv = "2.00";
    else if (result >= 82) equiv = "2.25";
    else if (result >= 79) equiv = "2.50";
    else if (result >= 76) equiv = "2.75";
    else if (result >= 75) equiv = "3.00  -- Passing";
    else                   equiv = "5.00  -- Below Passing";

    /* Truncate for display so box borders never break */
    char infixBuf[47], postfixBuf[47], equivBuf[47];
    truncate(infixBuf,   infix,   46);
    truncate(postfixBuf, postfix, 46);
    truncate(equivBuf,   equiv,   46);

    printf("\n");
    printDivider();
    printf("  |  Infix    : %-44s|\n", infixBuf);
    printf("  |  Postfix  : %-44s|\n", postfixBuf);
    printDivider();
    printf("  |  Result   : %-6.2f                                        |\n", result);
    printf("  |  Grade    : %-44s|\n", equivBuf);
    printDivider();
}

/*
 * sortAndDisplayBySubject: sorts tasks by subject using Insertion Sort.
 */
void sortAndDisplayBySubject() {
    int count = dllCount(taskHead);
    if (count == 0) {
        printHeader("Tasks by Subject");
        printf("  |  [!] No tasks found. Use option [1] to add a task.      |\n");
        printDivider();
        return;
    }
    Task arr[500];
    int  n = copyDLLToArray(taskHead, arr, 500);
    insertionSortBySubject(arr, n);

    int pending = 0, done = 0;
    for (int i = 0; i < n; i++) {
        if (arr[i].isDone) done++;
        else               pending++;
    }

    printHeader("Tasks Sorted by Subject  [ Insertion Sort ]");
    printTableHeader();
    for (int i = 0; i < n; i++) {
        printTaskRow(&arr[i]);
        if (i < n - 1) printTableSep();
    }
    printTaskSummary(n, pending, done);
}

/*
 * deleteTask: shows the task list first, then asks for ID to delete.
 * Includes a cancel option.
 */
void deleteTask() {
    printHeader("Delete a Task  [ Doubly Linked List ]");

    int count = dllCount(taskHead);
    if (count == 0) {
        printf("  |  [!] No tasks to delete.                                |\n");
        printDivider();
        return;
    }

    printTableHeader();
    DLLNode *current = taskHead;
    int first = 1;
    while (current != NULL) {
        if (!first) printTableSep();
        printTaskRow(&current->data);
        first = 0;
        current = current->next;
    }
    printTableBottom();
    printf("  Enter Task ID to delete (0 to cancel) : ");

    int id;
    if (!readIntInput(&id)) {
        printf("  [-] Invalid input. Action cancelled.\n");
        return;
    }
    if (id == 0) {
        printf("  [-] Action cancelled.\n");
        return;
    }

    Task deleted;
    if (dllDeleteByID(&taskHead, id, &deleted)) {
        stackPush(&undoTop, "DELETE", deleted);
        bstRebuild();
        pqRebuild();
        queueRebuild();
        printf("\n  [+] Task '%s' deleted. (Undo available via option [5])\n",
               deleted.title);
    } else {
        printf("  [-] Task ID %d not found.\n", id);
    }
}

/* ============================================================
 *  MAIN MENU
 * ============================================================ */

void showMainMenu() {
    printBanner();
    printf("\n");
    printDivider();
    printf("  |  TASK MANAGEMENT                                         |\n");
    printDivider();
    printf("  |   [1]  Add New Task                                      |\n");
    printf("  |   [2]  View All Tasks       (sorted by due date)         |\n");
    printf("  |   [3]  Mark Task as Done                                 |\n");
    printf("  |   [4]  Delete a Task                                     |\n");
    printf("  |   [5]  Undo Last Action                                  |\n");
    printDivider();
    printf("  |  SCHEDULE                                                |\n");
    printDivider();
    printf("  |   [6]  Today's Schedule                                  |\n");
    printf("  |   [7]  Weekly Schedule Manager                           |\n");
    printDivider();
    printf("  |  SEARCH & SORT                                           |\n");
    printDivider();
    printf("  |   [8]  Search Task                                       |\n");
    printf("  |   [9]  View Tasks Sorted by Subject                      |\n");
    printDivider();
    printf("  |  DEADLINES & SUBMISSIONS                                 |\n");
    printDivider();
    printf("  |   [10] Pending Submissions Queue                         |\n");
    printf("  |   [11] Priority Alerts      (due within 3 days)         |\n");
    printDivider();
    printf("  |  ACADEMIC TOOLS                                          |\n");
    printDivider();
    printf("  |   [12] Subject Hierarchy Browser                         |\n");
    printf("  |   [13] Grade Calculator                                  |\n");
    printDivider();
    printf("  |   [0]  Exit                                              |\n");
    printDivider();
    printf("  Choice : ");
}

/* ============================================================
 *  MAIN FUNCTION
 * ============================================================ */

int main() {
    /* Initialize all data structures */
    initQueue(&pendingQ);
    buildWeeklySchedule();
    initSubjectTree();

    printBanner();
    printf("\n");
    printDivider();
    printf("  |  [*] Welcome to DeadlineGuard!                           |\n");
    printf("  |      Your academic deadlines, organized.                 |\n");
    printDivider();
    printf("\n");

    /* Show priority alerts on startup */
    displayPriorityAlerts();

    printf("\n");
    printDivider();
    printf("  |  Press ENTER to go to the Main Menu...                  |\n");
    printDivider();
    getchar();

    /* Main program loop */
    while (1) {
        showMainMenu();

        int choice;
        if (!readIntInput(&choice)) {
            printf("\n  [-] Invalid input. Please enter a number from the menu.\n");
            printf("  Press ENTER to continue...");
            getchar();
            continue;
        }

        switch (choice) {
            case 1:  addTask();               break;
            case 2:  viewAllTasksSorted();    break;
            case 3:  markTaskDone();          break;
            case 4:  deleteTask();            break;
            case 5:  undoLastAction();        break;
            case 6:  viewTodaySchedule();     break;
            case 7:  manageWeeklySchedule();  break;
            case 8:  searchTask();            break;
            case 9:  sortAndDisplayBySubject(); break;
            case 10: viewPendingQueue();      break;
            case 11: displayPriorityAlerts(); break;
            case 12: browseSubjectTree();     break;
            case 13: gradeCalculator();       break;
            case 0:
                printf("\n");
                printThickDivider();
                printf("  |  Goodbye! Keep up with your deadlines. Good luck!       |\n");
                printThickDivider();
                printf("\n");
                return 0;
            default:
                printf("\n  [-] Invalid choice '%d'. Please enter a number from the menu.\n", choice);
        }

        printf("\n");
        printDivider();
        printf("  |  Press ENTER to return to the Main Menu...               |\n");
        printDivider();
        getchar();
    }

    return 0;
}
