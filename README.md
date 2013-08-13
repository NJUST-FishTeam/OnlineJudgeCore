NJUST Open Judge 判题核心(Linux平台)
=========================

# 文件说明
## judge.cpp
核心程序
## judge.h
主要头文件
## logge.h
日志系统
## rf_table.h
系统调用禁止表

# 参数说明
argc = 7
argv[0]: 自身路径
argv[1]: 待执行程序的路径
argv[2]：时间限制，以ms为单位，0表示不限制
argv[3]: 内存限制，以KB为单位，0表示不限制
argv[4]: 输入文件路径
argv[5]：实际输出文件路径
argv[6]: 判题结果文件路径

# 判题结果格式
Final Result: Proceeded 或 Runtime Error 或 Time Limit Exceeded 或 Memory Limit Exceeded \r\n
Run Time: Unavailable 或 (Int32) ms \r\n
Run Memory: Unavailable 或 (Int32) KB \r\n
Detail: \r\n
以下为其他额外信息
