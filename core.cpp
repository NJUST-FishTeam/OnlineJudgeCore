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

extern int errno;

static
bool has_suffix(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static
void parse_arguments(int argc, char** argv) {
    int opt;
    extern char *optarg;

    while ((opt = getopt(argc, argv, "c:t:m:s:S:d")) != -1) {
        switch (opt) {
            case 'c': PROBLEM::code_path    = optarg;           break;
            case 't': PROBLEM::time_limit   = atoi(optarg);     break;
            case 'm': PROBLEM::memory_limit = atoi(optarg);     break;
            case 's': PROBLEM::spj          = true;             break;
            case 'S': PROBLEM::spj_code_file= optarg;           break;
            case 'd': PROBLEM::run_dir      = optarg;           break;
            default:
                printf("unkown option\n");
                exit(JUDGE_CONF::EXIT_BAD_PARAM);
        }
    }
    //PROBLEM::code_path    = argv[1];
    //PROBLEM::time_limit   = atoi(argv[2]);
    //PROBLEM::memory_limit = atoi(argv[3]);
    //PROBLEM::input_path   = argv[4];
    //PROBLEM::output_path  = argv[5];
    //PROBLEM::result_path  = argv[6];

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

int main(int argc, char *argv[]) {
    return 0;
}
