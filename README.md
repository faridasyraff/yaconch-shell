# Yaconch 🐚

A Unix shell written in C by Muhammad Farid Asyraf, built from scratch as a systems programming learning project.

## About
Yaconch is a custom Unix shell implementation that explores core OS concepts through hands-on building. The name stands for **Yet Another Conch** — a nod to the Unix tradition of "yet another" projects, and the conch shell as a symbol of communication.

## Features
- **Command execution** — run any program available in your PATH
- **Built-in commands** — `cd`, `help`, `exit`
- **I/O Redirection** — `>`, `>>`, `<`
- **Pipes** — chain commands with `|`, supports multi-command pipelines
- **Signal handling** — `Ctrl+C` (SIGINT) and `Ctrl+Z` (SIGTSTP) handled gracefully
- **Background processes** — run commands with `&`
- **Zombie reaping** — background processes cleaned up automatically via SIGCHLD

## Building
```bash
gcc -std=c17 -Wall -Wextra -o yaconch yaconch.c
```

Or with make (if Makefile is present):
```bash
make
```

## Usage
```bash
./yaconch
```

### Basic commands
```bash
>> ls -la
>> cd /home
>> echo hello
>> help
>> exit
```

### Redirection
```bash
>> ls > output.txt          # redirect stdout to file
>> ls >> output.txt         # append stdout to file
>> cat < input.txt          # redirect stdin from file
>> cat < input.txt > out.txt # both
```

### Pipes
```bash
>> ls | grep .c
>> ls | grep .c | wc -l
>> cat < input.txt | grep foo > output.txt
```

### Background processes
```bash
>> sleep 10 &               # run in background
[1] pid:[1234]
>>                          # prompt returns immediately
```

### Signals
```bash
>> sleep 100
^C                          # kills sleep, yaconch survives
>> sleep 100
^Z                          # suspends sleep, yaconch survives
```

## OS Concepts Explored
This project was built as a deliberate exploration of operating systems internals:

| Feature | Concepts Learned |
|---|---|
| Command execution | `fork()`, `execvp()`, `waitpid()`, process lifecycle |
| Signal handling | `sigaction()`, signal masks, SIGINT, SIGTSTP, SIGCHLD |
| I/O Redirection | File descriptors, `dup2()`, `open()`, everything is a file |
| Pipes | IPC, `pipe()`, producer-consumer synchronization, reference counting |
| Background processes | Async process management, zombie reaping, WNOHANG |

## Project Structure
```
yaconch.c       — entire shell implementation (single file)
```

## Known Limitations / Work In Progress
- No job control (`jobs`, `fg`, `bg`) yet — in progress
- No environment variable expansion (`$VAR`)
- No command history (arrow keys)
- No tab completion
- No quote handling in parser (`"multi word arg"`)

## Roadmap
- [ ] Job control (`jobs`, `fg`, `bg`)
- [ ] Environment variables
- [ ] `/proc` explorer builtin
- [ ] Command history
- [ ] Wildcard expansion
- [ ] Shell scripting
- [ ] `chroot` builtin

## Version
0.2.0

## Author
Muhammad Farid Asyraf
