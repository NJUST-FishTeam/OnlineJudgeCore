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
#include "core.h"

static
bool has_suffix(const std::string &str, const std::string &suffix)
{
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static
void parse_arguments(int argc, char ** argv) {
    PROBLEM::code_path    = argv[1];
    PROBLEM::time_limit   = atoi(argv[2]);
    PROBLEM::memory_limit = atoi(argv[3]);
    PROBLEM::input_path   = argv[4]
    PROBLEM::output_path  = argv[5];
    PROBLEM::result_path  = argv[6];

    if (has_suffix(PROBLEM::code_path, ".cpp")){
        PROBLEM::lang = JUDGE_CONF::LANG_CPP;
    }else if (has_suffix(PROBLEM::code_path, ".c")){
        PROBLEM::lang = JUDGE_CONF::LANG_C;
    }else if (has_suffix(PROBLEM::code_path, ".java")){
        PROBLEM::lang = JUDGE_CONF::LANG_JAVA;
    }else{
        //print unkwon language
        exit(JUDGE_CONF::EXIT_BAD_PARAM);
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
    stdin = freopen(PROBLEM::input_path.c_str(), "r", stdin);
    stdout = freopen(PROBLEM::output_path.c_str(), "w", stdout);
    stderr = freopen("/dev/null", "w", stderr);

    if (stdin == NULL || stdout == NULL || stderr == NULL) {
        //freopen error
        exit(JUDGE_CONF::EXIT_PRE_JUDGE);
    }
}
