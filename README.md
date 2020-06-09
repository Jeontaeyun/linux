# Linux System Programming Document

For Compile C, You need to use gcc(GNU Compiler Collection)

## 01. Compile with gcc

```bash
$gcc -o YYY(Output File Name) XXX.c
```

And then you can get `executable file`. `-o` option is for customizing output file name.

## 02. Strace with mac

`strace` is tool for checking system call which is calling from work in progress program.  
In term of Mac OS, we can use `dstruss` command.

```bash
$sudo dtruss <PROGRAM_NAME> -e trace=open,read,write,close
```
