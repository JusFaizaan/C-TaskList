# Simple Task Tracker

A dependency free CLI task manager written in **C++17** (single `main.cpp`).  
Tasks are stored in a **`tasks.tsv`** file in the current directory.


## Features

- Add tasks with **priority** (`H/M/L`) and optional **due date** (`YYYY-MM-DD`)
- List tasks (pending by default) and **sort** by `due` / `priority` / `id`
- Mark tasks **done**, **remove** by id, or **clear** completed/all
- Plain-text storage: `tasks.tsv`
- Single file, easy to build on Linux/macOS/Windows


## Requirements

- A C++17 compiler with `<filesystem>` support  
- GCC 9+ (GCC 8 may require an extra link flag), Clang 7+, or MSVC 2019+


# Commands

## Add a few tasks
- ./tt add "Write README" -p H -d 2025-08-31
- ./tt add "Fix bug #42" -p M
- ./tt add "Buy milk" -p L

## See pending tasks (default sort: due date)
- ./tt list

## Mark one done
- ./tt done 2

## Show everything, sorted by priority
- ./tt list --all --sort=priority

## Remove a task
- ./tt rm 3

## Clear completed tasks (or use --all to remove everything)
- ./tt clear --done
