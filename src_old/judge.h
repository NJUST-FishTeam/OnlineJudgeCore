/*************************************************************************
	> File Name: judge.h
	> Author: maemual
	> Mail: maemual@gmail.com 
	> Created Time: 2013年03月22日 星期五 11时32分29秒
 ************************************************************************/

#ifndef __JUDGE_H__
#define __JUDGE_H__

#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

extern "C"
{
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>
}
#include "logger.h"
namespace judge_conf
{
	//judge本身的时间限制（ms）
	int JUDGE_TIME_LIMIT		= 15000;

	//编译时间限制（ms）
	int COMPILE_TIME_LIMIT		= 5000;

	//程序运行的栈空间大小
	int STACK_SIZE_LIMIT		= 8192;

	//运行时间附加值
	int TIME_LIMIT_ADDTION		= 0;

	//JAVA语言的运行时间放宽倍数
	int JAVA_TIME_FACTOR		= 2;

	//JAVA语言的运行内存放宽倍数
	int JAVA_MEM_FACTOR			= 2;

	//------------------以下是常量----------------------
	
	//OJ结果代码
	const int PROCEED			= 0;//已经处理并且正确运行退出
	const int CE				= 1;//编译错误
	const int TLE				= 2;//超时
	const int MLE				= 3;//超内存限制
	const int OLE				= 4;//输出文件超过大小限制
	const int RE				= 5;//运行时错误，包括数组越界、非法调用等

	//一些常量
	const int KILO				= 1024;
	const int MEGA				= KILO * KILO;
	const int GIGA				= KILO * MEGA;

	const int GCC_COMPILE_ERROR = 1;	//编译错误
	//退出原因
	const int EXIT_OK               = 0;//正常退出
    const int EXIT_UNPRIVILEGED     = 1;//缺乏权限退出
    const int EXIT_BAD_PARAM        = 3;//参数错误退出
    const int EXIT_VERY_FIRST       = 4;//judge程序运行超时退出
    const int EXIT_COMPILE          = 6;//编译错误退出
    const int EXIT_PRE_JUDGE        = 9;//预处理错误退出，如文件无法打开，无法fork进程等
    const int EXIT_PRE_JUDGE_PTRACE = 10;//ptrace运行错误退出
    const int EXIT_PRE_JUDGE_EXECLP = 11;//execlp运行错误退出
    const int EXIT_SET_LIMIT        = 15;//设置时间限制错误退出
    const int EXIT_SET_SECURITY     = 17;//设置安全限制错误退出
    const int EXIT_JUDGE            = 21;//程序调用错误退出
    const int EXIT_COMPARE          = 27;
    const int EXIT_COMPARE_SPJ      = 30;
    const int EXIT_COMPARE_SPJ_FORK = 31;
    const int EXIT_TIMEOUT          = 36;//超时退出
    const int EXIT_UNKNOWN          = 127;//不详
    
	//语言相关常量
	const std::string languages[] = {"unknown","c","c++","java"};
	const int LANG_UNKNOWN		= 0;
	const int LANG_C			= 1;
	const int LANG_CPP			= 2;
	const int LANG_JAVA			= 3;
}

namespace problem
{
	int id						= 0;
	int lang					= 0;
	int time_limit				= 1000;
	int memory_limit			= 65535;
	int output_limit			= 1024000;
	int result					= 0;
	int status;

	long memory_usage			= 0;
	int time_usage				= 0;

	std::string run;				//运行程序路径
	std::string program_name;		//运行程序文件名
	std::string input_data;			//输入文件路径
	std::string output_data;		//输出文件路径
	std::string result_data;		//判题结果文件路径
	std::string run_dir;			//程序的运行空间
}
void parse_arguments(int argc, char *argv[]){
	problem::run = argv[1];
	problem::time_limit = atoi(argv[2]);
	problem::memory_limit = atoi(argv[3]);
	problem::input_data = argv[4];
	problem::output_data = argv[5];
	problem::result_data = argv[6];

	if (problem::time_limit == 0){
		problem::time_limit = 0x3f3f3f3f;
		judge_conf::JUDGE_TIME_LIMIT = 0x3f3f3f3f;
	}
	if (problem::memory_limit == 0){
		problem::memory_limit = 4000000;
	}
	int len = problem::run.size();
	int i;
	for (i = len-1; i>=0; i--){
		if (problem::run[i] == '/')
			break;
	}
	problem::program_name = "";
	i++;
	for (; i < len; i++){
		problem::program_name += problem::run[i];
	}

	//判断语言类型
	problem::lang = judge_conf::LANG_CPP;
	FM_LOG_TRACE("problem::lang : %d", problem::lang);
	/*
	if (problem::source[len-2] == '.' && problem::source[len-1] == 'c')
		problem::lang = judge_conf::LANG_C;
	else if (problem::source[len-4] == '.' && problem::source[len-3] == 'c'
		&& problem::source[len-2] == 'p' && problem::source[len-1] == 'p')
		problem::lang = judge_conf::LANG_CPP;
	else if (problem::source[len-5] == '.' && problem::source[len-4] == 'j'
		&& problem::source[len-3] == 'a' && problem::source[len-2] == 'v'
		&& problem::source[len-1] == 'a')
		problem::lang = judge_conf::LANG_JAVA;
	else
		problem::lang = judge_conf::LANG_UNKNOWN;
	*/
	if (problem::lang == judge_conf::LANG_UNKNOWN){
		//printf("The language unknown.\n");
		FM_LOG_WARNING("The language unknown");
		exit(judge_conf::EXIT_BAD_PARAM);
	}

	char cwd[1024], *tmp = getcwd(cwd, 1024);
	std::string str((const char *)tmp);
	problem::run_dir = str+"/temp/";
/*
	//解析源文件名称
	int pos;
	for (int i = problem::source.length()-1; i >= 0; i--){
		if (problem::source[i] == '.'){
			pos = i;
			break;
		}
	}
	problem::program_name = problem::source.substr(0,pos);
*/
}
void timeout(int signo){
	if (signo == SIGALRM)
		exit(judge_conf::EXIT_TIMEOUT);
}

int malarm(int which, int milliseconds){
	struct itimerval t;
	FM_LOG_TRACE("malarm: %d", milliseconds);
	t.it_value.tv_sec		= milliseconds / 1000;
	t.it_value.tv_usec		= milliseconds % 1000 * 1000;//微秒
	t.it_interval.tv_sec	= 0;
	t.it_interval.tv_usec	= 0;
	return setitimer(which, &t, NULL);
}
void IO_redirect(){
	FM_LOG_TRACE("IO is redirecting.");
	//printf("in io redirect.\n");
	//printf("%s\n",problem::input_data.c_str());
	stdin = freopen(problem::input_data.c_str(), "r", stdin);
	//printf("%s\n",problem::output_data.c_str());
	stdout = freopen(problem::output_data.c_str(), "w", stdout);
	//stdout = freopen("./out.out", "w", stdout);
	//printf("WTF?\n");
	stderr = freopen("./temp/stderr.txt", "w", stderr);

	if (stdin == NULL || stdout == NULL || stderr == NULL){
		//printf("error freopen.\n");
		FM_LOG_WARNING("error freopen: stdin(%p), stdout(%p), stderr(%p)",
				stdin, stdout, stderr);
		exit(judge_conf::EXIT_PRE_JUDGE);
	}
	//printf("IO is ok.\n");
	FM_LOG_TRACE("IO redirect OK.");
}
void set_security(){
	struct passwd *nobody = getpwnam("nobody");
	if (nobody == NULL){
		//printf("no user named 'nobody'\n");
		FM_LOG_WARNING("no user named 'nobody'? %d : %s",errno,strerror(errno));
		exit(judge_conf::EXIT_SET_SECURITY);
	}

	//chdir
	if (EXIT_SUCCESS != chdir(problem::run_dir.c_str())){
		//printf("chdir failed.\n");
		FM_LOG_WARNING("chdir(%s) failed. %d: %s",
				problem::run_dir.c_str(), errno, strerror(errno));
		exit(judge_conf::EXIT_SET_SECURITY);
	}

	char cwd[1024], *tmp = getcwd(cwd, 1024);
	if (tmp == NULL){
		//printf("getcwd failed.\n");
		FM_LOG_WARNING("getcwd failed. %d: %s", errno, strerror(errno));
		exit(judge_conf::EXIT_SET_SECURITY);
	}
	//chroot
	if (problem::lang != judge_conf::LANG_JAVA){
		if (EXIT_SUCCESS != chroot(cwd)){
			//printf("chroot failed.\n");
			FM_LOG_WARNING("chroot(%s) failed. %d: %s",
					cwd, errno, strerror(errno));
			exit(judge_conf::EXIT_SET_SECURITY);
		}
	}

	//setuid
	if (EXIT_SUCCESS != setuid(nobody->pw_uid)){
		//printf("setuid failed.\n");
		FM_LOG_WARNING("setuid(%d) failed. %d : %s",
				nobody->pw_uid, errno, strerror(errno));
		exit(judge_conf::EXIT_SET_SECURITY);
	}
}
void set_limit(){
	//时间限制
	//printf("Im in set_limit\n");
	rlimit lim;
	lim.rlim_max = (problem::time_limit + 999) / 1000 + 1;//硬限制
	lim.rlim_cur = lim.rlim_max;
	if (setrlimit(RLIMIT_CPU, &lim) < 0){
		//printf("setrlimit failed.\n");
		FM_LOG_WARNING("error setrlimit for RLIMIT_CPU");
		exit(judge_conf::EXIT_SET_LIMIT);
	}
	//printf("time limited.\n");
	//不能直接在这里限制内存
	//由于linux的机制问题
	//要每次wait后计算缺页数来计算内存
	
	//堆栈空间限制
	getrlimit(RLIMIT_STACK, &lim);

	int rlim = judge_conf::STACK_SIZE_LIMIT * judge_conf::KILO;
	if (lim.rlim_max <= rlim){
		//printf("can't set stack size.\n");
		FM_LOG_WARNING("cant't set stack size to higher(%d <= %d)",
				lim.rlim_max, rlim);
	}else{
		lim.rlim_max = rlim;
		lim.rlim_cur = rlim;

		if (setrlimit(RLIMIT_STACK, &lim) < 0){
			//printf("setrlimit RLIMIT_STACK failed.\n");
			FM_LOG_WARNING("setrlimit RLIMIT_STACK failed: %s", strerror(errno));
			exit(judge_conf::EXIT_SET_LIMIT);
		}
	}


	log_close();

	//输出文件大小限制
	//关闭log
	
	lim.rlim_max = problem::output_limit * judge_conf::KILO;
	lim.rlim_cur = lim.rlim_max;
	if (setrlimit(RLIMIT_FSIZE, &lim) < 0){
		//printf("setrlimit RLIMIT_FSIZE failed.\n");
		perror("setrlimit RLIMIT_FSIZE failed.");
		exit(judge_conf::EXIT_SET_LIMIT);
	}
	//printf("set limtit is ok.\n");
//	FM_LOG_TRACE("set limit OK.");
}
void output_results(int status, int time, int mem){
	//输出结果，status为Proceed, TLE, MLE, RE
	//time为程序运行时间
	//mem为程序消耗内存
	FILE * fresult = fopen(problem::result_data.c_str(), "w");
	if (status == judge_conf::PROCEED){
		fprintf(fresult, "Final Result: Proceeding\r\nRun Time: %d ms\r\nRun Memory: %d KB\r\nDetail: \r\n", time, mem);
	}else if (status == judge_conf::TLE){
		fprintf(fresult, "Final Result: Time Limit Exceeded\r\nRun Time: Unavailable\r\nRun Memory: Unavailable\r\nDetail: \r\n");
	}else if (status == judge_conf::MLE){
		fprintf(fresult, "Final Result: Memory Limit Exceeded\r\nRun Time: Unavailable\r\nRun Memory: Unavailable\r\nDetail: \r\n");
	}else if (status == judge_conf::RE){
		fprintf(fresult, "Final Result: Runtime Error\r\nRun Time: Unavailable\r\nRun Memory: Unavailable\r\nDetail: \r\n");
	}else if (status == judge_conf::CE){
		fprintf(fresult, "Final Result: Compiler Error\r\nRun Time: Unavailable\r\nRun Memory: Unavailable\r\nDetail: \r\n");
	}
	FM_LOG_TRACE("output the results is ok.");
}
#include "rf_table.h"
//系统调用在进和出的时候都会暂停, 把控制权交给judge
static bool in_syscall = true;
bool is_valid_syscall(int lang, int syscall_id, pid_t child, user_regs_struct regs)
{
    in_syscall = !in_syscall;
    //FM_LOG_DEBUG("syscall: %d, %s, count: %d", syscall_id, in_syscall?"in":"out", RF_table[syscall_id]);
    if (RF_table[syscall_id] == 0)
    {
        //如果RF_table中对应的syscall_id可以被调用的次数为0, 则为RF
        long addr;
        if(syscall_id == SYS_open)
        {
#if __WORDSIZE == 32
            addr = regs.ebx;
#else
            addr = regs.rdi;
#endif
#define LONGSIZE sizeof(long)
            union u{ unsigned long val; char chars[LONGSIZE]; }data;
            unsigned long i = 0, j = 0, k = 0;
            char filename[300];
            while (true)
            {
                data.val = ptrace(PTRACE_PEEKDATA, child, addr + i,  NULL);
                i += LONGSIZE;
                for (j = 0; j < LONGSIZE && data.chars[j] > 0 && k < 256; j++) 
                {
                    filename[k++] = data.chars[j];
                }
                if (j < LONGSIZE && data.chars[j] == 0)
                    break;
            }
            filename[k] = 0;
            FM_LOG_TRACE("syscall open: filename: %s", filename);
            if (strstr(filename, "..") != NULL)
            {
                return false;
            }
            if (strstr(filename, "/proc/") == filename)
            {
                return true;
            }
            if (strstr(filename, "/dev/tty") == filename)
            {
                //output_result(judge_conf::OJ_RE_SEGV);
				output_results(judge_conf::RE, 0, 0);
                exit(judge_conf::EXIT_OK);
            }
        }
        return false;
    }
    else if (RF_table[syscall_id] > 0)
    {
        //如果RF_table中对应的syscall_id可被调用的次数>0
        //且是在退出syscall的时候, 那么次数减一
        if (in_syscall == false)
            RF_table[syscall_id]--;
    }
    else
    {
        //RF_table中syscall_id对应的指<0, 表示是不限制调用的
        ;
    }
    return true;
}
#endif
