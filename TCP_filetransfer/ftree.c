#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "hash.h"
#include "ftree.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <limits.h>

#ifndef PORT
  #define PORT 30000
#endif

struct client {
    int fd;
    struct in_addr ipaddr;
    struct client *next;
};

static struct client *addclient(struct client *top, int fd, struct in_addr addr);
static struct client *removeclient(struct client *top, int fd);
int handleclient(struct client *p);

//server
void rcopy_server(unsigned short port){
    //setup the server
    struct sockaddr_in self;
    int listenfd, fd;
    

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket");
        exit(1);
    }

    self.sin_family = AF_INET;
    self.sin_addr.s_addr = INADDR_ANY;
    self.sin_port = htons(port);

    memset(&self.sin_zero, 0 ,sizeof(self.sin_zero));

    printf("Listening on %d\n", PORT);

    if (bind(listenfd, (struct sockaddr *)&self, sizeof(self)) == -1){
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5) == -1){
        perror("listen");
        exit(1);
    }

    struct sockaddr_in new_socket;
    socklen_t socklen;
    
    //setup for the select
    struct client *head = NULL;
    struct client *p;
    struct timeval tv;
    
	fd_set allset, rset;
	int nready;
	int maxfd = listenfd;
	 
	FD_ZERO(&allset);
	FD_SET(listenfd, &allset);
    
    int j;
    while (1) {
        socklen = sizeof(new_socket);
        rset = allset;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        
        nready = select(maxfd + 1, &rset, NULL, NULL, &tv);
        if (nready == 0) {
        	printf("No response from clients\n");
        	continue;
        }
        
        if (nready == -1) {
        	perror("select");
        	continue;
        }
        
        if(FD_ISSET(listenfd, &rset)) {
    		if ((fd = accept(listenfd, (struct sockaddr *)&new_socket, &socklen)) < 0) {
    			perror("accept");
    			exit(1);
    		}
    		FD_SET(fd, &allset);
    		
    		if(fd > maxfd) {
    			maxfd = fd;
    		}
    		head = addclient(head, fd, new_socket.sin_addr);
        }
        
        for(j = 0; j <= maxfd; j++) {
			if (FD_ISSET(j, &rset)) {
				for (p = head; p != NULL; p = p -> next) {
					if (p -> fd == j) {
						int result = handleclient(p);
						if (result == -1) {
							int tmp_fd = p -> fd;
							head = removeclient(head, p -> fd);
							FD_CLR(tmp_fd, &allset);
							close(tmp_fd);
						}
						break;
					}
				}				
			}       
        }
    }
} 


//client
char* parent_dir;
static int check = 0;
int rcopy_client(char *source, char *host, unsigned short port){
    int soc;
    struct sockaddr_in sock;
    struct hostent *he;

    if ((he=gethostbyname(host)) == NULL) {  /* get the host info  */
        perror("gethostbyname");
        exit(1);
    }

    if ((soc = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("rcopy_client: sock");
        exit(1);
    }

    sock.sin_family = AF_INET;
    sock.sin_port = htons(PORT);
    sock.sin_addr = *((struct in_addr *)he->h_addr);


    if (connect(soc, (struct sockaddr *)&sock, sizeof(sock)) == -1) {
        perror("rcopy_client: connect");
        exit(1);
    }

    FILE *sourcefile = fopen(source, "rb");

    if (sourcefile == NULL) {
        perror("no such file");
        exit(1);
    }

    struct request client_file;

    struct stat source_buf;
    if ((lstat(source, &source_buf)) == -1){
        perror("source");
        exit(1);
    }
    char parent_path[MAXPATH];
    realpath(source, parent_path);
    
    if (check == 0){
    	parent_dir = malloc(MAXPATH);
    	strcpy(parent_dir, basename(parent_path));
    	check += 1;
    }
    
    

    if (S_ISREG(source_buf.st_mode)){
        //create struct type
        client_file.type = REGFILE;
        //create struct path
        strcpy(client_file.path, strstr(source, parent_dir));
        printf("%s\n", client_file.path);
        //create struct mode
        client_file.mode = source_buf.st_mode;
        //create struct hash
        strncpy(client_file.hash, hash(sourcefile), BLOCKSIZE);
        //create struct size
        client_file.size = source_buf.st_size;


        //sending components of struct to server        
        int32_t signal_1 = htonl(AWAITING_TYPE);
        write(soc, &signal_1, sizeof(int32_t));

        //send type
        int32_t f_type = htonl(client_file.type);
        write(soc, &f_type, sizeof(f_type));

        int32_t signal_2 = htonl(AWAITING_PATH);
        write(soc, (char*)&signal_2, sizeof(int32_t));
        

        //send path
        int sent_bytes = write(soc, client_file.path, MAXPATH);
        printf("%d\n", sent_bytes);

        int32_t signal_3 = htonl(AWAITING_PERM);
        write(soc, &signal_3, sizeof(int32_t));

        //send mode
        int32_t mode = htonl(client_file.mode);
        write(soc, &mode, sizeof(int32_t));

        int32_t signal_4 = htonl(AWAITING_HASH);
        write(soc, &signal_4, sizeof(int32_t));

        //send hash
        write(soc, client_file.hash, BLOCKSIZE);

        int32_t signal_5 = htonl(AWAITING_SIZE);
        write(soc, &signal_5, sizeof(int32_t));

        //send size
        int32_t f_size = htonl(client_file.size);
        write(soc, &f_size, sizeof(f_size));

        //the signal from server
        int32_t sig_from_sock;
        read(soc, &sig_from_sock, sizeof(int32_t));
        int check_signal = ntohl(sig_from_sock);
        
        //check signal
        if (check_signal == SENDFILE){
            //fork a new process, create a new client
            int result;
            result = fork();

            if (result == -1){
                perror("fork");
                exit(1);
            } else if (result > 0){ // parent
                int status;
                if (wait(&status) != -1) {
                    printf("waiting\n");
                    if (WEXITSTATUS(status) == -1){
								printf("error in child process\n");
                    		return ERROR;
                    }
                } else {
                	  return ERROR;
                }

            } else if (result == 0){ // child
                //create a new client
    			int soc_fork;
    			struct sockaddr_in sock_fork;
    			struct hostent *he_fork;

    			 if ((he_fork=gethostbyname(host)) == NULL) {  /* get the host info  */
    				 perror("gethostbyname");
    				 exit(1);
    			 }

    			 if ((soc_fork = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    				 perror("rcopy_client: sock");
    				 exit(1);
    			 }

				 sock_fork.sin_family = AF_INET;
				 sock_fork.sin_port = htons(PORT);
				 sock_fork.sin_addr = *((struct in_addr *)he_fork->h_addr);


				 if (connect(soc_fork, (struct sockaddr *)&sock_fork, sizeof(sock_fork)) == -1) {
    				 perror("rcopy_client: connect");
    				 exit(1);
				 }
                //create struct type
    			 client_file.type = TRANSFILE;
    			 //create struct path
    			 //int path_size = strlen(basename(source));
    			 strcpy(client_file.path, strstr(source, parent_dir));
    			 //create struct mode
    			 client_file.mode = source_buf.st_mode;
    			 //create struct hash
    			 rewind(sourcefile);
    			 strcpy(client_file.hash, hash(sourcefile));
    			 //create struct size
    			 client_file.size = source_buf.st_size;
					 
				 //sending components of struct to server        
            	 int32_t signal_1 = htonl(AWAITING_TYPE);
            	 write(soc_fork, (char*)&signal_1, sizeof(int32_t));

    			 //send type
    			 int32_t f_type = htonl(client_file.type);
    			 write(soc_fork, (char*)&f_type, sizeof(f_type));

            	 int32_t signal_2 = htonl(AWAITING_PATH);
            	 write(soc_fork, (char*)&signal_2, sizeof(int32_t));

    			 //send path
    			 write(soc_fork, client_file.path, MAXPATH);

            	 int32_t signal_3 = htonl(AWAITING_PERM);
            	 write(soc_fork, (char*)&signal_3, sizeof(int32_t));

    			 //send mode
    			 write(soc_fork, &client_file.mode, sizeof(mode_t));

            	 int32_t signal_4 = htonl(AWAITING_HASH);
            	 write(soc_fork, (char*)&signal_4, sizeof(int32_t));

    			 //send hash
    			 write(soc_fork, client_file.hash, BLOCKSIZE);

            	 int32_t signal_5 = htonl(AWAITING_SIZE);
            	 write(soc_fork, (char*)&signal_5, sizeof(int32_t));

    		 	 //send size
    			 int32_t f_size = htonl(client_file.size);
    			 write(soc_fork, (char*)&f_size, sizeof(f_size));

                //TRANSFERING FILE 
                FILE *sourcefile = fopen(source, "rb");
                char buf;
                while(fread(&buf, 1, 1, sourcefile)){
                    write(soc_fork, &buf, 1);
                }
                if (fclose(sourcefile) < 0){
                    perror("fclose\n");
                    exit(1);
                }

                fclose(sourcefile);
                
                int32_t received_signal;
                read(soc_fork, &received_signal, sizeof(int32_t));
                if (ntohl(received_signal) == OK){
                	close(soc_fork);
                	exit(1);

                } else {
                	fprintf(stderr, "error transmitting data\n");
                	close(soc_fork);
                	exit(1);
                } 
            }
        } else if (check_signal == OK) {
            printf("same file already exist!\n");

        } else {
        	perror("error\n");
        	exit(1);
        }

    // if source is a directory
    } else if(S_ISDIR(source_buf.st_mode)){
    	  
        //create struct type
        client_file.type = REGDIR;
        //create struct path
        strcpy(client_file.path, strstr(source, parent_dir));
        //create struct mode
        client_file.mode = source_buf.st_mode;
        //the hash for a directory should be all null terminators
        char terminators[BLOCKSIZE] = {'\0'};
        strncpy(client_file.hash, terminators, BLOCKSIZE);
        //create struct size
        client_file.size = source_buf.st_size;

        //send signal and components of struct
		  //sending components of struct to server        
        int32_t signal_1 = htonl(AWAITING_TYPE);
        write(soc, &signal_1, sizeof(int32_t));

        //send type
        int32_t f_type = htonl(client_file.type);
        write(soc, &f_type, sizeof(f_type));

        int32_t signal_2 = htonl(AWAITING_PATH);
        write(soc, (char*)&signal_2, sizeof(int32_t));
        

        //send path
        write(soc, client_file.path, MAXPATH);

        int32_t signal_3 = htonl(AWAITING_PERM);
        write(soc, &signal_3, sizeof(int32_t));

        //send mode
        int32_t mode = htonl(client_file.mode);
        write(soc, &mode, sizeof(int32_t));

        int32_t signal_4 = htonl(AWAITING_HASH);
        write(soc, &signal_4, sizeof(int32_t));

        //send hash
        write(soc, client_file.hash, BLOCKSIZE);

        int32_t signal_5 = htonl(AWAITING_SIZE);
        write(soc, &signal_5, sizeof(int32_t));

        //send size
        int32_t f_size = htonl(client_file.size);
        write(soc, &f_size, sizeof(f_size));
        
        DIR *dir = opendir(source);
        if (dir == NULL) {
		  	perror("opendir");
		  	exit(1);
        }
        
        struct dirent *dp;
        dp = readdir(dir);
        while(dp != NULL) {
    		if (dp -> d_name[0] != '.'){
				char child_path[MAXPATH];
    			strcpy(child_path, parent_path);
    			strcat(child_path, "/");
    			strcat(child_path, dp -> d_name);
    			strcat(child_path, "\0");
				//recursively go through every element in the directory			
    			rcopy_client(child_path, host, port);
    		}
    		dp = readdir(dir);
        }
        closedir(dir);
    }
	close(soc);
	return 0;
}

//HELPER FOR SERVER
static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->next = top;
    top = p;
    return top;
}

static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
    return top;
}


int handleclient(struct client *p) {
	int AWAITING_STATE;
    //allocating space on stack since its use in helper
    struct request *received_file = malloc(sizeof(struct request));
  
    //to get the initial state
    int32_t state;
    read(p -> fd, &state, sizeof(int32_t));        
    AWAITING_STATE = ntohl(state);
    
    int j = 0;
    while (j < 5){

        if(AWAITING_STATE == AWAITING_TYPE){
            int32_t receive_type;
            read(p -> fd, &receive_type, sizeof(int32_t));
            received_file -> type = ntohl(receive_type);
            printf("type received\n");

        } else if (AWAITING_STATE == AWAITING_PATH){
            int received_bytes;
            received_bytes = read(p -> fd, received_file -> path, MAXPATH);                        
            received_file -> path[received_bytes] = '\0'; 
            printf("%s\n", received_file -> path);          

        } else if (AWAITING_STATE == AWAITING_PERM){
        	   int32_t mode;
            read(p -> fd, &mode, sizeof(int32_t));
            received_file -> mode = ntohl(mode);
            printf("permission received\n");

        } else if (AWAITING_STATE == AWAITING_HASH){
            int hash_read = read(p -> fd, received_file -> hash, BLOCKSIZE);
            
            if (hash_read != BLOCKSIZE) {
            	fprintf(stderr, "hash read incorrect");
            	return -1;
            }
            printf("%s\n", received_file -> hash);

        } else if (AWAITING_STATE == AWAITING_SIZE){
            int32_t receive_size;
            read(p -> fd, &receive_size, sizeof(int32_t));
            received_file -> size = ntohl(receive_size);
            printf("%d\n", received_file -> size);
        }
        //after running 4 times, exit the loop
        if(j == 4) {
            break;
        }
        //update AWAITING_STATE
    	int num_read = read(p -> fd, &state, sizeof(int32_t));   
    	if (num_read <= 0) {
    	  	perror("read");
    	  	return -1;
    	}     
        AWAITING_STATE = ntohl(state);
     
        j++;
    }
   
    //have to check if the transfered data is a file or directory
    if (received_file -> type == REGFILE) {
        //to check if the file exist
        if((access(received_file -> path, 0))){ //if there is no such file in dest
            int32_t backsignal = htonl(SENDFILE);
            write(p -> fd, (char *)&backsignal, sizeof(int32_t));

        } else {//there is already such file in sandbox/dest
            struct stat dest_buf;

            if ((lstat(received_file -> path, &dest_buf)) == -1){
                perror("dest");
                return -1;
            }   
            //if the files are of different type but same name
            if ((S_ISDIR(dest_buf.st_mode) && (received_file -> type == REGFILE)) || 
                ((S_ISREG(dest_buf.st_mode) && (received_file -> type == REGDIR)))) {
                int32_t backsignal = htonl(ERROR);
                write(p -> fd, (char * )&backsignal, sizeof(int32_t));
                printf("Already exits %s which is incompatible\n", basename(received_file -> path));
                exit(1);
            //check if the two files are the same
            } else {
                FILE *dest_file_check = fopen(received_file -> path, "rb");
                if (dest_file_check == NULL){
                    perror("fopen: dest_file_check");
                    return -1;
                }
                char * hash1 = hash(dest_file_check);
               
                fclose(dest_file_check);
                
                int hash_check = 0;
                int i = 0;

                while(i < BLOCKSIZE){
                    if (hash1[i] != received_file -> hash[i]){
                        hash_check = 1;
                    }   
                    i++;
                    
                }

                if ((dest_buf.st_size != received_file -> size) || (hash_check == 1) ){//two files are not the sames
                    int32_t backsignal = htonl(SENDFILE);
                    write(p -> fd, (char *)&backsignal, sizeof(int32_t));

                } else {//if both files are the same
                    
                    int32_t backsignal = htonl(OK);
                    write(p -> fd, (char *)&backsignal, sizeof(int32_t));
                }
            }
        }
    } else if (received_file -> type == TRANSFILE) {//receiving file
        //create and copy the file
            FILE *dest = fopen(received_file -> path, "wb");

            if (dest == NULL) {
                perror("fopen: dest");
                return -1;
            }

            char buf;
					
            while ((read(p -> fd, &buf, 1)) == 1){
                fwrite(&buf, 1, 1, dest);
            }
           
            if (fclose(dest) < 0){
                perror("fclose: dest");
                return -1;
            }
            
            //transmit the OK if the file is transferred successful
            
            int32_t back_signal = htonl(ERROR);
            write(p -> fd, &back_signal, sizeof(int32_t));

    } else if (received_file -> type == REGDIR) {
		if (access(received_file -> path, F_OK) == -1) {
			mkdir(received_file -> path, received_file -> mode & 0777);	 
		} else {
			struct stat check_dest_file;
			if(lstat(received_file -> path, &check_dest_file) == -1){
				perror("stat");
				return -1;
			}
			if(S_ISDIR(check_dest_file.st_mode) == 0) {
				fprintf(stderr, "incompatible type!");
				return -1;
			}
		}            			    
		chmod(received_file -> path, received_file -> mode);          
    }
    

    free(received_file);

    return 1;
}