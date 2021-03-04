#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>

int historyCommand (std::vector<std::string>& command_list, std::vector<std::string>& command_history);
void displayHistory(std::vector<std::string>& command_history, int num_of_latest_entries);
int execute_command(std::vector<std::string>& command_list, std::vector<std::string>& os_path_list);

void splitString(std::string text, char d, std::vector<std::string>& result);
void vectorOfStringsToArrayOfCharArrays(std::vector<std::string>& list, char ***result);
void freeArrayOfCharArrays(char **array, size_t array_length);

int main(int argc, char **argv)
{
    //Get list of paths to binary executables
    std::vector<std::string> os_path_list;
    char* os_path = getenv("PATH");
    splitString(os_path, ':', os_path_list);

    //Welcome message
    printf("Welcome to OSShell! Please enter your commands ('exit' to quit).\n");

    std::vector<std::string> command_list;      //Stores the commands the user types in, split into its variour parameters
    std::vector<std::string> command_history;   //Stores each whole line the user enters

    bool exit_program = false;
    do {
        std::string user_input;
        //While user input is empty (meaning the user only hit enter), do not print a specific error, just re-display "osshell> ".
        do {    
            printf("osshell> ");
            getline(std::cin, user_input);
        } while(user_input.empty());

        command_history.push_back(user_input);          //Add the new user input to the command history.
        splitString(user_input, ' ', command_list);     //Split the user input up into its command and the parameters for this command.

        //If the user entered "exit" as the command, then exit the program.
        if(command_list[0] == "exit") {
            exit_program = true;
        //If the user entered "history" as the command, then run the historyCommand function.
        } else if(command_list[0] == "history") {
            //If the history command returned -1 then it failed to execute so print an error.
            if(historyCommand(command_list, command_history) == -1) {
                printf("Error: history expects an integer > 0 (or 'clear')\n");
            }
        //If the user entered anything else, then search for a command with that name in all of the environment variable paths.
        } else {
            //If execute_program returns -1, then the child process could not find a command to execute so the child process should end.
            if(execute_command(command_list, os_path_list) == -1) {
                exit_program = true;
            }
        }
    } while(!exit_program);
    return 0;
}


/* Executes the history command with the corresponding arguments if they are formatted correctly.
 * command_list: list of a command and its arguments.
 * command_history: list of previous commands.
 * Returns -1 if the history command was improperly formatted and could not be executed.
 * Returns 0 if the command executed successfully.
 */
int historyCommand(std::vector<std::string>& command_list, std::vector<std::string>& command_history) {
    //If the history command has some sort of argument, then try to check that argument.
    if(command_list.size() > 1) {
        //If something was entered after the first argument, then the command is invalid.
        if(command_list.size() > 2) {
            return -1;
        }
        //If the argument is "clear" then clear the history. Otherwise the argument is expected to be an integer.
        if(command_list[1] == "clear") {
            command_history.clear();
        } else {
            //Manually check to make sure every character in the first argument is an integer.
            for(int i=0; i<command_list[1].size(); i++) {
                if(command_list[1][i] < 48 || command_list[1][i] > 57) {
                    return -1;
                }
            }
            //If every character is an integer, then the argument can be converted to an int.
            int num_of_latest_entries;
            num_of_latest_entries = std::stoi(command_list[1]);

            //If the integer is outside the bounds of the number of history entries, then the command is invalid.
            if(num_of_latest_entries < 1 || num_of_latest_entries > command_history.size()-1) {
                return -1;
            }
            //If the function has not returned by this point, then the history can be displayed to the specified number of entries.
            displayHistory(command_history, num_of_latest_entries+1);
        }
    } else {
        displayHistory(command_history, command_history.size());    //Displays all history entries.
    }
    return 0;
}
void displayHistory(std::vector<std::string>& command_history, int num_of_latest_entries) {
    for(int i=command_history.size()-num_of_latest_entries; i<command_history.size()-1; i++) {
        std::cout << "  " << i+1 << ": " << command_history[i] << "\n";
    }
}

/* Attempts to execute the given command in command list. Searches all location in os_path_list for a corrisponding command.
 * command_list: list of a command and its arguments.
 * os_path_list: list of "PATH" paths.
 * Returns -1 if the child process could not find a command to execute.
 * Returns 0 after the parent process is done waiting for the child.
 */
int execute_command(std::vector<std::string>& command_list, std::vector<std::string>& os_path_list) {
    char **command_list_exec;       //command_list converted to an array of character arrays
    vectorOfStringsToArrayOfCharArrays(command_list, &command_list_exec);   //Turn the command and arguments into their C string version.
    int pid = fork();
    //If Process ID is 0 then this process is the child so it should try to run the command with exec.
    if(pid == 0) {
        //Try to run exec on each potential path until all paths are exausted.
        int i = 0;
        do {
        //Converts each path obtained from the environment variable into one that includes the command that is attempting to run. Assigns this new path to full_path.
            std::string full_path = os_path_list[i];
            full_path.append("/");
            full_path.append(command_list[0]);
        //Converts the full_path string into a C string.
            char* full_path_c_str = new char [os_path_list[i].size()+command_list[0].size()+1];
            std::strcpy(full_path_c_str, full_path.c_str());
        //Tries to run the command at the created path. If there is no command at this path then the exec will fail and the program will continue until all environment variable paths have been tried.
            execv(full_path_c_str, command_list_exec);
            i++;
        } while(i < os_path_list.size());
        std::cout << command_list[0] << ": Error command not found\n";    //If the program has made it to this point then exec was never able to run so the command does not exist.
        freeArrayOfCharArrays(command_list_exec, command_list.size() + 1);
        return -1;    //The child proccess should now end because no command was found to replace its process with.

    //If the Proccess ID is not 0 then this process is the parent so it should wait for the child process to end.
    } else {        
        waitpid(pid, nullptr, 0);
    }
    freeArrayOfCharArrays(command_list_exec, command_list.size() + 1);
    return 0;
}


/*
   text: string to split
   d: character delimiter to split `text` on
   result: vector of strings - result will be stored here
*/
void splitString(std::string text, char d, std::vector<std::string>& result)
{
    enum states { NONE, IN_WORD, IN_STRING } state = NONE;

    int i;
    std::string token;
    result.clear();
    for (i = 0; i < text.length(); i++)
    {
        char c = text[i];
        switch (state) {
            case NONE:
                if (c != d)
                {
                    if (c == '\"')
                    {
                        state = IN_STRING;
                        token = "";
                    }
                    else
                    {
                        state = IN_WORD;
                        token = c;
                    }
                }
                break;
            case IN_WORD:
                if (c == d)
                {
                    result.push_back(token);
                    state = NONE;
                }
                else
                {
                    token += c;
                }
                break;
            case IN_STRING:
                if (c == '\"')
                {
                    result.push_back(token);
                    state = NONE;
                }
                else
                {
                    token += c;
                }
                break;
        }
    }
    if (state != NONE)
    {
        result.push_back(token);
    }
}


/*
   list: vector of strings to convert to an array of character arrays
   result: pointer to an array of character arrays when the vector of strings is copied to
*/
void vectorOfStringsToArrayOfCharArrays(std::vector<std::string>& list, char ***result)
{
    int i;
    int result_length = list.size() + 1;
    *result = new char*[result_length];
    for (i = 0; i < list.size(); i++)
    {
        (*result)[i] = new char[list[i].length() + 1];
        strcpy((*result)[i], list[i].c_str());
    }
    (*result)[list.size()] = NULL;
}


/*
   array: list of strings (array of character arrays) to be freed
   array_length: number of strings in the list to free
*/
void freeArrayOfCharArrays(char **array, size_t array_length)
{
    int i;
    for (i = 0; i < array_length; i++)
    {
        if (array[i] != NULL)
        {
            delete[] array[i];
        }
    }
    delete[] array;
}
