#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include "threadpool.c"

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

//*********************************************************
//*************  PRIVATE FUNCTIONS ************************

int readRequest (void*);
int checkBuffAndWrite(char[]);
void printError(int);
void data_response();
char* get_type_file(char[]);

//*********************************************************
//************** GLOBAL VARIABLES *************************
int sock_id;
int flag_is_f_d = 0; // if is file or directory
int flag_is_file = 0;
int flag_is_directory = 0;
int flag_w = 0;
int flag_error = 0;
int there_is_files = 0;
int length_dir = 0;
int length_file = 0;
char* method;
char path[3500];
char* protocol;
int port;
int pool_size;
int max_num_request;
char* time_now;
char answer[4096];
char is_modify[128];
char type_error[256];
char answ_error[256];
time_t now;

//*********************************************************
//******************* MAIN  *******************************
int main(int argc, char* argv[])
{
    char* c_pool_size;
    char* c_port;
    char* c_max_num_request;
    int count = 0;

    //--- Check if the input of the server is correct ---//
    if(argc>4)
    {
        perror("\nInput Error-'3'\n");
        exit(1);
    }

    c_port = argv[1];
    c_pool_size = argv[2];
    c_max_num_request = argv[3];

    // to do-check the input with "isdigit'

    // converter char* to int
    port = atoi(c_port);
    pool_size = atoi(c_pool_size);
    max_num_request = atoi(c_max_num_request);

    //check port and request
    if(port<1024 || port>65535)
    {
        perror("\nPort Error\n");
        exit(1);
    }

    // --- Create socket to each client ---//
    struct sockaddr_in srv;  // save the data of the server
    int sock = socket(AF_INET, SOCK_STREAM, 0); // open
    if(sock < 0)
    {
        perror("\nsocket\n");
        return EXIT_FAILURE;
    }

    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    srv.sin_addr.s_addr = htonl(INADDR_ANY);

    // connect with the port
    int c = bind(sock, (struct sockaddr*) &srv, sizeof(srv));
    if(c < 0)  // the port is in use
    {
        perror("\nbind\n");
        return EXIT_FAILURE;
    }

    // listen to the port, max 10 clients
    if(listen(sock, 10) < 0)   //quetion- necesito poner num_request en vez de 10?????
    {
        perror("\nlisten\n");
        return EXIT_FAILURE;
    }

    printf("\nListening to the port was successful");

    // create threadpool
    threadpool* t_pool;
    t_pool = create_threadpool(pool_size);

    // Server allways listen to the request //
    while(count < max_num_request)
    {
        // -- ipus --
        flag_is_f_d = 0;
        flag_is_file = 0;
        flag_is_directory = 0;
        flag_w = 0;
        there_is_files = 0;
        flag_error = 0;
        
        printf("\nWaiting for the client\n");
        sock_id = accept(sock, NULL, NULL);
        if(sock_id < 0) // if the socket at we use,response no succesfull connect
        {
            perror("\nRequest Error\n");
            exit(1);
        }
        printf("Responding to the client\n");
        dispatch( t_pool, &readRequest, &sock_id );  // access to the func. read_request
        count++;
    }
    close(sock);
return 0;
}


//*************************************************************
//************** READ REQUEST-CLIENTS *************************
int readRequest (void* arg)
{
    int rd = 0;
    char buf[4000]; // contain the request data from the client
    int* sock_client = (int*)arg;

    // READ
    rd = read(*sock_client, buf, sizeof(buf));
    //printf("buf:\n%s\n",buf );  // check
    if(rd < 0)
    {
        perror("\nError reading from socket\n");
        exit(1);
    }
    checkBuffAndWrite(buf);
    return 0;
}

//********************************************************
//************** CHECK & WRITE ***************************
int checkBuffAndWrite(char buf[])
{
    int count_arguments = 0;
    char copy_buf[4000];
    char cwd[3500];
    char* path_enter;
    char* get_cwd;
    char* str = NULL;
    char* first_l = NULL;
    struct stat sb; // use to check if exist the path

    // check if there is 3 arguments
    strcpy(copy_buf, buf);
    first_l = strtok(copy_buf, "\n\r");
    str = strtok(first_l, " ");
    while(str != NULL)
    {
        count_arguments++;
        str = strtok(NULL," ");
    }
    //printf("count: %d",count_arguments); //check
    if(count_arguments != 3)
    {
        perror("\nArguments will need be 3\n");
        return 0;
    }

    // ---- take the arguments ----
    method = strtok(buf," ");
    path_enter = strtok(NULL," ");
    protocol = strtok(NULL, "\n\r");
    /*printf("\nmethod: %s\n",method );  //check
    printf("path_enter: %s\n", path_enter);
    printf("protocol: %s\n", protocol);*/

    get_cwd = getcwd(cwd, sizeof(cwd)); //the place that client are(.../home/name)
    strcat(path, get_cwd);
    strcat(path, path_enter);
    //printf("path: %s\n", path); //check

    // --- check protocol and method ----
    int c1, c2, c3, i, method_is_legal=0;
    char* methods_arr[] = {"GET","HEAD","POST","PUT","DELETE","CONNECT","OPTIONS","TRACE"};
    char* possible_method;

    c1 = strcmp(protocol, "HTTP/1.0");
    c2 = strcmp(protocol, "HTTP/1.1");

    for (i = 0; i < 8; i++)
    {
      possible_method = methods_arr[i];
      c3 = strcmp(possible_method, method);
      if( c3 == 0 )
        method_is_legal = 1;
    }
    //printf("method_is_legal: %d\n", method_is_legal);  //check

    if( (c1 != 0 && c2 != 0) )
    {
        printError(400);
        flag_error = 1;
    }
    
    if(method_is_legal != 1 || strcmp(method,"GET") != 0)
    {
        printError(501);
        flag_error = 1;
    }

    // ---- Check if exist the PATH ----
    if(flag_error == 0)
    {
        if( stat(path, &sb) == 0) // exist
        {
            //printf("\nExist the path\n"); //check
            if( sb.st_mode & S_IFREG ) // if is file
            {
                //printf("\nis file\n"); //check
                flag_is_file = 1;
                if( S_ISREG(sb.st_mode) ) //normal file
                {
                    //printf("\nis normal file\n"); //check
                    memset(is_modify, '\0', sizeof(is_modify));
                    flag_is_file = 1;
                    flag_is_f_d = 1;
                    length_file = sb.st_size;
                    strcpy(is_modify, ctime(&sb.st_mtime));
                    data_response(length_file);
                }

                else
                  printError(403);
            }


            else if( sb.st_mode & S_IFDIR ) // if is directory
            {
                //printf("\nis directory\n");  //check
                if( path[strlen(path)-1] != '/' )
                    printError(302);

                else
                {
                    memset(is_modify, '\0', sizeof(is_modify));
                    flag_is_directory = 1;
                    flag_is_f_d = 1;
                    length_dir = sb.st_size;
                    strcpy(is_modify, ctime(&sb.st_mtime));
                    data_response(length_dir);
                }
            }

            else  // is something else
                printError(404);
        }
        
        else // the path not exist
            printError(404);
    }   

    


    //printf("\nSERVER WRITTING \n");  //check
    // ---- SERVER WRITTING -----
    if( write(sock_id, answer, strlen(answer)) < 0)
    {
        perror("write\n");
        return 0;
    }

    // --- Open Files ---
    if(flag_w == 1)
    {
        //printf("\nopen files\n");  //check
        FILE *fp;
        char b[512];
        fp = fopen(path,"r");
        if(fp == NULL)
            printError(404);

        while(fread(b, sizeof(b), 1, fp) == 1)
        {
            if(write(sock_id, b, strlen(b)) < 0)
            {
                perror("write\n");
                return 0;
            }
            memset(b, '\0', sizeof(b));
        }
    }

    memset(path, '\0', sizeof(path));
    memset(answer, '\0', sizeof(answer));
    close(sock_id);
    return 0;
}

//************************************************************
//***************** PRINT TYPE ERROR *************************
void printError(int flag)
{
    //printf("\nInside print-error\n"); //check
    
    int cont_length=0;
    if(flag == 302)
    {
      strcpy(type_error,"302 found");
      strcpy(answ_error,"Directories must end with a slash.");
      cont_length=123;
      //flag=1
    }

    else if(flag == 400)
    {
      strcpy(type_error,"400 Bad Request");
      strcpy(answ_error,"Bad Request.");
      cont_length=113;
    }

    else if(flag == 403)
    {
      strcpy(type_error,"403 Forbidden");
      strcpy(answ_error,"Access denied.");
      cont_length=111;
    }

    else if(flag == 404)
    {
      strcpy(type_error,"404 Not Found");
      strcpy(answ_error,"File not found.");
      cont_length=112;
    }

    else if(flag == 501)
    {
      strcpy(type_error,"501 Not supported");
      strcpy(answ_error,"Method is not supported.");
      cont_length=129;
    }

    //printf("\nFinished print-error\n"); //check
    data_response(cont_length);
    return;
}

//***********************************************************
//***************** DATA RESPONSE ***************************
void data_response(int cont_length)
{
    //printf("\nInside data_response\n"); //check
    char* type_file = NULL;
    char size[32];
    sprintf(size, "%d", cont_length); // int into string

    // -- time --
    char time_now[128];
    now = time(NULL);
    strftime(time_now, sizeof(time_now), RFC1123FMT , gmtime(&now) );

    // -- is file/directory --
    if(flag_is_f_d == 1)
    {
      strcpy(type_error,"200 OK");
      strcpy(answ_error,"OK");
    }

    // --- strcat ---
    strcpy(answer, "\r\nHTTP/1.0 ");
    strcat(answer,type_error);
    strcat(answer,"\r\n");
    strcat(answer,"Server: webserver/1.0\r\n");
    strcat(answer,"Date: ");
    strcat(answer,time_now);
    strcat(answer,"\r\n");

    // -- type of File --
    if(flag_is_file == 1)
      type_file = get_type_file(path);
    else
      type_file = "text/hmtl";

    if(type_file != NULL)
    {
        strcat(answer,"Content-Type: ");
        strcat(answer,type_file);
        strcat(answer,"\r\n");
    }

    // --- length ---
    strcat(answer, "Content-Length: ");
    strcat(answer, size);
    strcat(answer,"\r\n");

    // -- is file/directory --
    if(flag_is_f_d == 1)
    {
        strcat(answer, "Last-Modified: ");
        strcat(answer,is_modify);
        strcat(answer, "\r\n");
    }

    strcat(answer,"Connection: close\r\n");
    strcat(answer, "\r\n");

    // -- if there is Error --
    if(flag_is_f_d == 0)
    {
        strcat(answer,"<HTML><HEAD><TITLE>");
        strcat(answer,type_error);
        strcat(answer,"</TITLE></HEAD>\r\n");
        strcat(answer,"<BODY><H4>");
        strcat(answer,type_error);
        strcat(answer,"</H4>\r\n");
        strcat(answer,answ_error);
        strcat(answer,"\r\n");
        strcat(answer,"</BODY></HTML>\r\n");
    }
    
    // -- is a Directory --
    if(flag_is_directory == 1)
    {
        //printf("\nINSIDE - FLAG_DIR\n");
        
        // -- search for index.html --
        char temp_path[3500];
        struct stat check;

        strcpy(temp_path,path);
        //printf("\ntemp_path:%s\n", temp_path);  //check
        strcat(temp_path,"index.html");
        //printf("\ntemp_path:%s\n", temp_path); //check

         if(stat(temp_path, &check) == 0 ) //exist
         {
            // -- if is a file index.html --
            if(check.st_mode & S_IFREG)
                there_is_files = 1;
         }

         // -- there is not index.html --
         else
         {
            struct stat contents;
            if ( stat(path , &contents) == 0 )
            {
                strcat(answer,"<HTML>\r\n");
                strcat(answer,"<HEAD><TITLE>Index of ");
                strcat(answer,path);
                strcat(answer,"</TITLE></HEAD>\r\n");
                strcat(answer,"\r\n");
                strcat(answer,"<BODY>\r\n");
                strcat(answer,"<H4>Index of ");
                strcat(answer,path);
                strcat(answer,"</H4>\r\n");
                strcat(answer,"\r\n");
                strcat(answer,"<table CELLSPACING=8>\r\n");
                strcat(answer,"<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n");

                // -- open directory --
                DIR *dir;
                struct dirent *dent;
                dir = opendir(path); //open
                if(dir == NULL)
                {
                    // -- printError(404) --
                    strcpy(type_error,"404 Not Found");
                    strcpy(answ_error,"File not found.");
                    cont_length=112;
                    return;
                }

                //printf("\nAbri el directorio\n"); //check

                // -- dir != NULL , for each entity in the path --
                while ( (dent  = readdir(dir)) != NULL )
                {
                    struct stat s;

                    if ( stat(dent->d_name, &s) == 0 ) //exist files
                    {
                        // if into the directory there is file / directory
                        if ( S_ISREG(s.st_mode) || S_ISDIR(s.st_mode))
                        {
                                //printf("\ninto the directory there is file / directory\n"); //check
                                strcat(answer,"<tr><td><A HREF=");
                                strcat(answer,path);
                                strcat(answer,dent->d_name);
                                if( S_ISDIR(s.st_mode) )
                                    strcat(answer,"/");
                                strcat(answer,"'>");

                                strcat(answer,dent->d_name);
                                strcat(answer,"</A></td><td>");
                                strcat(answer,ctime(&s.st_mtime));
                                strcat(answer,"</td><td>");

                                // if is a file
                                if(!S_ISDIR(s.st_mode))
                                {
                                    char str[24];
                                    sprintf(str, "%d",(int)s.st_size);
                                    strcat(answer,str);
                                }
                                strcat(answer,"</td></tr>");
                         }
                   }

                   else
                   {
                        // -- printError(404) --
                        strcpy(type_error,"404 Not Found");
                        strcpy(answ_error,"File not found.");
                        cont_length=112;
                        return;
                   }

                }

                closedir (dir);
                strcat(answer,"</table>\n<HR>\n<ADDRESS>webserver/1.0</ADDRESS>");
                strcat(answer,"</BODY></HTML>");
            }
         }
    }

   /*printf("\nflag_dir:%d\n", flag_is_directory);  //check
    printf("\nflag_file:%d\n", flag_is_file);
    printf("\nflag_there_is_files:%d\n", there_is_files);
    printf("\nflag_is_f_g:%d\n", flag_is_f_d);*/

    if(flag_is_file == 1 || there_is_files == 1)
        flag_w = 1;

    return;
}


//********************************************************
//************** TYPE OF FILE ****************************
char* get_type_file(char path[])
{
    char *temp = strrchr(path, '.');
    if (!temp)
       return NULL;

    if (strcmp(temp, ".html") == 0 || strcmp(temp, ".htm") == 0)
        return "text/html";

    if (strcmp(temp, ".jpg") == 0 || strcmp(temp, ".jpeg") == 0)
       return "image/jpeg";

    if (strcmp(temp, ".mpeg") == 0 || strcmp(temp, ".mpg") == 0)
       return "video/mpeg";

    if (strcmp(temp, ".gif") == 0)
       return "image/gif";

    if (strcmp(temp, ".png") == 0)
       return "image/png";

    if (strcmp(temp, ".css") == 0)
       return "text/css";

    if (strcmp(temp, ".au") == 0)
       return "audio/basic";

    if (strcmp(temp, ".wav") == 0)
       return "audio/wav";

    if (strcmp(temp, ".avi") == 0)
       return "video/x-msvideo";

    if (strcmp(temp, ".mp3") == 0)
       return "audio/mpeg";

    return NULL;
}
