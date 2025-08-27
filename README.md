# Versioned Document Store - Commands List

This document lists **all commands** supported by the Versioned Document Store CLI.

## 📄 Document Editing
| Command                 | Description                                      |
|-------------------------|--------------------------------------------------|
| `create <text>`         | Initialize a new document with given text        |
| `append <text>`         | Append text to the current working document      |
| `remove <n>`            | Remove the last `n` characters from the document |
| `undo`                  | Revert to last uncommitted state                 |

## 💾 Versioning
| Command                 | Description                                      |
|-------------------------|--------------------------------------------------|
| `commit <message>`      | Save the current content as a new version        |
| `log`                   | Show the version history (latest to oldest)      |
| `rollback <id>`         | Revert working content to a specific version     |
| `show [id]`             | Show the content of a version (latest by default)|
| `diff <v1> <v2>`        | Show differences between two versions (color-coded) |

## 🌿 Branching
| Command                 | Description                                      |
|-------------------------|--------------------------------------------------|
| `branch <name>`         | Create a new branch at current version           |
| `checkout <name>`       | Switch to another branch                         |
| `rebase <branch>`       | Replay current branch commits onto another       |

## 📈 Analysis
| Command                 | Description                                      |
|-------------------------|--------------------------------------------------|
| `filter <keyword>`      | Show commits whose message contains a keyword    |
| `avg`                   | Show average character count across all versions |

## 📂 Persistence
| Command                 | Description                                      |
|-------------------------|--------------------------------------------------|
| `save`                  | Save all branches and versions to `history.json` |
| `load`                  | Load from `history.json`                         |

## 🆘 Utilities
| Command                 | Description                                      |
|-------------------------|--------------------------------------------------|
| `help`                  | Display all available commands                   |
| `exit`                  | Exit the application                             |
