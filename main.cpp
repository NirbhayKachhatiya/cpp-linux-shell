#include <bits/stdc++.h>

#include <cstdlib>

#include <filesystem>

#include <iostream>

#include <string>

#include <unistd.h>

#include <sys/wait.h>

#include <fcntl.h>

namespace fs = std::filesystem;

std::string find_path(std::string & word) {
  std::string found_path = "";
  // extracting the path environment variable
  const char * env_p = std::getenv("PATH");
  if (env_p) {
    // converting to string because env_p can change the path itself , so its safer to make a copy and work with it
    std::string path_env(env_p);
    std::stringstream ss(path_env);
    std::string dir;
    // enumerating each directory in the "PATH" env variable
    while (std::getline(ss, dir, ':')) {
      // adding our input to current directory and checking if that path exists
      fs::path p = fs::path(dir) / word;
      if (fs::exists(p)) {
        // get the list of all permissions for the current modified path
        fs::perms prms = fs::status(p).permissions();
        if ((prms & fs::perms::owner_exec) != fs::perms::none) {
          // we have the execution permissions
          found_path = p.string();
          break;
        }
      }
    }
  }
  return found_path;
}

std::vector < std::string > tokenize(const std::string & input) {
  std::vector < std::string > tokens;
  std::string current;
  size_t i = 0;
  bool in_single = false;
  bool in_double = false;
  while (i < input.size()) {
    char c = input[i];
    if (c == ' ' && !in_single && !in_double) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      ++i;
    } else if (c == '\\' && !in_single) {
      if (i + 1 < input.size()) {
        char next = input[i + 1];
        if (in_double) {
          // In double quotes, only escape " and backslash
          if (next == '"' || next == '\\') {
            current += next;
            i += 2;
          } else {
            current += c;
            ++i;
          }
        } else {
          // Outside quotes, escape next char
          current += next;
          i += 2;
        }
      } else {
        // Trailing backslash, treat literally
        current += c;
        ++i;
      }
    } else if (c == '\'') {
      if (in_double) {
        current += c;
        ++i;
      } else if (in_single) {
        in_single = false;
        ++i;
      } else {
        in_single = true;
        ++i;
      }
    } else if (c == '"') {
      if (in_single) {
        current += c;
        ++i;
      } else if (in_double) {
        in_double = false;
        ++i;
      } else {
        in_double = true;
        ++i;
      }
    } else {
      current += c;
      ++i;
    }
  }
  if (!current.empty()) {
    tokens.push_back(current);
  }
  return tokens;
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::string input;
  fs::path current_path = fs::current_path();
  while (true) {
    std::cout << "$ ";
    if (!std::getline(std::cin, input))
      break;
    if (input == "exit")
      break;
    else if (input.substr(0, 5) == "echo ") {
      // Parse echo arguments, handling quotes and redirection
      auto args = tokenize(input.substr(5));
      
      // Parse for redirection in echo command
      std::vector<std::string> echo_args;
      std::string redirect_file = "";
      int redirect_fd = 1; // 1 for stdout, 2 for stderr
      bool redirect = false;
      bool append_mode = false;
      for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == ">" || args[i] == "1>") {
          // redirect stdout (truncate mode)
          if (i + 1 < args.size()) {
            redirect_file = args[i + 1];
            redirect_fd = 1;
            redirect = true;
            append_mode = false;
            break;
          } else {
            std::cout << "syntax error: expected file after " << args[i] << std::endl;
            continue;
          }
        } else if (args[i] == ">>" || args[i] == "1>>") {
          // redirect stdout (append mode)
          if (i + 1 < args.size()) {
            redirect_file = args[i + 1];
            redirect_fd = 1;
            redirect = true;
            append_mode = true;
            break;
          } else {
            std::cout << "syntax error: expected file after " << args[i] << std::endl;
            continue;
          }
        } else if (args[i] == "2>") {
          // redirect stderr (truncate mode)
          if (i + 1 < args.size()) {
            redirect_file = args[i + 1];
            redirect_fd = 2;
            redirect = true;
            append_mode = false;
            break;
          } else {
            std::cout << "syntax error: expected file after " << args[i] << std::endl;
            continue;
          }
        } else if (args[i] == "2>>") {
          // redirect stderr (append mode)
          if (i + 1 < args.size()) {
            redirect_file = args[i + 1];
            redirect_fd = 2;
            redirect = true;
            append_mode = true;
            break;
          } else {
            std::cout << "syntax error: expected file after " << args[i] << std::endl;
            continue;
          }
        } else {
          echo_args.push_back(args[i]);
        }
      }
      
      // Handle redirection for echo
      int saved_fd = -1;
      if (redirect) {
        // open the file for writing, create if doesn't exist
        // O_WRONLY: open for writing only
        // O_CREAT: create the file if it doesn't exist
        // O_TRUNC: truncate the file to zero length if it exists (used with >)
        // O_APPEND: append to the end of file (used with >>)
        // 0644: permissions for the file (rw-r--r--)
        int flags = O_WRONLY | O_CREAT;
        if (append_mode) {
          flags |= O_APPEND;
        } else {
          flags |= O_TRUNC;
        }
        int fd = open(redirect_file.c_str(), flags, 0644);
        if (fd == -1) {
          std::cerr << "failed to open file: " << redirect_file << std::endl;
          continue;
        }
        // dup the current file descriptor to save it for later restoration
        int target_fd = (redirect_fd == 1) ? STDOUT_FILENO : STDERR_FILENO;
        saved_fd = dup(target_fd);
        // dup2 duplicates the file descriptor fd to the target fd, closing the old one
        // now writes to the target fd will go to the file
        dup2(fd, target_fd);
        close(fd); // close the original fd since target now points to it
      }
      
      // Print the echo arguments using write to ensure it goes to the correct fd
      std::string output;
      for (size_t i = 0; i < echo_args.size(); ++i) {
        if (i > 0) output += " ";
        output += echo_args[i];
      }
      output += "\n";
      // write directly to STDOUT_FILENO, which may be redirected
      write(STDOUT_FILENO, output.c_str(), output.size());
      
      // Restore the file descriptor if redirected
      if (redirect && saved_fd != -1) {
        int target_fd = (redirect_fd == 1) ? STDOUT_FILENO : STDERR_FILENO;
        // dup2 the saved fd back to the target fd
        dup2(saved_fd, target_fd);
        close(saved_fd); // close the duplicate
      }
    } else if (input.substr(0, 5) == "type ") {
      // Parse type command argument, handling quotes
      auto args = tokenize(input.substr(5));
      if (args.size() != 1) {
        std::cout << "type: expected 1 argument" << std::endl;
        continue;
      }
      std::string word = args[0];
      std::vector < std::string > valid = {
        "echo",
        "exit",
        "type",
        "pwd"
      };
      bool val = 0;
      for (auto cmd: valid) {
        if (word == cmd) {
          val = 1;
          break;
        }
      }
      if (val) {
        std::cout << word << " is a shell builtin" << std::endl;
      } else {
        std::string found_path = find_path(word);
        if (!found_path.empty()) {
          std::cout << word << " is " << found_path << std::endl;
        } else {
          std::cout << word << ": not found" << std::endl;
        }
      }
    } else if (input == "pwd") {
      std::cout << current_path.string() << std::endl;
    } else if (input.substr(0, 3) == "cd ") {
      // Parse cd command argument, handling quotes
      auto args = tokenize(input.substr(3));
      if (args.size() != 1) {
        std::cout << "cd: expected 1 argument" << std::endl;
        continue;
      }
      std::string newPath = args[0];
      // the path is absolute
      if (newPath[0] == '/') {
        if (fs::exists(newPath)) {
          fs::path path = newPath;
          current_path = path;
        } else {
          std::cout << "cd: " << newPath << ": No such file or directory" << std::endl;
        }
      }
      // the path is relative
      else {
        std::stringstream ss(newPath);
        fs::path tempPath = current_path;
        std::string folder;
        bool change = true;
        int index = 0;
        while (getline(ss, folder, '/')) {
          if (folder == ".")
            continue;
          else if (folder == "..") {
            tempPath = tempPath.parent_path();
          } else if ((folder == "~") && (index == 0)) {
            tempPath = fs::path(std::getenv("HOME"));
          } else {
            tempPath = tempPath / folder;
            if (!fs::exists(tempPath)) {
              std::cout << "cd: " << newPath << ": No such file or directory" << std::endl;
              change = false;
              break;
            }
          }
          index = 1;
        }
        if (change) {
          current_path = tempPath;
        }
      }
    } else {
      // Parse external command arguments, handling quotes
      auto args = tokenize(input);
      if (args.empty())
        continue;
      
      // Parse for redirection - we need to separate the command from the redirection operator and file
      std::vector<std::string> cmd_args;
      std::string redirect_file = "";
      int redirect_fd = 1; // 1 for stdout, 2 for stderr
      bool redirect = false;
      bool append_mode = false;
      for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == ">" || args[i] == "1>") {
          // redirect stdout (truncate mode)
          if (i + 1 < args.size()) {
            redirect_file = args[i + 1];
            redirect_fd = 1;
            redirect = true;
            append_mode = false;
            break; // stop parsing args, rest are ignored after redirection
          } else {
            std::cout << "syntax error: expected file after " << args[i] << std::endl;
            continue;
          }
        } else if (args[i] == ">>" || args[i] == "1>>") {
          // redirect stdout (append mode)
          if (i + 1 < args.size()) {
            redirect_file = args[i + 1];
            redirect_fd = 1;
            redirect = true;
            append_mode = true;
            break; // stop parsing args, rest are ignored after redirection
          } else {
            std::cout << "syntax error: expected file after " << args[i] << std::endl;
            continue;
          }
        } else if (args[i] == "2>") {
          // redirect stderr (truncate mode)
          if (i + 1 < args.size()) {
            redirect_file = args[i + 1];
            redirect_fd = 2;
            redirect = true;
            append_mode = false;
            break; // stop parsing args, rest are ignored after redirection
          } else {
            std::cout << "syntax error: expected file after " << args[i] << std::endl;
            continue;
          }
        } else if (args[i] == "2>>") {
          // redirect stderr (append mode)
          if (i + 1 < args.size()) {
            redirect_file = args[i + 1];
            redirect_fd = 2;
            redirect = true;
            append_mode = true;
            break; // stop parsing args, rest are ignored after redirection
          } else {
            std::cout << "syntax error: expected file after " << args[i] << std::endl;
            continue;
          }
        } else {
          cmd_args.push_back(args[i]);
        }
      }
      
      if (cmd_args.empty()) {
        std::cout << "syntax error: no command before redirection" << std::endl;
        continue;
      }
      
      std::string found_path = find_path(cmd_args[0]);
      if (found_path.size() == 0)
        std::cout << cmd_args[0] << ": command not found" << std::endl;
      else {
        // create a child process (to execute the command)
        pid_t pid = fork();
        // child process
        if (pid == 0) {
          if (redirect) {
            // open the file for writing, create if doesn't exist
            // O_WRONLY: open for writing only
            // O_CREAT: create the file if it doesn't exist
            // O_TRUNC: truncate the file to zero length if it exists (used with > and 1>)
            // O_APPEND: append to the end of file (used with >> and 1>>)
            int flags = O_WRONLY | O_CREAT;
            if (append_mode) {
              flags |= O_APPEND;
            } else {
              flags |= O_TRUNC;
            }
            int fd = open(redirect_file.c_str(), flags, 0644);
            if (fd == -1) {
              std::cerr << "failed to open file: " << redirect_file << std::endl;
              exit(1);
            }
            // redirect the appropriate file descriptor to the file
            int target_fd = (redirect_fd == 1) ? STDOUT_FILENO : STDERR_FILENO;
            dup2(fd, target_fd);
            close(fd); // close the original fd since target now points to it
          }
          // converting vector of string to vector of char*
          std::vector < char * > c_args;
          for (auto & s: cmd_args) {
            // c_str() return type is const_char* , so here we are converting it to char* using const_cast
            c_args.push_back(const_cast < char * > (s.c_str()));
          }
          // required to end the vector of char* with nullptr (to use in execv function)
          c_args.push_back(nullptr);
          //.data() return pointer to c_args
          execv(found_path.c_str(), c_args.data());
          exit(1);
        }
        // parent process
        else if (pid > 0) {
          int status;
          waitpid(pid, & status, 0);
        }
      }
    }
  }
  return 0;
}