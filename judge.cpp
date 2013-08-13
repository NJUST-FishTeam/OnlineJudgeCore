/*************************************************************************
  > File Name: judge.cpp
  > Author: maemual
  > Mail: maemual@gmail.com
  > Created Time: 2013年03月22日 星期五 11时49分15秒
  > Version: 1.0
  > Modify Time: 2013年04月03日 星期三 17时44分10秒
 ************************************************************************/

#include <iostream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>
extern "C"
{
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/ptrace.h>
}
#include "judge.h"

using namespace std;


int main(int argc, char *argv[])
{
	log_open("./errorLog/core.log");
	//root
	if (geteuid() != 0){
		FM_LOG_FATAL("euid != 0, please run as root.");
		output_results(judge_conf::RE, 0, 0);
		exit(judge_conf::EXIT_UNPRIVILEGED);
	}

	//解析选项
	parse_arguments(argc,argv);

	judge_conf::JUDGE_TIME_LIMIT += problem::time_limit;
	//设定定时器，在到达judge_conf::JUDGE_TIME_LIMIT后发出SIGALARM信号
	//表示整个judge的过程不能超过这一值
	//当judge正常退出，即EXIT_SUCCESS
	//若超时，则返回judge_conf::EXIT_TIMEOUT
	if (EXIT_SUCCESS != malarm(ITIMER_REAL, judge_conf::JUDGE_TIME_LIMIT)){
		FM_LOG_WARNING("set alarm for judge failed. %d : %s", errno, strerror(errno));
		output_results(judge_conf::RE, 0, 0);
		exit(judge_conf::EXIT_VERY_FIRST);
	}
	signal(SIGALRM, timeout);

	/*
	//编译------------------------------------------------------------------
	pid_t compiler = fork();
	int status = 0;
	if (compiler < 0){
		//printf("fork compiler error.\n");
		FM_LOG_WARNING("error fork compiler.");
		output_results(judge_conf::RE, 0, 0);
		exit(judge_conf::EXIT_COMPILE);
	}else if (compiler == 0){
		//编译器子进程

		log_add_info("compiler");

		stdout = freopen("./temp/compiler_log.log", "w", stdout);
		stderr = freopen("./temp/compiler_log.log", "w", stderr);
		if (stdout == NULL || stderr == NULL){
			FM_LOG_WARNING("error freopen: stdout(%p), stderr(%p)", stdout,stderr);
			output_results(judge_conf::RE, 0, 0);
			exit(judge_conf::EXIT_COMPILE);
		}
		//设定编译的时间限制
		malarm(ITIMER_REAL, judge_conf::COMPILE_TIME_LIMIT);
		switch (problem::lang)
		{
			case judge_conf::LANG_C:
				FM_LOG_TRACE("start: gcc");
				execlp("gcc", "gcc",
						"-o",(problem::run_dir+"a.out").c_str(),
						problem::run.c_str(), "-static", "-w", "-lm",
						"-std=c99", //"-O2",
						NULL);
				break;

			case judge_conf::LANG_CPP:
				FM_LOG_TRACE("start: g++");
				execlp("g++", "g++",
						"-o",(problem::run_dir+"a.out").c_str(), 
						problem::run.c_str(), "-static", 
						"-w", //"-O2",
						"-lm",
						NULL);
				break;

			case judge_conf::LANG_JAVA:
				FM_LOG_TRACE("start: javac");
				execlp("javac", "javac", problem::run.c_str(),
						"-d",problem::run_dir.c_str(),
						NULL);
		}
		FM_LOG_WARNING("exec errno.");
		output_results(judge_conf::RE, 0, 0);
		exit(judge_conf::EXIT_COMPILE);
	}else{
		//judge进程，父进程
		pid_t w = waitpid(compiler, &status, WUNTRACED);
		if (w == -1){
			//printf("waitpid error.\n");
			FM_LOG_WARNING("waitpid error.");
			output_results(judge_conf::RE, 0, 0);
			exit(judge_conf::EXIT_COMPILE);
		}

		//printf("compiler finished.\n");
		FM_LOG_TRACE("compiler finished.");
		if (WIFEXITED(status)){
			//编译程序自己退出
			if (EXIT_SUCCESS == WEXITSTATUS(status)){
				//printf("compile succeed.\n");
				FM_LOG_TRACE("compiler succeeded.");
			}else if (judge_conf::GCC_COMPILE_ERROR == WEXITSTATUS(status)){
				//printf("compiler error.\n");
				FM_LOG_TRACE("compiler error.");
				output_results(judge_conf::CE, 0, 0);
				exit(judge_conf::EXIT_OK);
			}else{
				//printf("compiler unknown exit status.\n");
				FM_LOG_WARNING("compiler unknown exit status %d", WEXITSTATUS(status));
				output_results(judge_conf::CE, 0, 0);
				exit(judge_conf::EXIT_COMPILE);
			}
		}else{
			//编译程序因为超时等其他原因被杀死
			if (WIFSIGNALED(status)){
				if (SIGALRM == WTERMSIG(status)){
					//printf("compiler time limit exceeded.\n");
					FM_LOG_WARNING("compiler time limit exceeded.");
					output_results(judge_conf::CE, 0, 0);
					exit(judge_conf::EXIT_OK);
				}else
					//printf("unknown signal.\n");
					FM_LOG_WARNING("unknown signal(%d)",WTERMSIG(status));
			}else if (WIFSTOPPED(status)){
				//printf("stopped by signal.\n");
				FM_LOG_WARNING("stopped by signal %d\n", WSTOPSIG(status));
			}else{
				//printf("unknown stop reason.");
				FM_LOG_WARNING("unknown stop reason.");
			}
			output_results(judge_conf::CE, 0, 0);
			exit(judge_conf::EXIT_COMPILE);
		}
	}
	*/
//judge--------------------------------------------------------------------
	FM_LOG_TRACE("start to judge.");
	struct rusage rused;
	pid_t executive = fork();
	if (executive < 0){
		FM_LOG_WARNING("fork for child failed.");
		output_results(judge_conf::RE, 0, 0);
		exit(judge_conf::EXIT_PRE_JUDGE);
	}else if (executive == 0){
		//子进程，用于运行用户提交的程序

		log_add_info("executive....");
		//IO重定向
		IO_redirect();
		//安全相关设置，如seteuid, chroot等
		set_security();
		int real_time_limit = problem::time_limit
			+ judge_conf::TIME_LIMIT_ADDTION;
		//判题时间限制
		if (EXIT_SUCCESS != malarm(ITIMER_REAL, real_time_limit)){
			FM_LOG_WARNING("malarm failed.");
			output_results(judge_conf::RE, 0, 0);
			exit(judge_conf::EXIT_PRE_JUDGE);
		}

		//setlimit,包括时间、空间、输出文件的限制
		set_limit();
		if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0){
			output_results(judge_conf::RE, 0, 0);
			exit(judge_conf::EXIT_PRE_JUDGE_PTRACE);
		}

		//载入程序
		if (problem::lang != judge_conf::LANG_JAVA){
			execl(("./"+problem::program_name).c_str(), problem::program_name.c_str(), NULL);
		}else{
			execlp("java", "java", "-Djava.security.manager",
					"-Djava.security.policy==./java.policy", "Main",
					NULL);
		}

		output_results(judge_conf::RE, 0, 0);
		exit(judge_conf::EXIT_PRE_JUDGE_EXECLP);
	}else{
		//父进程

		int status = 0;
		int syscall_id = 0;
		struct user_regs_struct regs;
		init_RF_table(problem::lang);
		FM_LOG_TRACE("start juding....");
		while (1)
		{
			if (wait4(executive, &status, 0, &rused) < 0){
				FM_LOG_WARNING("wait4 failed, %d: %s", errno, strerror(errno));
				output_results(judge_conf::RE, 0, 0);
				exit(judge_conf::EXIT_JUDGE);
			}

			if (WIFEXITED(status)){
				//子程序主动退出

				//JAVA返回值非0表示出错，其他语言不考虑返回值
				if (problem::lang != judge_conf::LANG_JAVA
						|| WEXITSTATUS(status) == EXIT_SUCCESS){
					//子程序返回0
					FM_LOG_TRACE("normal quit.");
				}else{
					//子程序返回异常
					FM_LOG_TRACE("abnormal quit.");
				}
				break;
			}
			if ( WIFSIGNALED(status) ||
					//超过软/硬限制收到信号
					(WIFSTOPPED(status) && WSTOPSIG(status) != SIGTRAP))
				//程序收到信号暂停，但是并未结束
				//被Ptrace暂停的情况(SIGTRAP)是正常的，要过滤掉
			{
				//判断是否是RE/TLE/OLE等退出情况
				int signo = 0;
				if (WIFSIGNALED(status)){
					signo = WTERMSIG(status);
					FM_LOG_TRACE("child signaled by %d: %s", signo, strsignal(signo));
				}else{
					signo = WSTOPSIG(status);
					FM_LOG_TRACE("child stopped by %d: %s", signo, strsignal(signo));
				}

				switch(signo)
				{
					//TLE
					case SIGALRM:
					case SIGXCPU:
					case SIGVTALRM:
						//printf("time limit exceeded.\n");
						FM_LOG_TRACE("Time limit exceeded.");
						output_results(judge_conf::TLE, 0, 0);
						exit(judge_conf::EXIT_OK);
						break;
					//OLE
					case SIGXFSZ:
						//printf("file limit exceed.\n");
						FM_LOG_TRACE("File limit exceed.");
						break;
					//RE
					case SIGSEGV:
					case SIGFPE:
					case SIGBUS:
					case SIGABRT:
					case SIGKILL:
						//printf("runtime error.\n");
						FM_LOG_TRACE("Runtime Error.");
						output_results(judge_conf::RE, 0, 0);
						exit(judge_conf::EXIT_OK);
						break;
					default:
						//printf("unknown.\n");
						FM_LOG_TRACE("Unknown signal.");
						output_results(judge_conf::RE, 0, 0);
						exit(judge_conf::EXIT_OK);
						break;
				}
				ptrace(PTRACE_KILL, executive, NULL, NULL);
				break;
			}
			//MLE
			problem::memory_usage = std::max(problem::memory_usage,
					rused.ru_minflt * (getpagesize() / judge_conf::KILO));
			if (problem::memory_usage > problem::memory_limit)
			{
				FM_LOG_TRACE("Memory limit exceed.");
				output_results(judge_conf::MLE, 0, 0);
				ptrace(PTRACE_KILL, executive, NULL, NULL);
				break;
			}
			//截获的SYSCALL, 检查
			if (ptrace(PTRACE_GETREGS, executive, NULL, &regs) < 0)
			{
				FM_LOG_WARNING("ptrace(PTRACE_GETREGS) failed, %d: %s",errno, strerror(errno));
				output_results(judge_conf::RE, 0, 0);
				exit(judge_conf::EXIT_JUDGE);
			}
#ifdef __i386__
			syscall_id = regs.orig_eax;
#else
			//此处是从bnuoj直接copy过来的，没有用i386平台测试过...
			syscall_id = regs.orig_rax;
#endif
			if (syscall_id > 0 && !is_valid_syscall(problem::lang, syscall_id, executive, regs))
			{
				FM_LOG_TRACE("restricted function, syscall_id: %d", syscall_id);
				if (syscall_id == SYS_rt_sigprocmask) //occur when glibc fail
				{
					//problem::result = judge_conf::OJ_RE_SEGV;
					FM_LOG_WARNING("Runtime error : SEGV.");
				}
				else
				{
					//problem::result = judge_conf::OJ_RF;
					FM_LOG_WARNING("restricted fuction.");
				}
				output_results(judge_conf::RE, 0, 0);
				ptrace(PTRACE_KILL, executive, NULL, NULL);
				break;
			}
			if (ptrace(PTRACE_SYSCALL, executive, NULL, NULL) < 0){
				//printf("ptrace failed.\n");
				FM_LOG_WARNING("ptrace failed.");
				output_results(judge_conf::RE, 0, 0);
				exit(judge_conf::EXIT_JUDGE);
			}
		}
		problem::time_usage += rused.ru_utime.tv_sec * 1000
			+ rused.ru_utime.tv_usec / 1000;
		problem::time_usage += rused.ru_stime.tv_sec * 1000
			+ rused.ru_stime.tv_usec / 1000;
	}
	//output the result
	output_results(judge_conf::PROCEED, problem::time_usage, problem::memory_usage);
	return 0;
}
//Thanks for the help of felix021
