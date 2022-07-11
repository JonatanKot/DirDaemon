#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <syslog.h>
#include "avl.h"
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include "queue.h"
#include <time.h>
#include <limits.h>

typedef struct avl_table avl_table;
typedef struct avl_node avl_node_t;
typedef struct dirent dirent;
typedef struct timespec timespec;

typedef struct tree_data{
dirent val;
struct timespec mtim;
} tree_data;

void direntcpy(char *prefix, dirent* source, dirent* dest){ /* gdzie dest jest pusty*/
	strcpy(dest->d_name,prefix);
	strcpy(&dest->d_name[strlen(prefix)],source->d_name);
	dest->d_fileno = source->d_fileno;
	dest->d_type = source->d_type;
}

int dirent_comparator(const void *l, const void *r, void *avl_param)
{
	const dirent* li = l;
	const dirent* ri = r;

	return strcmp(li->d_name, ri->d_name);
}

void dirent_free(void* item, void* param){
	//dirent* entry = item;
	//free(entry->d_name);
	free(item);
}

int timespec_comparator(const timespec *lhs, const timespec *rhs)
{
    if (lhs->tv_sec == rhs->tv_sec)
        return lhs->tv_nsec < rhs->tv_nsec;
    else
        return lhs->tv_sec < rhs->tv_sec;
}

void time_msg(int facility_priority, char *buf, char *msg, char *msg2){
    time_t rawtime;
    struct tm * timeinfo;
    int len;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    strcpy(buf, asctime(timeinfo));
    len = strlen(buf);
    buf[len] = ' ';
    //strcpy(&buf[len+1], getlogin());
    //len = strlen(buf);
    //buf[len] = ' ';
    //strcpy(&buf[len+1], "imp");
    //len = strlen(buf);
    //buf[len] = ' ';
    strcpy(&buf[len+1], msg);
    if(msg2[0] != '\0'){
        len = strlen(buf);
        buf[len] = ' ';
        strcpy(&buf[len+1], msg2);
    }
    syslog(facility_priority, buf);
    closelog();
}

void handler1(int signum){
    char buf[256];
    time_msg(LOG_INFO, buf, "SIGUSR1 received", "");
}

/////////////////////////////////////////////////////////////////////////////////// main
int main(int argc, char** args){

    int Rflag = 0;
    int sleeps = 300;
    struct stat st;
    char msgbuf[256];
    off_t big_fsize = 1048576;

	if(argc < 3){
		write(2,"Zla ilosc argumentow\n",21);
		return(-1);
	}

    int option;
	while ((option = getopt(argc, args, "Rf:s:")) != -1) {
    switch (option) {
    case 'R':
      Rflag = 1;
      break;
    case 'f':
      big_fsize = atoi(optarg);
      break;
    case 's':
      sleeps = atoi(optarg);
      break;
    default:
      write(2,"opcja ",6);
      write(2,&optopt,1);
      write(2," nie istnieje\n",15);
    }
  }

	char path1[PATH_MAX + 1];
	char path2[PATH_MAX + 1];

	/* Copy the directory path into entry_path. */
	realpath(args[optind],path1);
	size_t path1len = strlen (path1);
	path1[path1len] = '\0';
	realpath(args[optind+1],path2);
	size_t path2len = strlen (path2);
	path2[path2len] = '\0';
	/* If the directory path doesn't end with a slash, append a slash. */
	if (path1[path1len - 1] != '/') {
		path1[path1len] = '/';
		path1[path1len + 1] = '\0';
		++path1len;
	}
	if (path2[path2len - 1] != '/') {
		path2[path2len] = '/';
		path2[path2len + 1] = '\0';
		++path2len;
	}
	lstat(path1,&st);
	if(! S_ISDIR (st.st_mode)){
        write(2,"\"",1);
        write(2, path1, path1len);
        write(2, "\" nie jest katalogiem", 21);
        return(-1);
	}
	lstat(path2,&st);
	if(! S_ISDIR (st.st_mode)){
        write(2,"\"",1);
        write(2, path2, path2len);
        write(2, "\" nie jest katalogiem", 21);
        return(-1);
	}

//////////////////////////////////////////////////////////////////////// demonizacja
	pid_t pid = fork();
	if (pid < 0) {
                time_msg(LOG_ERR, msgbuf, "fork failure", "");
                exit(EXIT_FAILURE);
        }
	if (pid > 0) {
                exit(EXIT_SUCCESS);
        }
	umask(0);
	pid_t sid = setsid();
        if (sid < 0) {
                time_msg(LOG_ERR, msgbuf, "setsid failure", "");
                exit(EXIT_FAILURE);
        }
	if ((chdir("/")) < 0) {
                time_msg(LOG_ERR, msgbuf, "chdir failure", "");
                exit(EXIT_FAILURE);
        }
    time_msg(LOG_INFO, msgbuf, "Utworzenie demona", "");


	if (signal(SIGUSR1, handler1) == SIG_ERR){
        	time_msg(LOG_ERR, msgbuf, "SIGUSR1 handler assignement error", "");
            exit(EXIT_FAILURE);
    	}

	avl_table *files1, *files2;
        files1 = avl_create(dirent_comparator, NULL, NULL);
        files2 = avl_create(dirent_comparator, NULL, NULL);

    //////////////////////////////////////////////////////////////////////// Definicje większości zmiennych
	DIR *dir1, *dir2;
	int fd1, fd2;
	dirent* entry;
	avl_node_t* where;
	tree_data *datapointer;
	tree_data datasearch;
	struct avl_traverser traverser;
	char buf[256];
	size_t bytes_read;
	queue_t dir_queue;
	char *dir_string;
	int fnameoffset;
	char *spawner;
	queue_t dir_stack;
	//clock_t start, end;  //Do mierzenia czasu kopiowania
    //double cpu_time_used;
    //char cptime[32];

    //////////////////////////////////////////////////////////////////////// Pętla demona
	while (1) {
    ///////////////////////////////////////////////////////////////// Uzupełnianie drzewa katalogu źródłowego
	   dir_string = malloc(1);
	   dir_string[0] = '\0';
       enqueue(&dir_queue, dir_string);

	   while(dir_string = dequeue(&dir_queue)){

            strncpy (path1 + path1len, dir_string,
                sizeof (path1) - path1len);
            fnameoffset = path1len + strlen(dir_string);

            if((dir1 = opendir(path1)) == 0){
                time_msg(LOG_ERR, msgbuf, path1, " błąd otwarcia katalogu");
                exit(EXIT_FAILURE);
            }
            while ((entry = readdir (dir1)) != NULL) {
                if(strcmp(entry->d_name,".") == 0 ||
                    strcmp(entry->d_name,"..") == 0)
                    continue;

                strncpy (path1 + fnameoffset, entry->d_name,
                sizeof (path1) - fnameoffset);
                lstat (path1, &st);
                if (S_ISREG (st.st_mode) || (S_ISDIR (st.st_mode) && Rflag)){
                    datapointer = malloc(sizeof(tree_data));
                    direntcpy(dir_string, entry, datapointer);

                    if ((where = avl_probe(files1, datapointer)) != NULL){
                        datapointer->mtim = st.st_mtim;

                        if(S_ISDIR (st.st_mode) && Rflag){
                            int len = strlen(datapointer->val.d_name);
                            spawner = malloc(len+2);
                            strcpy(spawner, datapointer->val.d_name);
                            strcpy(&spawner[len], "/");
                            enqueue(&dir_queue, spawner);
                        }
                    }
                    else
                        free(datapointer);
                }
            }
            path1[path1len] = '\0';
            closedir(dir1);
            free(dir_string);
        }
        //////////////////////////////////////////////////////// Uzupełnianie drzewa katalogu docelowego

        dir_string = malloc(1);
        dir_string[0] = '\0';
        enqueue(&dir_queue, dir_string);

        while(dir_string = dequeue(&dir_queue)){

            strncpy (path2 + path2len, dir_string,
                sizeof (path2) - path2len);
            fnameoffset = path2len + strlen(dir_string);

            if((dir2 = opendir(path2)) == 0){
                time_msg(LOG_ERR, msgbuf, path1, " błąd otwarcia katalogu");
                exit(EXIT_FAILURE);
            }
            while ((entry = readdir (dir2)) != NULL) {
                if(strcmp(entry->d_name,".") == 0 ||
                    strcmp(entry->d_name,"..") == 0)
                    continue;

                strncpy (path2 + fnameoffset, entry->d_name,
                    sizeof (path2) - fnameoffset);
                lstat (path2, &st);
                if (S_ISREG (st.st_mode) || (S_ISDIR (st.st_mode) && Rflag)){
                    direntcpy(dir_string, entry, &datasearch);

                    if (avl_find(files1, &datasearch) == NULL){     ///////////////////////////////////////
                        if(S_ISDIR (st.st_mode) && Rflag){          //Usuwanie plików z katalogu docelowego
                            int len = strlen(datasearch.val.d_name);///////////////////////////////////////
                            spawner = malloc(len+2);
                            strcpy(spawner, datasearch.val.d_name);
                            strcpy(&spawner[len], "/");
                            enqueue(&dir_queue, spawner);
                            spawner = malloc(len+1);
                            strcpy(spawner, datasearch.val.d_name);
                            push(&dir_stack, spawner);
                            continue;
                        }
                        unlink(path2);
                        time_msg(LOG_INFO, msgbuf, path2, " deleted");
                        avl_delete(files2, &datasearch);
                    }
                    else{

                        if ((where = avl_probe(files2, &datasearch)) != NULL){
                            datapointer = where->avl_data = malloc(sizeof(tree_data));
                            direntcpy("", &datasearch, datapointer);
                            datapointer->mtim = st.st_mtim;
                        }
                        if(S_ISDIR (st.st_mode) && Rflag){
                            int len = strlen(datapointer->val.d_name);
                            spawner = malloc(len+2);
                            strcpy(spawner, datapointer->val.d_name);
                            strcpy(&spawner[len], "/");
                            enqueue(&dir_queue, spawner);
                        }
                    }
                }
            }
            path2[path2len] = '\0';
            closedir(dir2);
            free(dir_string);
        }

        while(spawner = dequeue(&dir_stack)){////////////////////////Usuwanie katalogów z katalogu docelowego
            strncpy (path2 + path2len, spawner,
                    sizeof (path2) - path2len);
            rmdir(path2);
            time_msg(LOG_INFO, msgbuf, path2, " deleted");
            free(spawner);
        }
        path2[path2len] = '\0';

    /////////////////////////////////////////////////////////////////////////////////////Kopiowanie plików
	   entry = avl_t_first(&traverser, files1);
	   while(entry != NULL){
            datapointer = avl_find(files2,entry);
            if(datapointer == NULL || timespec_comparator(&datapointer->mtim, &((tree_data*)entry)->mtim)){
                strncpy (path1 + path1len, entry->d_name,
                    sizeof (path1) - path1len);
                strncpy (path2 + path2len, entry->d_name,
                    sizeof (path2) - path2len);

                stat(path1, &st);
                if(S_ISDIR (st.st_mode)){
                    if(datapointer == NULL){
                        mkdir(path2,0777);
                        chmod(path2, st.st_mode);
                        time_msg(LOG_INFO, msgbuf, path2, " created");
                    }
                    path1[path1len] = '\0';
                    path2[path2len] = '\0';
                    entry = avl_t_next(&traverser);
                    continue;
                }

                if(datapointer != NULL)
                    unlink(path2);
                if ((fd1 = open(path1,O_RDONLY)) == -1){
                    time_msg(LOG_ERR, msgbuf, path1, " błąd otwarcia pliku");
                    exit(EXIT_FAILURE);
                }

                if((fd2 = open(path2,O_WRONLY | O_CREAT, 0666)) == -1){
                    time_msg(LOG_ERR, msgbuf, path2, " błąd otwarcia pliku");
                    exit(EXIT_FAILURE);
                }

                if(st.st_size <= big_fsize){

                    //start = clock();

                    while((bytes_read = read(fd1,buf,256)) > 0){
                        if(write(fd2,buf,bytes_read) == -1){
                            time_msg(LOG_ERR, msgbuf, path2, " błąd zapisu do pliku");
                            exit(EXIT_FAILURE);
                        }
                    }

                    //end = clock();
                    //cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
                    //sprintf(cptime,"%f",cpu_time_used);
                    //time_msg(LOG_INFO, msgbuf, path2, cptime);

                    if(bytes_read == -1){
                        time_msg(LOG_ERR, msgbuf, path2, " błąd odczytu z pliku");
                        exit(EXIT_FAILURE);
                    }
                }
                else{
                    //start = clock();
                    while((bytes_read = copy_file_range (fd1, NULL, fd2, NULL,
                        SSIZE_MAX, 0)) == SSIZE_MAX);
                    //end = clock();
                    //cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
                    //sprintf(cptime,"%f",cpu_time_used);
                    //time_msg(LOG_INFO, msgbuf, path2, cptime);
                    if(bytes_read == -1){
                        time_msg(LOG_ERR, msgbuf, path2, " copy_file_range error");
                        exit(EXIT_FAILURE);
                    }
                }

                chmod(path2, st.st_mode);
                close(fd1);
                close(fd2);
                time_msg(LOG_INFO, msgbuf, path2, " mirrored");

                path1[path1len] = '\0';
                path2[path2len] = '\0';
            }
            entry = avl_t_next(&traverser);
        }
        avl_destroy (files1, dirent_free);
        avl_destroy (files2, dirent_free);
        time_msg(LOG_INFO, msgbuf, "Uśpienie demona", "");

           sleep(sleeps);

        time_msg(LOG_INFO, msgbuf, "Obudzenie demona", "");
        }

	exit(EXIT_SUCCESS);
}
