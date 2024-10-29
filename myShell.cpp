#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
//模拟linux shell
using namespace std;
class Shell {
public:
    struct ProcessControlBlock {
        pid_t pid;
        int status; // 1 for exited, 2 for running, 3 for terminating
        int exitCode;
        //fd,etc
    };

private:
    static vector<ProcessControlBlock> pcb;
    static const int MAX_PROCESSES = 50;

public:
    Shell() {
        //注册SIGCHLD的处理函数，SIGCHLD是子进程终结发出的信号
        std::signal(SIGCHLD, Shell::sigchld_handler);
    }

    ~Shell() {
        for (auto& process : pcb) {
            if (process.status == 2) {  // Still running
                cout << "Killing [" << process.pid << "]"<<endl;
                //kill(pid,sig) sig一般是sigaction(2)
                //sigterm sigkill区别：15，进程可以决定如何处置信号
                //9:立刻停止，无法释放资源
                kill(process.pid, SIGTERM);
            }
        }
        delete pcb;
         cout<< "\nGoodbye\n"<<endl;
    }
    //处理sigchld或其他的时候，经常用waitpid()获取子状态信息
    //exit(): 正常；sigkill,kill9:硬终止；
    static void sigchld_handler(int s) {
        int status;
        pid_t cp;
        //-1:等任何子
        //wnohang:非阻塞检查子进程
        //wuntraced:阻塞，适用于严格同步
        while ((cp = waitpid(-1, &status, WNOHANG)) > 0) {
            cout<<"terminating "<<cp<<endl;
            for(int i =0;i < pcb.size();i++){
                if(pcb[i].pid==cp){
                    pcb[i].status=1;
                    //正常结束
                    if(WIFEXITED(status)){
                        //WEXITSTATUS 是一个宏，用于从子进程的正常终止状态中提取出退出状态码。
                        pcb[i].exitCode = WEXITSTATUS(status); // 更新该进程的退出码
                    } else if (WIFSIGNALED(status)){
                        //非0，异常终止
                        //用于从子进程的异常终止状态中提取出退出状态码。
                        pcb[i].exitCode = WTERMSIG(status);
                    }
                    break;
                }
            }
        }
    }

    void process_command(const std::vector<std::string>& tokens) {
        if (tokens.empty()) return;
        if (tokens.size() < 2) {
            std::cerr << "Usage: info <option>"<<endl;
            return;
        }
        if (tokens[0] == "info") {//info 0
            handle_info(tokens);
        } else if(tokens[0]=="terminate"){
            long cp = stol(tokens[1]);
            terminate(cp);
        }else if(tokens[0]=="wait"){
            long cp = stol(tokens[1]);
            waitCP(cp);
        }else{

            bool ind = false;
            if(tokens[tokens.size()-1]=="&")ind = true;

            pid_t cp = fork();


            if(cp==0){
                //子进程，

                //用于用另一个程序替换当前进程的程序映像
                // execv(path/to/exe,char* const argv[]);POSIX API
                //多个子进程可以同时执行同一个可执行文件，因为写拷贝，他们数据栈独立，，
                //子线程共享全局变量和堆，没有写拷贝，，

                execv(tokens[0], argv);
            }else {//父进程
                ProcessControlBlock* p = new struct ProcessControlBlock();
                p->pid = cp;
                p->status = 2;
                pcb.push_back(*p);
                if(ind){
                    //结尾有&，父进程不阻塞等待子进程完成命令
                    //父进程为前台进程，子进程为后台进程
                    //同一时间多个后台进程和他同时运行
                    cout<<"child in back"<<endl;
                }else {
                    //无&，前台进程（父进程）阻塞，等待子进程完成命令
                    //两个都是前台进程
                    //同时执行多个前台进程的命令，父进程会阻塞，不会读取下一个命令，因此同一时间只有一个子进程和他同时运行
                    int stat;
                    waitpid(cp,&stat,WUNTRACED);
                    pcb[pcb.size()-1].status = 1;
                    if(WIFEXITED(stat)){
                        pcb[pcb.size()-1].exitCode = WEXITSTATUS(stat);
                    } else if (WIFSIGNALED(stat)){
                        pcb[pcb.size()-1].exitCode = WTERMSIG(stat);
                    }
                }
            }
        }
    }
    void waitCP(pid_t cp){
        int status;
        for(int i = 0;i<pcb.size();i++){
            if(pcb[i].pid==cp){
                if(pcb[i].status!=2){
                    cout<<"Not a running process"<<endl;
                }else{
                    waitpid(cp,&status,WUNTRACED);
                    if(WIFEXITED(status)){
                        pcb[i].exitCode = WEXITSTATUS(status);
                    }else if(WIFSIGNALED(status)){
                        pcb[i].exitCode=WTERMSIG(status);
                    }
                    pcb[i].status=1;
                    break;

                }

            }
        }

    }
    void terminate(pid_t cp){
        for(int i = 0;i<pcb.size();i++){
            if(pcb[i].pid==cp){
                if(pcb[i].status!=2){
                    cout<<"Not a running proc"<<endl;
                }else {
                    pcb[i].status=3;
                    kill(pcb[i].pid,SIGTERM);//子进程自动发送SIGCHLD
                    break;
                }
            }
        }
    }

private:
    void handle_info(const std::vector<std::string>& tokens) {

        if (tokens[1] == "0") {//{info, 0}各个进程状态
            for (auto& p : pcb) {
                if(p.status==1)cout << "[" << p.pid << "] exited"<< " ExitCode: " << p.exitCode << endl;
                if(p.status==2)cout << "[" << p.pid << "] running"<< endl;
                if(p.status==3)cout << "[" << p.pid << "] terminating"<< endl;
            }
        }
    }

};

int main() {
    Shell myShell;
    std::vector<std::string> commands = {"info", "0"};
    myShell.process_command(commands);
    return 0;
}
