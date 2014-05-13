extern "C"
{
#include <cstdio>
#include <stdlib.h>
#include <cstring>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/ptrace.h>
}
#include "core.h"
#include "rf_table.h"

extern int errno;

static
bool has_suffix(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static
void parse_arguments(int argc, char* argv[]) {
    int opt;
    extern char *optarg;

    while ((opt = getopt(argc, argv, "c:t:m:d:S:s")) != -1) {
        switch (opt) {
            case 'c': PROBLEM::code_path    = optarg;         break;
            case 't': PROBLEM::time_limit   = atoi(optarg);   break;
            case 'm': PROBLEM::memory_limit = atoi(optarg);   break;
            case 's': PROBLEM::spj          = true;           break;
            case 'S': PROBLEM::spj_code_file= optarg;         break;
            case 'd': PROBLEM::run_dir      = optarg;         break;
            default:
                printf("unkown option\n");
                exit(JUDGE_CONF::EXIT_BAD_PARAM);
        }
    }

    if (has_suffix(PROBLEM::code_path, ".cpp")){
        PROBLEM::lang = JUDGE_CONF::LANG_CPP;
    }else if (has_suffix(PROBLEM::code_path, ".c")){
        PROBLEM::lang = JUDGE_CONF::LANG_C;
    }else if (has_suffix(PROBLEM::code_path, ".java")){
        PROBLEM::lang = JUDGE_CONF::LANG_JAVA;
    }else{
        printf("unkwon language\n");
        exit(JUDGE_CONF::EXIT_BAD_PARAM);
    }

    PROBLEM::exec_file = PROBLEM::run_dir + "/a.out";
    PROBLEM::input_file = PROBLEM::run_dir + "/in.in";
    PROBLEM::output_file = PROBLEM::run_dir + "/out.out";
    PROBLEM::exec_output = PROBLEM::run_dir + "/out.txt";
    PROBLEM::stdout_file_compiler = PROBLEM::run_dir + "/stdout_file_compiler.txt";
    PROBLEM::stderr_file_compiler = PROBLEM::run_dir + "/stderr_file_compiler.txt";

    if (PROBLEM::lang == JUDGE_CONF::LANG_JAVA) {
        PROBLEM::exec_file = PROBLEM::run_dir + "/Main";
        PROBLEM::time_limit *= JUDGE_CONF::JAVA_TIME_FACTOR;
        PROBLEM::memory_limit *= JUDGE_CONF::JAVA_MEM_FACTOR;
    }
}

static
void timeout(int signo) {
    if (signo == SIGALRM)
        exit(JUDGE_CONF::EXIT_TIMEOUT);
}

static
int malarm(int which, int milliseconds) {
    struct itimerval t;
    //设置时间限制
    t.it_value.tv_sec     = milliseconds / 1000;
    t.it_value.tv_usec    = milliseconds % 1000 * 1000; //微秒
    t.it_interval.tv_sec  = 0;
    t.it_interval.tv_usec = 0;
    return setitimer(which, &t, NULL);
}

static
void io_redirect() {
    stdin = freopen(PROBLEM::input_file.c_str(), "r", stdin);
    stdout = freopen(PROBLEM::output_file.c_str(), "w", stdout);
    stderr = freopen("/dev/null", "w", stderr);

    if (stdin == NULL || stdout == NULL || stderr == NULL) {
        printf("freopen error\n");
        exit(JUDGE_CONF::EXIT_PRE_JUDGE);
    }
}

static
void security_control() {
    struct passwd *nobody = getpwnam("nobody");
    if (nobody == NULL){
        printf("no user name nobody\n");
        exit(JUDGE_CONF::EXIT_SET_SECURITY);
    }

    //chdir
    if (EXIT_SUCCESS != chdir(PROBLEM::run_dir.c_str())) {
        printf("chdir failed\n");
        exit(JUDGE_CONF::EXIT_SET_SECURITY);
    }

    char cwd[1024], *tmp = getcwd(cwd, 1024);
    if (tmp == NULL) {
        printf("getcwd failed\n");
        exit(JUDGE_CONF::EXIT_SET_SECURITY);
    }

    //chroot
    if (PROBLEM::lang != JUDGE_CONF::LANG_JAVA) {
        if (EXIT_SUCCESS != chroot(cwd)) {
            printf("chroot failed\n");
            exit(JUDGE_CONF::EXIT_SET_SECURITY);
        }
    }

    //setuid
    if (EXIT_SUCCESS != setuid(nobody->pw_uid)) {
        printf("set uid failed\n %d: %s\n", errno, strerror(errno));
        exit(JUDGE_CONF::EXIT_SET_SECURITY);
    }
}

static
void set_limit() {
    rlimit lim;

    lim.rlim_max = (PROBLEM::time_limit - PROBLEM::time_usage + 999) / 1000 + 1;//硬限制
    lim.rlim_cur = lim.rlim_max; //软限制
    if (setrlimit(RLIMIT_CPU, &lim) < 0) {
        printf("setrlimit for RLIMIT_CPU error\n");
        exit(JUDGE_CONF::EXIT_SET_LIMIT);
    }

    //内存不能在此做限制


    //堆栈空间限制
    getrlimit(RLIMIT_STACK, &lim);
    //printf

    int rlim = JUDGE_CONF::STACK_SIZE_LIMIT * JUDGE_CONF::KILO;
    if (lim.rlim_max <= rlim) {
        printf("cannot set stack size\n");
    } else {
        lim.rlim_max = rlim;
        lim.rlim_cur = rlim;

        if (setrlimit(RLIMIT_STACK, &lim) < 0) {
            printf("setrlimt RLIMIT_STACK failed\n");
            exit(JUDGE_CONF::EXIT_SET_LIMIT);
        }
    }

    //log_close()

    //输出文件大小限制
    lim.rlim_max = PROBLEM::output_limit * JUDGE_CONF::KILO;
    lim.rlim_cur = lim.rlim_max;
    if (setrlimit(RLIMIT_FSIZE, &lim) < 0) {
        printf("setrlimit RLIMIT_FSIZE failed\n");
        exit(JUDGE_CONF::EXIT_SET_LIMIT);
    }
}

static
void output_result(int result, int time_usage = 0, int memory_usage = 0) {
    printf("%d %d %d\n", result, time_usage, memory_usage);
}

#include "rf_table.h"
//系统调用在进和出的时候都会暂停, 把控制权交给judge
static bool in_syscall = true;
static
bool is_valid_syscall(int lang, int syscall_id, pid_t child, user_regs_struct regs) {
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
            //FM_LOG_TRACE("syscall open: filename: %s", filename);
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
                exit(JUDGE_CONF::EXIT_OK);
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

static
void compiler_source_code() {
    pid_t compiler = fork();
    int status = 0;
    if (compiler < 0) {
        printf("fork compiler error\n");
        exit(JUDGE_CONF::EXIT_COMPILE);
    }else if (compiler == 0) {
        //log
        stdout = freopen(PROBLEM::stdout_file_compiler.c_str(), "w", stdout);
        stderr = freopen(PROBLEM::stderr_file_compiler.c_str(), "w", stderr);
        if (stdout == NULL || stderr == NULL) {
            printf("error freopen stdout, stderr\n");
            exit(JUDGE_CONF::EXIT_COMPILE);
        }

        malarm(ITIMER_REAL, JUDGE_CONF::COMPILE_TIME_LIMIT);
        switch (PROBLEM::lang) {
            case JUDGE_CONF::LANG_C:
                printf("compiler gcc\n");
                execlp("gcc", "gcc", "-o", PROBLEM::exec_file.c_str(), PROBLEM::code_path.c_str(),
                        "-static", "-w", "-lm", "-std=c99", "-O2", "-DONLINE_JUDGE", NULL);
                break;
            case JUDGE_CONF::LANG_CPP:
                printf("compiler g++\n");
                execlp("g++", "g++", "-o", PROBLEM::exec_file.c_str(), PROBLEM::code_path.c_str(),
                        "-static", "-w", "-lm", "-O2", "-DONLINE_JUDGE", NULL);
                break;
            case JUDGE_CONF::LANG_JAVA:
                printf("compiler java\n");
                execlp("javac", "javac", PROBLEM::code_path.c_str(), "-d", PROBLEM::run_dir.c_str(), NULL);
        }
        printf("exec error");
        exit(JUDGE_CONF::EXIT_COMPILE);
    }else {
        pid_t w = waitpid(compiler, &status, WUNTRACED);
        if (w == -1) {
            printf("waitpid error\n");
            exit(JUDGE_CONF::EXIT_COMPILE);
        }

        printf("compiler finished\n");
        if (WIFEXITED(status)) {
            if (EXIT_SUCCESS == WEXITSTATUS(status)) {
                printf("compile succeeded.\n");
            }else if (JUDGE_CONF::GCC_COMPILE_ERROR == WEXITSTATUS(status)){
                printf("compile error\n");
                output_result(JUDGE_CONF::CE);
                exit(JUDGE_CONF::EXIT_OK);
            }else {
                printf("compiler unkown exit status %d\n", WEXITSTATUS(status));
                exit(JUDGE_CONF::EXIT_COMPILE);
            }
        }else {
            if (WIFSIGNALED(status)){
                if (SIGALRM == WTERMSIG(status)) {
                    printf("compiler time out\n");
                    output_result(JUDGE_CONF::CE);
                    exit(JUDGE_CONF::EXIT_OK);
                }else {
                    printf("unkown signal\n");
                }
            }else if (WIFSTOPPED(status)){
                printf("stopped by signal\n");
            }else {
                printf("unknown stop reason");
            }
            exit(JUDGE_CONF::EXIT_COMPILE);
        }
    }
}

static
void judge() {
    struct rusage rused;
    pid_t executive = fork();
    if (executive < 0) {
        printf("fork for child failed\n");
        exit(JUDGE_CONF::EXIT_PRE_JUDGE);
    }else if (executive == 0) {
        //log

        io_redirect();

        security_control();

        int real_time_limit = PROBLEM::time_limit;
        if (EXIT_SUCCESS != malarm(ITIMER_REAL, real_time_limit)) {
            printf("malarm failed\n");
            exit(JUDGE_CONF::EXIT_PRE_JUDGE);
        }

        //log
        set_limit();

        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
            exit(JUDGE_CONF::EXIT_PRE_JUDGE_PTRACE);
        }

        if (PROBLEM::lang != JUDGE_CONF::LANG_JAVA){
            execl("./a.out", "a.out", NULL);
        }else {
            execlp("java", "java", "Main", NULL);
        }

        exit(JUDGE_CONF::EXIT_PRE_JUDGE_EXECLP);
    }else {
        int status = 0;
        int syscall_id = 0;
        struct user_regs_struct regs;

        init_RF_table(PROBLEM::lang);

        while (true) {
            if (wait4(executive, &status, 0, &rused) < 0) {
                printf("wait4 failed\n");
                exit(JUDGE_CONF::EXIT_JUDGE);
            }

            if (WIFEXITED(status)){
                if (PROBLEM::lang != JUDGE_CONF::LANG_JAVA ||
                    WEXITSTATUS(status) == EXIT_SUCCESS) {
                    printf("normal quit\n");

                    PROBLEM::result = JUDGE_CONF::PROCEED;
                }else {
                    printf("abnormal quit\n");
                    PROBLEM::result = JUDGE_CONF::RE;
                }
                break;
            }

            if (WIFSIGNALED(status) ||
                (WIFSTOPPED(status) && WSTOPSIG(status) != SIGTRAP)) {
                int signo = 0;
                if (WIFSIGNALED(status)) {
                    signo = WTERMSIG(status);
                    printf("child signaled by %d : %s\n", signo, strsignal(signo));
                }else {
                    signo = WSTOPSIG(status);
                    printf("child stop by %d : %s\n", signo, strsignal(signo));
                }

                switch (signo) {
                    //TLE
                    case SIGALRM:
                    case SIGXCPU:
                    case SIGVTALRM:
                    case SIGKILL:
                        printf("time limit exeeded\n");
                        PROBLEM::result = JUDGE_CONF::TLE;
                        break;
                    case SIGXFSZ:
                        printf("file limit exceeded");
                        PROBLEM::result = JUDGE_CONF::OLE;
                        break;
                    case SIGSEGV:
                    case SIGFPE:
                    case SIGBUS:
                    case SIGABRT:
                        PROBLEM::result = JUDGE_CONF::RE;
                        break;
                    default:
                        PROBLEM::result = JUDGE_CONF::RE;
                        break;
                }

                ptrace(PTRACE_KILL, executive, NULL, NULL);
                break;
            }

            //MLE
            PROBLEM::memory_usage = std::max(PROBLEM::memory_usage,
                    rused.ru_minflt * (getpagesize() / JUDGE_CONF::KILO));

            if (PROBLEM::memory_usage > PROBLEM::memory_limit) {
                PROBLEM::result = JUDGE_CONF::MLE;
                printf("MLE\n");
                ptrace(PTRACE_KILL, executive, NULL, NULL);
                break;
            }

            if (ptrace(PTRACE_GETREGS, executive, NULL, &regs) < 0) {
                printf("ptrace PTRACE_GETREGS failed\n");
                exit(JUDGE_CONF::EXIT_JUDGE);
            }

#ifdef __i386__
            syscall_id = regs.orig_eax;
#else
            syscall_id = regs.orig_rax;
#endif

            if (syscall_id > 0 &&
                !is_valid_syscall(PROBLEM::lang, syscall_id, executive, regs)) {
                printf("restricted fuction %d\n", syscall_id);
                if (syscall_id == SYS_rt_sigprocmask){
                    printf("glibc failed\n");
                }else{
                    printf("%d\n", SYS_write);
                    printf("rf table\n");
                }
                PROBLEM::result = JUDGE_CONF::RE;
                ptrace(PTRACE_KILL, executive, NULL, NULL);
                break;
            }

            if (ptrace(PTRACE_SYSCALL, executive, NULL, NULL) < 0) {
                printf("ptrace PTRACE_SYSCALL failed\n");
                exit(JUDGE_CONF::EXIT_JUDGE);
            }
        }
    }

    PROBLEM::time_usage += (rused.ru_utime.tv_sec * 1000 +
                            rused.ru_utime.tv_usec / 1000);
    PROBLEM::time_usage += (rused.ru_stime.tv_sec * 1000 +
                            rused.ru_stime.tv_usec / 1000);

}

int main(int argc, char *argv[]) {

    if (geteuid() != 0) {
        printf("must run as root\n");
        exit(JUDGE_CONF::EXIT_UNPRIVILEGED);
    }

    parse_arguments(argc, argv);

    JUDGE_CONF::JUDGE_TIME_LIMIT += PROBLEM::time_limit;

    if (EXIT_SUCCESS != malarm(ITIMER_REAL, JUDGE_CONF::JUDGE_TIME_LIMIT)) {
        printf("set alarm for judge failed, %d: %s\n", errno, strerror(errno));
        exit(JUDGE_CONF::EXIT_VERY_FIRST);
    }
    signal(SIGALRM, timeout);

    compiler_source_code();

    judge();

    output_result(PROBLEM::result);

    return 0;
}