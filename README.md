NJUST Open Judge 判题核心(Linux平台)
=========================

## 综述

此判题核心程序通过构建一个沙盒，运行用户提交的代码，
利用ptrace跟踪，限制用户代码的行为和资源消耗。

`core.h`文件中是一些常量和全局变量的定义

`core.cpp`是主程序文件

`logger.h`是一个简易的日志程序

`rf_table.h`是一个限制系统调用的表

判题核心通过传入命令行参数获知输入，结果输出到文件，
所以不具有线程安全性。

## 参数

`-c` 源代码路径

`-t` 时间限制，可选，默认1000 MS

`-m` 内存限制，可选，默认65535 KB

`-s` 表示是否是special judge

`-S` special judge程序的语言，1是C，2是C++，3是Java

`-d` 表示运行的文件夹

示例：

    sudo ./Core -c ./test/test.c -t 1000 -m 65535 -s -S 2 -d ./test/

上面表明，用户提交的代码是`./test/test.c`，时限1000 ms，内存限制65535 KB，
需要SpecialJudge，SpecialJudge程序的语言是C++，运行的文件夹在`./test/`下。

## 程序编译

    g++ core.cpp -o Core -O2

## 约定

构建的沙盒在`./test/`文件夹下

其中c语言程序是xxx.c，C++ 程序是xxx.cpp，Java程序是Main.java

标准输入文件是`in.in`，标准输出文件是`out.out`，程序运行返回的结果是`result.txt`文件

SpecialJudge程序是一个已经编译好的程序，名字叫`SpecialJudge`。运行结束后请自行清理`./test/`文件夹
下的各种文件

## result.txt

返回结果格式：

第一行：结果，`Compile Error`等

第二行：运行时间，不带单位，默认毫秒

第三行：内存消耗，不带单位，默认KB

接下来所有行：额外信息，一般情况下为空，当`Compile Error`时，编译错误信息存在这里


