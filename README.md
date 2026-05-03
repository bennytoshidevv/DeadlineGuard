# DeadlineGuard 
### Student Schedule & Deadline Tracker System

**CC104: Data Structures and Algorithms — Final Project**
Central Bicol State University of Agriculture
College of Information Technology | 2nd Year | 2nd Semester, SY 2025–2026

---

## Overview

DeadlineGuard is a console-based academic planner written in C. It was built as a final project for CC104: Data Structures and Algorithms to demonstrate the practical application of various data structures and algorithms through a real-world system that students can actually use.

The system allows a student to manage all of their academic tasks in one place — from assignments and exams to quizzes, projects, and activities. It keeps track of due dates, automatically flags urgent deadlines, organizes a weekly class schedule, and even helps compute weighted grades. Everything runs in the terminal with a simple numbered menu, so no installation of any external tools or libraries is needed.

---

## Purpose

The main purpose of DeadlineGuard is to solve a problem that almost every college student faces — keeping track of multiple deadlines across different subjects at the same time. It is easy to forget a quiz due tomorrow or miss a project submission because you were focused on a different subject. DeadlineGuard addresses this by putting everything in one organized system that sorts tasks by urgency, alerts you about upcoming deadlines, and lets you manage your entire semester schedule in one place.

Beyond its practical use, the project also serves as a demonstration of how data structures and algorithms are not just theoretical concepts but tools that can be applied to build something genuinely useful. Each feature in DeadlineGuard is powered by a specific DSA concept, chosen because it was the most appropriate structure for that particular problem.

---

## Features

- **Add Tasks** — Add assignments, exams, projects, quizzes, and activities with a title, subject, type, and due date
- **View All Tasks** — See all tasks sorted from the nearest to the furthest due date
- **Mark Task as Done** — Mark a task as completed; pending tasks are shown first so you know which ID to enter
- **Delete a Task** — Remove a task from the system; shows done/pending status before asking for the ID
- **Undo Last Action** — Reverse the last add, delete, or mark-as-done action with a confirmation prompt
- **View Today's Schedule** — See what subjects are scheduled for today
- **Weekly Schedule** — View and manage your full week; supports multiple subjects per day with start and end times
- **Search Tasks** — Look up tasks by title keyword, subject name, or due date
- **Sort by Subject** — View all tasks grouped and sorted alphabetically by subject
- **Pending Submissions Queue** — See all tasks that are still waiting to be submitted
- **Priority Alerts** — Automatically highlights tasks due within the next 3 days
- **Subject Hierarchy Browser** — Organize your subjects, topics, and subtopics in a tree structure
- **Grade Calculator** — Enter a weighted grade formula and get your computed grade with its equivalent

---

## DSA Concepts Used

| Data Structure / Algorithm | How It Is Used |
|---|---|
| Doubly Linked List | Main task list — supports adding, deleting, and traversing tasks forward and backward |
| Stack | Undo system — saves a snapshot of every action so it can be reversed |
| Queue | Pending submissions — tasks enter the queue when added and are removed when done |
| Circular Doubly Linked List | Weekly schedule — the 7 days of the week loop back to Monday after Sunday |
| Priority Queue | Deadline alerts — tasks are sorted by due date so the most urgent appears first |
| Binary Search Tree | Subject search — tasks are indexed by subject name for fast lookup |
| General Tree | Subject hierarchy — organizes a semester into subjects, topics, and subtopics |
| Bubble Sort | Sorts tasks by due date when viewing all tasks |
| Insertion Sort | Sorts tasks alphabetically by subject name |
| Linear Search | Searches task titles by keyword |
| Binary Search | Searches a sorted array of tasks by exact due date |
| Infix to Postfix | Grade calculator — converts a typed formula into postfix notation and evaluates it |

---

## How to Compile

Make sure you have GCC installed, then run:

```bash
gcc deadlineguard.c -o deadlineguard
```

Or with the C99 standard flag:

```bash
gcc deadlineguard.c -o deadlineguard -std=c99
```

---

## How to Run

**On Linux or Mac:**
```bash
./deadlineguard
```

**On Windows:**
```bash
deadlineguard.exe
```

---

## Notes

- Due dates must be entered in `YYYY-MM-DD` format, for example `2025-05-30`
- Class schedule times are entered as a range, for example start `7:30 AM` and end `9:30 AM`
- Priority is automatically computed based on how many days are left until the due date
  - **Urgent** — 3 days or less
  - **Upcoming** — 4 to 7 days
  - **Safe** — more than 7 days
- The undo feature works for adding a task, deleting a task, and marking a task as done
- In the grade calculator, weights should add up to 1.0, for example `0.3 + 0.4 + 0.3 = 1.0`
- Type `cancel` or enter `0` at any prompt to go back without making changes