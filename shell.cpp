#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vector>
#include <string>
#include <string.h>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    char buf[256];
    string prev_dir = getcwd(buf, 256);  // variable storing previous working directory for "cd -"
    vector<pid_t> PIDS;  // vector storing pids of background processes
    int in = dup(0);  // duplicate stdin
    int out = dup(1);  // duplicate stdout

    for (;;) {
        // need date/time, username, and absolute path to current dir
        time_t _time = time(NULL);
        string curr_time = ctime(&_time);
        curr_time.erase(curr_time.find('\n',0), 1);
        
        // user prompt
        char* dir_name = getcwd(buf, 256);
        cout << YELLOW << curr_time << " " << getenv("USER") << ":" << dir_name << NC << "$ ";
        
        // get user inputted command
        string input;
        getline(cin, input);

        // TODO: implement iteration over vector of bg pid (vector declared outside scope)
        // waitpid() - using flag for non-blocking
        int status;
        for (size_t i = 0; i < PIDS.size(); i++)
        {
            if ((status = waitpid(PIDS[i], 0, WNOHANG)) > 0)
            {
                cout << "PID killed: " << *(PIDS.begin() + i) << endl;
                PIDS.erase(PIDS.begin() + i);
                i--;
            }
        }

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }

        

        // print out every command token-by-token on individual lines
        // prints to cerr to avoid influencing autograder
        // for (auto cmd : tknr.commands) {
        //     for (auto str : cmd->args) {
        //         cerr << "|" << str << "| ";
        //     }
        //     if (cmd->hasInput()) {
        //         cerr << "in< " << cmd->in_file << " ";
        //     }
        //     if (cmd->hasOutput()) {
        //         cerr << "out> " << cmd->out_file << " ";
        //     }
        //     cerr << endl;
        // }

        // chdir()
        // if dir (cd <dir>) is "-", then go to previous working directory
        // variable storing previous working directory (it needs to be declared outside loop)
        if (tknr.commands.size() == 1 && tknr.commands[0]->args[0] == "cd")
        {   
            if (tknr.commands[0]->args[1] == "-")
            {
                cout << prev_dir << endl;
                chdir(prev_dir.c_str());
            }
            else
            {
                prev_dir = dir_name;
                chdir(tknr.commands[0]->args[1].c_str());  // change directory to what's following chdir
            }
        }
        else  // not a cd command
        {
            for (size_t j = 0; j < tknr.commands.size(); j++) // loop through the piped commands
            {
                // for piping
                // for cmd : tknr.commands
                //      create pipe()
                //      fork() -> in child, redirect stdout; in parent redirect stdin
                // add checks for first or last commands; first command don't redirect stdin last command don't redirect stdout
                // outside of loop, restore stdin and stdout

                int fds[2];  // pipe's file descriptors
                if (pipe(fds) < 0)  // create pipe
                {
                    perror("pipe");
                    exit(2);
                }

                // fork to create child
                pid_t pid = fork();
                if (pid < 0) {  // error check
                    perror("fork");
                    exit(2);
                }

                if (pid == 0) {  // if child, exec to run command
                    // run single commands with no arguments
                    //char* args[] = {(char*) tknr.commands.at(0)->args.at(0).c_str(), nullptr};

                    //TODO implement multiple arguments - iterate over args of current command to make char* array
                    char** cmds = new char*[tknr.commands[j]->args.size() + 1];
                    for (size_t i = 0; i < tknr.commands[j]->args.size(); i++)
                    {
                        cmds[i] = ((char*)tknr.commands[j]->args[i].c_str());
                    }
                    cmds[tknr.commands[j]->args.size()] = nullptr;

                    // redirect stdout to write and close read side pipe
                    if (j < tknr.commands.size() - 1)
                    {
                        dup2(fds[1], 1);  // redirect stdout to write
                        close(fds[0]);
                    }

                    // TODO if current command is redirected, then open file and dup2 stdin or stdout
                    // implement it for both safely at the same time
                        if (tknr.commands[j]->hasOutput())
                        {
                            int fd1 = open(tknr.commands[j]->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);  // output file

                            if (fd1 == -1)
                            {
                                perror("open output");
                                exit(1);
                            }

                            // Redirect output
                            if (dup2(fd1, STDOUT_FILENO) == -1)
                            {
                                perror("dup2");
                                exit(1);
                            }
                        }

                    // Input redirection
                    
                    if (tknr.commands[j]->hasInput()) 
                    {
                        int fd2 = open(tknr.commands[j]->in_file.c_str(), O_RDONLY);  // input file

                        if (fd2 == -1)
                        {
                            perror("open input");
                            exit(1);
                        }

                        if (dup2(fd2, STDIN_FILENO) == -1)
                        {
                            perror("dup2");
                            exit(1);
                        }
                    }
                    
                    if (execvp(cmds[0], cmds) < 0) {  // error check
                        perror("execvp");
                        exit(2);
                    }
                    delete [] cmds;
                }
                else {  // if parent, wait for child to finish
                    int status = 0;
                    bool bg = false;

                    if (j < tknr.commands.size() - 1)
                    {
                        dup2(fds[0], 0);  // redirect stdin to read
                        close(fds[1]);
                    }

                    //TODO: add check for bg process - add pid to vector if bg and dont waitpid() in parent
                    for (size_t i = 0; i < tknr.commands.size(); i++)
                    {
                        if (tknr.commands[i]->isBackground())  // dont waitpid()
                        {
                            bg = true;
                            PIDS.push_back(pid);
                        }
                    }

                    if (!bg) // if no background process, execute waitpid
                    {
                        waitpid(pid, &status, 0);
                        if (status > 1) {  // exit if child didn't exec properly
                            exit(status);
                        }                    
                    }
                }
            }
            // restore stdin and stdout
            dup2(in, 0);
            dup2(out, 1);
        }       
    }
}
