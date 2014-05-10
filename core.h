#ifndef CORE_H
#define CORE_H

#include <string>

namespace JUDGE_CONF
{
int JUDGE_TIME_LIMIT   = 15000;  //judge本身的时间限制（ms）

int COMPILE_TIME_LIMIT = 5000;  //编译时间限制（ms）

int STACK_SIZE_LIMIT   = 8192;  //程序运行的栈空间大小

int TIME_LIMIT_ADDTION = 0;  //运行时间附加值

int JAVA_TIME_FACTOR   = 2;  //JAVA语言的运行时间放宽倍数

int JAVA_MEM_FACTOR    = 2;  //JAVA语言的运行内存放宽倍数

//------------------以下是常量----------------------

//OJ结果代码
const int PROCEED = 0;  //已经处理并且正确运行退出
const int CE      = 1;  //编译错误
const int TLE     = 2;  //超时
const int MLE     = 3;  //超内存限制
const int OLE     = 4;  //输出文件超过大小限制
const int RE      = 5;  //运行时错误，包括数组越界、非法调用等

//一些常量
const int KILO = 1024;
const int MEGA = KILO * KILO;
const int GIGA = KILO * MEGA;

//const int GCC_COMPILE_ERROR = 1;    //编译错误
//退出原因
const int EXIT_OK               = 0;  //正常退出
const int EXIT_UNPRIVILEGED     = 1;  //缺乏权限退出
const int EXIT_BAD_PARAM        = 3;  //参数错误退出
const int EXIT_VERY_FIRST       = 4;  //judge程序运行超时退出
const int EXIT_COMPILE          = 6;  //编译错误退出
const int EXIT_PRE_JUDGE        = 9;  //预处理错误退出，如文件无法打开，无法fork进程等
const int EXIT_PRE_JUDGE_PTRACE = 10;  //ptrace运行错误退出
const int EXIT_PRE_JUDGE_EXECLP = 11;  //execlp运行错误退出
const int EXIT_SET_LIMIT        = 15;  //设置时间限制错误退出
const int EXIT_SET_SECURITY     = 17;  //设置安全限制错误退出
const int EXIT_JUDGE            = 21;  //程序调用错误退出
const int EXIT_COMPARE          = 27;
const int EXIT_COMPARE_SPJ      = 30;
const int EXIT_COMPARE_SPJ_FORK = 31;
const int EXIT_TIMEOUT          = 36;  //超时退出
const int EXIT_UNKNOWN          = 127;  //不详

//语言相关常量
const std::string languages[] = {"Unknown", "C", "C++", "Java"};
const int LANG_UNKNOWN = 0;
const int LANG_C       = 1;
const int LANG_CPP     = 2;
const int LANG_JAVA    = 3;

}

namespace PROBLEM
{
 
int id           = 0;
int lang         = 0;
int time_limit   = 1000;   //MS
int memory_limit = 65535;  //KB
int output_limit = 1024000; //KB
int result       = 0;
int status;

long memory_usage = 0;
int time_usage    = 0;

// std::string run;  //运行程序路径
// std::string program_name;  //运行程序文件名
// std::string input_data;  //输入文件路径
// std::string output_data;  //输出文件路径
// std::string result_data;  //判题结果文件路径
// std::string run_dir;  //程序的运行空间
std::string code_path;
std::string program_name = "a.out"
std::string input_path;
std::string output_path;
std::string result_path;
std::string run_dir;
}

#endif