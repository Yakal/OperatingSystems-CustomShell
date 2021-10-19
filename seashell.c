#include <unistd.h>
#include <sys/wait.h>  
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
const char *sysname = "seashell";


//PATH USED FOR QUESTION 2
char abspath[500];
char namefile_path[500];
char pathfile_path[500];

//METHODS DEFINED 

//METHODS USED IN Q2
int find_index(char* str1, char* fileName);
void clear();
void update_txt(int line_number, char *new_txt, char *file_name);
void set_name(char *name, char *path);
void delete(char *name);
void list_associations();
char* get_path(char *name);

//METHODS USED IN Q3
char* to_lower_case(char* word);
void print_colored_text(char *color, char*text);
void highlight(char *word, char* color, char* file_name);

//METHODS USED IN Q5
void compare_txt_files(char *txt1, char *txt2);
void compare_binary_files(char *file1_name, char *file2_name);
//METHODS USED IN Q6
void concatenate_txt_files(int argc, char *argv[]);
//------------------------------------------

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

	//FIXME: backspace is applied before printing chars
	show_prompt();
	int multicode_state = 0;
	buf[0] = 0;
	while (1)
	{
		c = getchar();
		//  printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

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

	//
	//IN QUESTION 2 THE ASSOCIATIONS ARE STORED IN TXT FILES
	//TO WORK ON ANY MACHINE THE PATHS ARE DETERMINED AT THE BEGINNING
	//
	getcwd(abspath, sizeof(abspath));
	strcat(abspath,"/");
	strcpy(namefile_path, abspath);
	strcpy(pathfile_path, abspath);
	strcat(namefile_path, "name.txt");
	strcat(pathfile_path, "path.txt");
	//
	//
	//

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
			return SUCCESS;
		}
	}
	//---------------QUESTION 2---------------//
	else if (strcmp(command->name, "shortdir") == 0)
	{
		if(strcmp(command->args[0], "set") == 0 && command->args[1]!= NULL){
			char cwd[500];
			set_name(command->args[1],getcwd(cwd, sizeof(cwd)));
		}else if(strcmp(command->args[0], "jump") == 0 && command->args[1]!= NULL){
			char *path = get_path(command->args[1]);
			if(strcmp(path, " ")!= 0){
				chdir(path);
			}
		}else if(strcmp(command->args[0], "del") == 0 && command->args[1]!= NULL){
			delete(command->args[1]);
		}else if(strcmp(command->args[0], "clear") == 0 && command->args[1]== NULL){
			clear();
		}else if(strcmp(command->args[0], "list") == 0 && command->args[1]== NULL){
			list_associations();
		}else{
			printf("Wrong argument %s\n", command->args[0]);
		}

		return SUCCESS;
	}
	
	//---------------QUESTION 3---------------//
	else if(strcmp(command->name, "highlight") == 0){
		if(command->arg_count == 3){
			highlight(command->args[0], command->args[1], command->args[2]);
		}else{
			printf("Error with the argument format\n");
		}
		return SUCCESS;
	}
	//---------------QUESTION 4---------------//
	else if(strcmp(command->name, "goodMorning") == 0){
		if(command->arg_count!=2){
			printf("Problem with the arguments\n");
			return SUCCESS;
		}
		char *hour = strtok(command->args[0], ".");
		char *minutes = strtok(NULL, ".");
		if(hour==NULL || minutes==NULL){
			printf("Problem with the time format");
			return SUCCESS;
		}

		FILE *job_file = fopen("new_job.sh", "w");
		fprintf(job_file, "%s %s * * * aplay %s\n", minutes, hour, command->args[1]);		
		fclose(job_file);

		char *crontab_args[] = {"crontab", "new_job.sh", NULL};

		execv("/usr/bin/crontab", crontab_args);
		return SUCCESS;
	}
	//---------------QUESTION 5---------------//
	else if(strcmp(command->name, "kdiff") == 0){
		if(command->arg_count<2){
			printf("Problem with the arguments\n");
		}
		else if(strcmp(command->args[0], "-b")==0){
			compare_binary_files(command->args[1], command->args[2]);
		}else{
			compare_txt_files(command->args[command->arg_count-2], command->args[command->arg_count-1]);
		}
		return SUCCESS;
	}
	//---------------QUESTION 6---------------//
	else if(strcmp(command->name, "concatenate") == 0){
		concatenate_txt_files(command->arg_count, command->args);
		return SUCCESS;
	}

   	pid_t pid = fork();
	 if (pid == 0) {

		command->args=(char **)realloc(command->args, sizeof(char *)*(command->arg_count+=2));

		for (int i=command->arg_count-2;i>0;--i)
			command->args[i]=command->args[i-1];

		command->args[0]=strdup(command->name);
		command->args[command->arg_count-1]=NULL;

	//---------------QUESTION 1---------------//
		if(command->name[0]=='/' || command->name[0]=='.'){
			execv(command->name, command->args);
		}
		else if(strcmp(command->name, "gcc")==0){
			execv("/usr/bin/gcc", command->args);
		}

		else{
			char path[100] = "/bin/";
			strcat(path, command->name);
			execv(path, command->args);
		}
     }

	
	else
	{
		if (!command->background)
			wait(0); // wait for child process to finish
		return SUCCESS;
	}

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}

// HELPER METHODS FOR QUESTIONS 2-3-5-6 //

// QUESTION 2 HELPER METHODS START //

//find the line number of the name or path from the txt files
//if there is no association returns -1
//related updates are done according to line number
int find_index(char* str1, char* file_name){
   int line_num=0;
   int is_found = -1;

   char line[256];
   char name[256];
   strcpy(name, str1);

   FILE* file = fopen(file_name, "r"); 

   if(file==NULL){
      return -1;
   }

   while (fgets(line, sizeof(line), file)) {
      if(strcmp(line, name) == 0){
         is_found ++;
         break;
      }
      line_num ++;   
   }
   fclose(file);
   return is_found==-1 ? -1 : line_num;
}

//deletes all associations by removing txt files
void clear(){
   remove(namefile_path);
   remove(pathfile_path);
   printf("All associations are removed\n");
}

//updates the value in the given line of the file
void update_txt(int line_number, char *new_txt, char *file_name){
	if (line_number<0){
		return;
	}
      FILE *txt_file, *dummy;
      txt_file = fopen(file_name, "r");
      dummy = fopen("dummy.txt", "a+"); 
      char line[100];
      int count = 0;

      while (fgets(line, sizeof(line), txt_file) != NULL){
         if (count == line_number){
            fputs(new_txt, dummy);
         }else{
            fputs(line, dummy);
         }
         count++;
      }
      fclose(txt_file);
      fclose(dummy);

      remove(file_name);
      rename("dummy.txt", file_name);
}

//set or update the association in txt files
void set_name(char *name, char *path){
   char inp_name[100], inp_path[100];
   
   strcpy(inp_name, name);
   strcat(inp_name, "\n");
   strcpy(inp_path, path);
   strcat(inp_path, "\n");

   int index_path = find_index(inp_path, pathfile_path);
   int index_name = find_index(inp_name, namefile_path);

   if(index_name>index_path){
	   update_txt(index_name, "", namefile_path);
	   update_txt(index_name, "", pathfile_path);
	   update_txt(index_path, "", namefile_path);
	   update_txt(index_path, "", pathfile_path);
   }else{
	   update_txt(index_path, "", namefile_path);
	   update_txt(index_path, "", pathfile_path);
	   update_txt(index_name, "", namefile_path);
	   update_txt(index_name, "", pathfile_path);
   }

	FILE *fp_name, *fp_path;
	fp_name = fopen(namefile_path, "a+");
	fp_path = fopen(pathfile_path, "a+");
	fputs(inp_name, fp_name);
	fputs(inp_path, fp_path);
	fclose(fp_name);
	fclose(fp_path);

   printf("%s is set as an alias for %s\n", name, path);
}

//deletes the association from the txt files
void delete(char *name){
   char inp_name[100];
   
   strcpy(inp_name, name);
   strcat(inp_name, "\n");

   int index = find_index(inp_name, namefile_path);
   if(index!=-1){
      update_txt(index, "", namefile_path);
      update_txt(index, "", pathfile_path);
	  printf("%s association is removed\n", name);
   }else{
      printf("No association with the given name %s\n", name);
   }
}

//prints all the present associations
void list_associations(){
    FILE *fp_name, *fp_path;
    fp_name = fopen(namefile_path, "r");
    fp_path = fopen(pathfile_path, "r");

   if(fp_name==NULL && fp_path==NULL){
      printf("There isn't any association\n");
      return;
   }

   char line_name[100], line_path[100];
   int counter = 0;

   while ((fgets(line_name, sizeof(line_name), fp_name) != NULL) && (fgets(line_path, sizeof(line_path), fp_path) != NULL)){
      printf("\nNAME: %sPATH: %s\n", line_name, line_path);
	  counter ++;
   }
   
   if (counter==0){
		printf("There isn't any association\n");
   }
   fclose(fp_name);
   fclose(fp_path);
}

//returns the path corresponding to given name
//used for shortdir jump
char* get_path(char *name){
	char inp_name[100];
   strcpy(inp_name, name);
   strcat(inp_name, "\n");
   int index = find_index(inp_name, namefile_path);
   char *path = malloc(256);

   if (index==-1){
      printf("There is no associated path with the given name\n");
      strcpy(path, "");
   }else{
      FILE *fp_path;
      fp_path = fopen(pathfile_path, "r");
      
      char line[100];
      int count=0;

      while(fgets(line, sizeof(line), fp_path) != NULL){
         if (count == index){
            line[strlen(line)-1] = '\0';
            strcpy(path, line);
			break;
         }
         count ++;
      }
      fclose(fp_path);
   }
   return path;
}
// QUESTION 2 HELPER METHODS END //


// QUESTION 3 HELPER METHODS START //

//since the search is case-insensitive all the words are replaced with lower versions
char* to_lower_case(char* word){
   char *new_word = malloc(100);
   strcpy(new_word, word);

   for(int i=0;i<=strlen(new_word);i++){
      if(new_word[i]>=65&&new_word[i]<=90){
         new_word[i]=new_word[i]+32;
      }
   }
   return new_word;
}

//displaying colored text r, g or b
void print_colored_text(char *color, char*text){
   if (strcmp(color, "r") == 0){
      printf("\033[;31m%s \033[0m",text);
   }else if(strcmp(color, "b") == 0){
      printf("\033[;34m%s \033[0m",text);
   }else if(strcmp(color, "g") == 0){
      printf("\033[;32m%s \033[0m",text);
   }
} 

//the main method of the question3
//takes a word, color char and file_name. Changes the color of every occurence of the given word in given file.
void highlight(char *word, char* color, char* file_name){
   char* lowered_word = to_lower_case(word);
   
   FILE *txt_file;
   txt_file = fopen(file_name, "r");

   char line[256];
   char word_in_line[50];
   
   int i;
   int j = 0;

   while(fgets(line, sizeof(line), txt_file)!=NULL){
      for (i=0; i<strlen(line); i++){
         if(line[i]== ' ' || line[i]=='\0' || line[i]=='\n'){
            word_in_line[j] = '\0';
            if(strcmp(lowered_word, to_lower_case(word_in_line))==0){
               print_colored_text(color, word_in_line);
            }else{
               printf("%s ", word_in_line);
            }
            j=0;
         }
         else{
            word_in_line[j] = line[i];
            j++;
         }
      }
      printf("\n");
   }
   fclose(txt_file);
}

// QUESTION 3 HELPER METHODS END //


// QUESTION 5 HELPER METHODS START //

//takes two files in .txt format and displays the different lines.
void compare_txt_files(char *txt1, char *txt2){
   FILE *txt1_file, *txt2_file;
   int l1 = strlen(txt1);
   int l2 = strlen(txt2);

   if(l1<4 || txt1[l1-4]!='.' || txt1[l1-3]!='t' || txt1[l1-2]!='x' || txt1[l1-1]!='t' || 
   l2<4 || txt2[l2-4]!='.' || txt2[l2-3]!='t' || txt2[l2-2]!='x' || txt2[l2-1]!='t'){
      printf("File should be in txt format\n");
      return;
   }

   txt1_file = fopen(txt1, "r");
   txt2_file = fopen(txt2, "r");

   if(txt1_file==NULL || txt2_file==NULL){
      printf("Problem with the file format");
      return;
   }

   char txt1_line[100], txt2_line[100];
   
   int line_number = 1;
   int mismatch = 0;

   int checker1 = 0;
   int checker2 = 0;

   while(1){
      if(fgets(txt1_line, sizeof(txt1_line), txt1_file)==NULL) checker1++;
      if(fgets(txt2_line, sizeof(txt2_line), txt2_file)==NULL) checker2++;
      if(checker1!=0 && checker2!=0){
         break;
      }
      if(checker1 != 0) strcpy(txt1_line, "\n");
      if(checker2 != 0) strcpy(txt2_line, "\n");

      if(txt1_line[strlen(txt1_line)-1] != '\n') strcat(txt1_line, "\n");
      if(txt2_line[strlen(txt2_line)-1] != '\n') strcat(txt2_line, "\n");

      if(strcmp(txt1_line, txt2_line)!=0){
         mismatch++;
         printf("%s:Line %d: %s",txt1, line_number, txt1_line);
         printf("%s:Line %d: %s",txt2, line_number, txt2_line);
      }
      line_number++;
   }
   fclose(txt1_file);
   fclose(txt2_file);
   if(mismatch==0){
      printf("The two files are identical\n");
   }else{
      printf("%d different lines found\n", mismatch);
   }
}

//takes two files in any format and compares them byte by byte
void compare_binary_files(char *file1_name, char *file2_name){
    FILE *file1, *file2;
    long size1, size2;  
    char tmp1, tmp2;

    file1 = fopen(file1_name,"r");
    file2 = fopen(file2_name,"r");

   if(file1==NULL || file2==NULL){
      printf("Problem with the file format");
      return;
   }

    fseek (file1 , 0 , SEEK_END);
    size1 = ftell (file1);
    rewind (file1);

    fseek (file2 , 0 , SEEK_END);
    size2 = ftell (file2);
    rewind (file2);

   int i=0;
   int difference_counter = 0;

   for (i=0;i<size1;i++) {
      fread(&tmp1, 1, 1, file1);
      fread(&tmp2, 1, 1, file2);
      if (tmp1 != tmp2) {
         difference_counter += 1;
      }
   }

   int length_diff = size1-size2;
   if (length_diff<0){
	   length_diff = length_diff *-1;
   }
   difference_counter = difference_counter + length_diff;

   if (difference_counter==0){
      printf("The two files are identical\n");
   }else{
      printf("The two files are different in %d bytes\n", difference_counter);
   }
}
// QUESTION 5 HELPER METHODS END //


// QUESTION 6 HELPER METHOD START //

// # of arguments = 1....n where n>=2
// All arguments need to be in the .txt format.
// Starting from the second .txt file all .txt files are concatenated one by one.
// The resulting concatenated .txt file is saved with the name given in the first argument.
void concatenate_txt_files(int argc, char *argv[]){
   if(argc<2){
      printf("Bad format there should be at least 2 arguments. %d is given\n", argc);
      return;
   }
   
   int l;
   int i;
   for (i=0; i<argc; i++){
      l = strlen(argv[i]);
      if(l<4 || argv[i][l-4]!='.' || argv[i][l-3]!='t' || argv[i][l-2]!='x' || argv[i][l-1]!='t'){
         printf("All files should be in txt format\n");
         return;
      }
   }

   FILE *output_txt, *input_txt;
   output_txt = fopen(argv[0], "a+");
   char line[256];

   for(i=1; i<argc;i++){
      input_txt = fopen(argv[i], "r");
      while(fgets(line, sizeof(line), input_txt)!=NULL){
         fputs(line, output_txt);
      }
      fclose(input_txt);
   }
   fclose(output_txt);
}
// QUESTION 6 HELPER METHOD END //
