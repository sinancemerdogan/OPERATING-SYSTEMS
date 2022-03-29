#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
const char *sysname = "shellfyre";

//Declaration of recursive fileSearch function
void fileSearch(char *keyword, char *current_dir, int recursive, int open);

//Declaration of cdh command helper functions
void printCdHistory(char *cdHistory[]);
void addCdToHistory(char *cd);
void writeToCdhFile();
void readFromCdhFile();

//Fixed sized string array (list) for keeping directory history
char *cdHistory[10];
//Index for reaching elements of the history list
int cdCount = 0;
//Path to directory in which shellfyre exist
char pathToShellfyre[512];

enum return_codes
{
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};

struct command_t
{
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3];		// in/out redirection
	struct command_t *next; // for piping
};

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command)
{
	int i = 0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background ? "yes" : "no");
	printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
	printf("\tRedirects:\n");
	for (i = 0; i < 3; i++)
		printf("\t\t%d: %s\n", i, command->redirects[i] ? command->redirects[i] : "N/A");
	printf("\tArguments (%d):\n", command->arg_count);
	for (i = 0; i < command->arg_count; ++i)
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	if (command->next)
	{
		printf("\tPiped to:\n");
		print_command(command->next);
	}
}

/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
	if (command->arg_count)
	{
		for (int i = 0; i < command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}
	for (int i = 0; i < 3; ++i)
		if (command->redirects[i])
			free(command->redirects[i]);
	if (command->next)
	{
		free_command(command->next);
		command->next = NULL;
	}
	free(command->name);
	free(command);
	return 0;
}

/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
	char cwd[1024], hostname[1024];
	gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}

/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
	const char *splitters = " \t"; // split at whitespace
	int index, len;
	len = strlen(buf);
	while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
	{
		buf++;
		len--;
	}
	while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
		buf[--len] = 0; // trim right whitespace

	if (len > 0 && buf[len - 1] == '?') // auto-complete
		command->auto_complete = true;
	if (len > 0 && buf[len - 1] == '&') // background
		command->background = true;

	char *pch = strtok(buf, splitters);
	command->name = (char *)malloc(strlen(pch) + 1);
	if (pch == NULL)
		command->name[0] = 0;
	else
		strcpy(command->name, pch);

	command->args = (char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index = 0;
	char temp_buf[1024], *arg;

	while (1)
	{
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch)
			break;
		arg = temp_buf;
		strcpy(arg, pch);
		len = strlen(arg);

		if (len == 0)
			continue;										 // empty arg, go for next
		while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
		{
			arg++;
			len--;
		}
		while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
			arg[--len] = 0; // trim right whitespace
		if (len == 0)
			continue; // empty arg, go for next

		// piping to another command
		if (strcmp(arg, "|") == 0)
		{
			struct command_t *c = malloc(sizeof(struct command_t));
			int l = strlen(pch);
			pch[l] = splitters[0]; // restore strtok termination
			index = 1;
			while (pch[index] == ' ' || pch[index] == '\t')
				index++; // skip whitespaces

			parse_command(pch + index, c);
			pch[l] = 0; // put back strtok termination
			command->next = c;
			continue;
		}

		// background process
		if (strcmp(arg, "&") == 0)
			continue; // handled before

		// handle input redirection
		redirect_index = -1;
		if (arg[0] == '<')
			redirect_index = 0;
		if (arg[0] == '>')
		{
			if (len > 1 && arg[1] == '>')
			{
				redirect_index = 2;
				arg++;
				len--;
			}
			else
				redirect_index = 1;
		}
		if (redirect_index != -1)
		{
			command->redirects[redirect_index] = malloc(len);
			strcpy(command->redirects[redirect_index], arg + 1);
			continue;
		}

		// normal arguments
		if (len > 2 && ((arg[0] == '"' && arg[len - 1] == '"') || (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
		{
			arg[--len] = 0;
			arg++;
		}
		command->args = (char **)realloc(command->args, sizeof(char *) * (arg_index + 1));
		command->args[arg_index] = (char *)malloc(len + 1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count = arg_index;
	return 0;
}

void prompt_backspace()
{
	putchar(8);	  // go back 1
	putchar(' '); // write empty over
	putchar(8);	  // go back 1 again
}

/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{
	int index = 0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

	// tcgetattr gets the parameters of the current terminal
	// STDIN_FILENO will tell tcgetattr that it should write the settings
	// of stdin to oldt
	static struct termios backup_termios, new_termios;
	tcgetattr(STDIN_FILENO, &backup_termios);
	new_termios = backup_termios;
	// ICANON normally takes care that one line at a time will be processed
	// that means it will return if it sees a "\n" or an EOF or an EOL
	new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
	// Those new settings will be set to STDIN
	// TCSANOW tells tcsetattr to change attributes immediately.
	tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

	// FIXME: backspace is applied before printing chars
	show_prompt();
	int multicode_state = 0;
	buf[0] = 0;

	while (1)
	{
		c = getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		if (c == 9) // handle tab
		{
			buf[index++] = '?'; // autocomplete
			break;
		}

		if (c == 127) // handle backspace
		{
			if (index > 0)
			{
				prompt_backspace();
				index--;
			}
			continue;
		}
		if (c == 27 && multicode_state == 0) // handle multi-code keys
		{
			multicode_state = 1;
			continue;
		}
		if (c == 91 && multicode_state == 1)
		{
			multicode_state = 2;
			continue;
		}
		if (c == 65 && multicode_state == 2) // up arrow
		{
			int i;
			while (index > 0)
			{
				prompt_backspace();
				index--;
			}
			for (i = 0; oldbuf[i]; ++i)
			{
				putchar(oldbuf[i]);
				buf[i] = oldbuf[i];
			}
			index = i;
			continue;
		}
		else
			multicode_state = 0;

		putchar(c); // echo the character
		buf[index++] = c;
		if (index >= sizeof(buf) - 1)
			break;
		if (c == '\n') // enter key
			break;
		if (c == 4) // Ctrl+D
			return EXIT;
	}
	if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
		index--;
	buf[index++] = 0; // null terminate string

	strcpy(oldbuf, buf);

	parse_command(buf, command);

	// print_command(command); // DEBUG: uncomment for debugging

	// restore the old settings
	tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
	return SUCCESS;
}

int process_command(struct command_t *command);

int main()
{
	while (1)
	{
		struct command_t *command = malloc(sizeof(struct command_t));
		memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

		int code;
		code = prompt(command);
		if (code == EXIT)
			break;

		code = process_command(command);
		if (code == EXIT)
			break;

		free_command(command);
	}

	printf("\n");
	return 0;
}

int process_command(struct command_t *command)
{
	int r;
	if (strcmp(command->name, "") == 0)
		return SUCCESS;

	if (strcmp(command->name, "exit") == 0)
		return EXIT;

	if (strcmp(command->name, "cd") == 0)
	{
		if (command->arg_count > 0)
		{
			r = chdir(command->args[0]);
			if (r == -1)
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			
			//Upon changing directory, add the current directory to chHistory for cdh command
			else {
			
				char cwd[512];
				if(getcwd(cwd,sizeof(cwd)) != NULL) {
					addCdToHistory(cwd);
				}
			
			}
			/////////////////
			return SUCCESS;
		}
	}

	// TODO: Implement your custom commands here
	
	//fileserach command
	if(strcmp(command->name, "filesearch") == 0) {

		int recursive = 0;
		int open = 0;
		char keyword[30];

		if(command->args[0] == NULL) {

			printf("Usage: filesearch 'keyword'. Options: -r, -o");
			return SUCCESS;
		}
		//Assigning command line options to open and recursive variables
		else if (strcmp(command->args[0],"-r") == 0) {
			recursive = 1;
			
			if(strcmp(command->args[1], "-o") == 0) {
				open = 1;
				strcpy(keyword, command->args[2]);
			}
			else {
				strcpy(keyword, command->args[1]);
			}
		}
		else if (strcmp(command->args[0],"-o") == 0) {
				open = 1;

			if(strcmp(command->args[1], "-r") == 0) {
				recursive = 1;
				strcpy(keyword, command->args[2]);
			}
			else {
				strcpy(keyword, command->args[1]);
			}
		}
		else {
			strcpy(keyword, command->args[0]);
		}
		//////////////////////////////////////////////////////////////////
		
		//Calling fileSearch function with inputs keyword, current dir (.),and options recursive and open
		fileSearch(keyword,".", recursive, open);
		return SUCCESS;	
	}
	//cdh command
	if(strcmp(command->name, "cdh") == 0) {

		if(command->arg_count > 0) {
			printf("cdh: Works with zero arguments.\n");
			return SUCCESS;
		}

		else if(cdCount == 0) {
			printf("No previous directories to select from!\n");
			return SUCCESS;
		}

		printCdHistory(cdHistory);

		char selected_dir[100];
		char selected_dir_main[100];
		pid_t pid;
		int pipefds[2];
		
		if(pipe(pipefds) == -1) {

			printf("Pipe failed!\n");
		}

		pid = fork();

		if(pid == 0) {

			printf("Select a directory by letter or number: ");
			scanf("%s",selected_dir);

			close(pipefds[0]);
			write(pipefds[1],selected_dir,(strlen(selected_dir)+1));
			exit(0);

		}
		else {
			wait(NULL);

			close(pipefds[1]);
			read(pipefds[0],selected_dir_main,sizeof(selected_dir_main));


			if(strcmp(selected_dir_main, "a") == 0) {
				strcpy(selected_dir_main, "1");
			}
			else if (strcmp(selected_dir_main, "b") == 0) {
				strcpy(selected_dir_main, "2");
			}
			else if (strcmp(selected_dir_main, "c") == 0) {
				strcpy(selected_dir_main, "3");
			}
			else if (strcmp(selected_dir_main, "d") == 0) {
				strcpy(selected_dir_main, "4");
			}
			else if (strcmp(selected_dir_main, "e") == 0) {
				strcpy(selected_dir_main, "5");
			}
			else if (strcmp(selected_dir_main, "f") == 0) {
				strcpy(selected_dir_main, "6");
			}
			else if (strcmp(selected_dir_main, "g") == 0) {
				strcpy(selected_dir_main, "7");
			}
			else if (strcmp(selected_dir_main, "h") == 0) {
				strcpy(selected_dir_main, "8");
			}
			else if (strcmp(selected_dir_main, "i") == 0) {
				strcpy(selected_dir_main, "9");
			}
			else if (strcmp(selected_dir_main, "j") == 0) {
				strcpy(selected_dir_main, "10");
			}

			int index = atoi(selected_dir_main);	
			
			r = chdir(cdHistory[cdCount - index]);
			if (r == -1) {
				printf("Please provide a valid number or letter!\n");
			}
			else {
			
				char cwd[512];
				if(getcwd(cwd,sizeof(cwd)) != NULL) {
					addCdToHistory(cwd);
				}
			
			}


		}
		return SUCCESS;
	}
	
	pid_t pid = fork();

	if (pid == 0) // child
	{
		// increase args size by 2
		command->args = (char **)realloc(
			command->args, sizeof(char *) * (command->arg_count += 2));

		// shift everything forward by 1
		for (int i = command->arg_count - 2; i > 0; --i)
			command->args[i] = command->args[i - 1];

		// set args[0] as a copy of name
		command->args[0] = strdup(command->name);
		// set args[arg_count-1] (last) to NULL
		command->args[command->arg_count - 1] = NULL;

		/// TODO: do your own exec with path resolving using execv()
		char path[1024] = "/bin/";
		strcat(path,command->name);
		execv(path,command->args);
		exit(0);
	}
	else
	{
		/// TODO: Wait for child to finish if command is not running in background
		if(command->args[command->arg_count-2] != "&") {
			wait(NULL);
		}
		return SUCCESS;
	}

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}

/**
  *Finds the files named "keyword" in given directory 
  * @param keyword
  * @param current_dir
  * @paran recursive
  * @param open
  * */
void fileSearch(char *keyword, char *current_dir,int recursive, int open) {
    
	struct dirent *dir;
	DIR *d = opendir(current_dir);
	char next_dir[512];
	char directory[512];


	if(d != NULL) {
		while((dir = readdir(d)) != NULL) {
		    
		    //If it is current or previous directory then ignore otherwise creates infinite loop
			if(strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {			

				if(strstr(dir->d_name,keyword)) {
	
					printf("%s/%s\n",current_dir,dir->d_name);

					if (open) {

						strcpy(directory,current_dir);
						strcat(directory,"/");
						strcat(directory,dir->d_name);
						printf("Directory: %s\n",directory);
						printf("Current DÄ°rectory: %s\n",current_dir);
						
						//Calling xdg-open in the child
						
						char *path = "/bin/xdg-open";
						char *args[] = {path,directory,NULL};
						pid_t pid = fork();

						if(pid == 0) {

							execv(path, args);
						}
						else {
							wait(NULL);
						}
						///////////////////////////////////
					}
				}
				//Next_dir resolving and calling recursively
				if(recursive) {
					strcpy(next_dir,current_dir);
					strcat(next_dir,"/");
					strcat(next_dir,dir->d_name);
					fileSearch(keyword, next_dir,recursive,open);

				}

			}
				
		}
		closedir(d);
	}
	return;	
}
