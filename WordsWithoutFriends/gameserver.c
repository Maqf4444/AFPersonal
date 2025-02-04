#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>

#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#define MAX_LEN_WORD 30
#define MIN_RANDOM_WORDLEN 6


#define SHORT_BUFFER_LEN 1024
#define FILE_BUFFER_LEN (1024 * 1024)
#define STR_BUFFER_LEN (1024 * 16)
#define STR_SERVER_PORT_NUMBER "8000"

const char *strRootDirectory = NULL; // this will be set in main 


// can be reset later when new game is started so need to be global so that main thread can access the changed value. 
char *gMasterWord = NULL; 	
struct gameListNode* gMasterList = NULL;

struct wordListNode { 
	char word[MAX_LEN_WORD + 1];
	struct wordListNode *next;
} ; 

struct gameListNode {
	char word[MAX_LEN_WORD + 1];
	bool found;
	struct gameListNode *next;
} ; 


struct threadArgStruct {
    struct gameListNode *gln;
    char *masterWord;
    int fd;
};

// global dictionary linked list
struct wordListNode *gRootNode = NULL;

// total number of dictionary words
int gNumWords = 0;

int randomInt(int n) {

    int r= rand();

	printf("In randomInt(): returning random number = %d\n\n", r%n);
    return r % n;
}


struct wordListNode *getRandomWord(int nWords) { 
	
	int r = randomInt(nWords);

	struct wordListNode *curr = gRootNode; 
	
	int i = 0;
	while( curr != NULL) { 
		if( i == r ) { 
			break;
		}
		curr = curr->next;
		i++;
	}

	if( curr != NULL ) { 
		while( strlen( curr->word) < MIN_RANDOM_WORDLEN ) {
			printf("In getRandomWord: curr->word is '%s' less than 6 letter long. Going to next\n", curr->word);
			curr = curr->next;
		}
	}

	printf("In getRandomWord(): Returning curr = %p curr->word = %s\n\n", curr, curr->word);
	return curr;
}

void toUpper(char *str) {
	while ( (*str = toupper(*str) ))
   		++str;
}

int readFile(char *filename) {
    char dic[MAX_LEN_WORD];
    FILE *fp;   


    fp = fopen(filename, "r");  

	if( fp == NULL) { 
		printf("In readFile(): Could not read file %s\n", filename ) ;
		return -1;
	}
	
	int nWords = 0, maxLen = 0;
	
	struct wordListNode *prev = NULL;
	while(fscanf(fp, "%s", dic) != EOF){
	
		struct wordListNode *curr = NULL;
		toUpper(dic);

		if( gRootNode == NULL) { 
				gRootNode = (struct wordListNode *) malloc( sizeof(struct wordListNode));					
				gRootNode ->next = NULL;
				curr = gRootNode;
        } else { 
				curr = (struct wordListNode *) malloc( sizeof( struct wordListNode));					
				curr ->next = NULL;
				prev->next = curr;
		}
		
		strcpy( curr->word, dic);

		prev = curr;

		int len = strlen(dic);
		if( len > maxLen) { 
			maxLen = len;
		}
		nWords++;
    } 

	printf("From file %s, read %d words. Longest word length: %d\n\n", filename, nWords, maxLen);

	fclose(fp);
	return nWords;
}

bool compareCounts(int lettersOfTheAlphabet[26], int wordBank[26]) {
    for (int i = 0; i < 26; i++) {
        if (lettersOfTheAlphabet[i] > wordBank[i] || (lettersOfTheAlphabet[i] > 0 && wordBank[i] == 0)) {
            return false;
        }
    }
    return true;
}

bool isDone(struct gameListNode* masterList) {
/*	printf("Do you want to continue. Press Y/y for Yes and N/n if you want to exit.");
	char choice[2];
	scanf("%1s", choice);
	
	int ch;
	while ((ch = getchar()) != '\n' && ch != EOF)
		continue;  	

	char c = choice[0];
	if( c == 'Y' || c == 'y') {
		return false;
	}

	if( c == 'N' || c == 'n') {
	    return true;
	}
*/

	struct gameListNode *curr = masterList;

	bool atLeastOneMissing = false;

	int nTotal = 0, nMissing = 0;
	while( curr != NULL) {
		if( curr->found == false) {
			atLeastOneMissing = true;
			nMissing++;
		}
		nTotal++;

		curr= curr->next;
	}

	printf("in isDone: Total words: %d Matched: %d Missing: %d\n\n", nTotal, nTotal - nMissing, nMissing);
	return !atLeastOneMissing;
}


void getLetterDistribution(const char* word, int numbersOfLetters[26]) {
    int i = 0;

    while (word[i] != '\0') {
        if (word[i] >= 'A' && word[i] <= 'Z') {
            numbersOfLetters[word[i] - 'A']++;
        }
        i++;
    }
}



struct gameListNode *findWords(char *masterWord) {
	printf("In findWords\n");

	struct wordListNode *curr = gRootNode; 

	int masterDistribution[26] = {0};
	getLetterDistribution(masterWord, masterDistribution);


	struct gameListNode *gameList = NULL, *currGameList = NULL; 


	//printDistribution(masterDistribution);

	printf( "master word is : %s\n\n", masterWord);

	int nGameListNodes = 0;
	while( curr != NULL) { 
		int currDistribution[26] = {0};
		getLetterDistribution(curr->word, currDistribution);
		//printDistribution(currDistribution);


		//printf("Words '%s': ", curr->word);
		if (compareCounts(currDistribution, masterDistribution)) {
			if( gameList == NULL) {
				currGameList = gameList = (struct gameListNode*) malloc(sizeof( struct gameListNode));
				strcpy(currGameList->word, curr->word);
				currGameList->found = false;
				currGameList->next = NULL;	
			} else {
				currGameList->next = (struct gameListNode*) malloc(sizeof( struct gameListNode));
				currGameList = currGameList->next;
				strcpy(currGameList->word, curr->word);
				currGameList->found = false;
				currGameList->next = NULL;		
			}
		
			nGameListNodes++;
			//printf("The word '%s' matches.\n", curr->word);
		} else {
			//printf("no match.\n");
		}

		curr = curr->next;
	}
	printf("Total words added to game list = %d\n\n", nGameListNodes);
	return gameList;
}


int initialization() {
	printf("In initialization()\n");
    srand(time(NULL));

	gNumWords = readFile("2of12.txt");

	struct wordListNode * masterWordNode = NULL;

	// remain in for loop till we get a random word. sometimes getRandomWord will return NULL when it reaches to end of dictionary, 
    // in that case call it again till it returns a valid word.  
	// 'Ensure that you handle the case that you get to the end of the dictionary without finding a long enough word."
    for ( masterWordNode = getRandomWord(gNumWords); masterWordNode == NULL; )
		;
	
	gMasterWord = masterWordNode->word; 	
	gMasterList = findWords(gMasterWord);
	printf("in initialization: reset gMasterList. gMasterWord = %s\n", gMasterWord);
	return gNumWords;
}

void cleanUpGameListNodes(struct gameListNode *gameList) {
	struct gameListNode *curr = gameList;

	while( curr != NULL) {
		struct gameListNode *next = curr->next;
		free( curr);
		curr = next;
	}
	gMasterList = NULL;
}

void cleanUpWordListNodes() { 
	struct wordListNode *curr = gRootNode;

	while( curr != NULL) {
		struct wordListNode *next = curr->next;
		free( curr);
		curr = next;
	}

	gRootNode = NULL;

}

void teardown() {
	printf("\n In teardown\n");
	cleanUpWordListNodes();
    printf("Finished teardown.");
}


void	printDistribution(int *masterDistribution) {
	printf("Printing distribution: ");
	for( int i = 0; i < 26; i++ ) {
		printf("%d ", masterDistribution[i]);
	}
	printf("\n\n");
}


bool acceptInput(struct gameListNode* masterList, const char *wordGuess) {

	if( !strcmp(wordGuess, "exitgame")) {
		return false;
	}


	char capitalizedWordGuess[MAX_LEN_WORD+1];

    int i = 0;
    while (i < MAX_LEN_WORD && wordGuess[i] != '\0') {
        capitalizedWordGuess[i] = toupper(wordGuess[i]);
    	i++;
	}

	if( i > MAX_LEN_WORD ) {
		printf("in acceptInput: error: word is longer than %d\n", MAX_LEN_WORD);
		return false;
	}

	capitalizedWordGuess[i] = 0;

	//printf("%d", wordGuess[i-1]);

    printf("in acceptInput: capitalizedWordGuess is: %s\n", capitalizedWordGuess);

	struct gameListNode *curr = masterList; 

	while( curr != NULL) {
		//printf("Comparing |%s| with |%s|\n", curr->word, wordGuess);
		if( !strcmp( curr->word, capitalizedWordGuess)) {
			printf("                    Found word %s\n", capitalizedWordGuess);
			curr->found = true;
		} else {
			//curr->found = false;
		}
		curr = curr->next;
	}

	return true;
}

char* displayWorld(char* masterWord, struct gameListNode* masterList) {
	
    printf("In displayWorld\n");
    if( masterList == NULL) {
		printf("in displayWorld: masterList is NULL. Error");
		return "<p>NULL";
	}

    char *retStr = (char *) malloc( STR_BUFFER_LEN);
    int lenStr = 0;

    lenStr += snprintf(retStr, STR_BUFFER_LEN, "%s", "<p>------------------------------------------------------------------------------------------------------------------------<p><p>");
	
    printf("in displayWorld: Got master Word = %s\n", masterWord);

	int nTotal = 0, nMissing = 0;
	lenStr += snprintf(retStr + lenStr, STR_BUFFER_LEN - lenStr, "<table>");

	for( struct gameListNode* curr = masterList; curr != NULL; ) {
		lenStr += snprintf(retStr + lenStr, STR_BUFFER_LEN - lenStr, "<tr>");
		int i = 0;
		for( ; i < 3 && curr != NULL; curr = curr->next, i++) {	
			if( curr->found) {

				lenStr += snprintf(retStr + lenStr, STR_BUFFER_LEN - lenStr, "<td>%d. FOUND: %s</td>", nTotal +1, curr->word);

			} else {
				//printf("_________%s\n", curr->word);

				lenStr += snprintf(retStr + lenStr, STR_BUFFER_LEN - lenStr,  "<td>%d. _________</td>", nTotal+1);
				nMissing++;
			}
			nTotal++;
	
		}
		if( i < 3) {
			for( int j = i; j < 3; j++) {
				lenStr += snprintf(retStr + lenStr, STR_BUFFER_LEN - lenStr, "<td>-</td>" );

			}
		}

		lenStr += snprintf(retStr + lenStr, STR_BUFFER_LEN - lenStr, "</tr>");	
	
	}
	lenStr += snprintf(retStr + lenStr, STR_BUFFER_LEN - lenStr, "</table>");

	lenStr += snprintf(retStr + lenStr, STR_BUFFER_LEN - lenStr, "Total words: %d Words not yet found: %d<p><p>", nTotal, nMissing);
    //printf("retStr = %s strlen(retStr) = %ld \n lenStr = %d\n", retStr, strlen(retStr),  lenStr);
	return retStr;
}

// reads file contents and creates response string. returns 404 if file does not exist.
void createResponseString(const char *strMove, char *strResponse, size_t *ptrResponseLen, struct threadArgStruct *pThreadArg) {
    const char * const response_header = "HTTP/1.1 200 OK\r\nContent-Length: ";

    char *masterWord = pThreadArg->masterWord;
    printf("In createResponseString\n");

    if( !strncmp(strMove, ".//words", 8)) {
        printf("In createResponseString words\n");

        //*ptrResponseLen = snprintf(strResponse, FILE_BUFFER_LEN, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n Move Request");    
        if( !strncmp(strMove, ".//words?move=", 14)) {
            printf("In createResponseString words > 9. calling displayWorld()\n" );
            const char *move = &strMove[14];
            printf("move = %s\n", move);

			// to help with debuggin
			if( !strcmp(move, "exitgame")) {
				cleanUpGameListNodes(pThreadArg->gln);
				teardown();
				exit(1);
			}

			acceptInput(pThreadArg->gln, move);

            char *strRet = displayWorld(masterWord, pThreadArg->gln);
            *ptrResponseLen += snprintf(strResponse, 
                FILE_BUFFER_LEN, 
                "%s%ld\r\n\r\n<html><body>%s<form submit=\"words\"><input type=\"text\" name=move autofocus > </input></form><p> <a href=\"%s\">Start a new Game></a><body></html>", 
                response_header, 
                strlen(strRet) +140 , 
                strRet,
				"./words");

            //printf("strResponse = %s \n strlen(strResponse) = %ld\n", strResponse, strlen(strResponse));
            printf("\n strlen(strResponse) = %ld\n", strlen(strResponse));

			free(strRet);
        } else {
			// start a new game 
			cleanUpGameListNodes(pThreadArg->gln);
			teardown();
				
			initialization();

            char *strRet = displayWorld(masterWord, pThreadArg->gln);
            *ptrResponseLen += snprintf(strResponse, 
                FILE_BUFFER_LEN, 
                "%s%ld\r\n\r\n<html><body>Started a new game <p> %s<form submit=\"words\"><input type=\"text\" name=move autofocus > </input></form><p> <a href=\"%s\">Start a new Game></a><body></html>", 
                response_header, 
                strlen(strRet) +160 , 
                strRet,
				"./words");

            printf("In createResponseString no ./words?move=. Started a new game." );
			free(strRet);
        }
    }
    else {
        printf("In createResponseString normal file strMove = %s\n", strMove );
        // serve normal file
        int fd = open(strMove, O_RDONLY);
        
        // in case file does not exist, then send 404 Not Found
        if (fd == -1 ) {
            printf("In createResponseString file does not exist\n" );        
            *ptrResponseLen = snprintf(strResponse, FILE_BUFFER_LEN, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n404 Not Found");
        }
        else {                              // if file exists, then send contents of file
            printf("In createResponseString serving file\n" );        
            struct stat st; 
            stat(strMove, &st);
            *ptrResponseLen += snprintf(strResponse, FILE_BUFFER_LEN, "%s%ld\r\n\r\n", response_header, st.st_size);

            ssize_t bytes_read;
            while ((bytes_read = read(fd, strResponse + *ptrResponseLen, FILE_BUFFER_LEN - *ptrResponseLen)) > 0)
                *ptrResponseLen += bytes_read;
            close(fd);
        }
    }
}

// for one client connection, this functions is invoked in a separate thread. 
void *requestHandlerFunction(void *arg) {

    struct threadArgStruct *pThreadArg = (struct threadArgStruct *) arg;  

    int fd_socket = pThreadArg->fd;


    char strRequest[SHORT_BUFFER_LEN], strMove[SHORT_BUFFER_LEN];
    
    // read and analyze request from client
    ssize_t nRequestBytes = recv(fd_socket, strRequest, SHORT_BUFFER_LEN, 0);
    const int GET_LEN = 5;
    if( nRequestBytes > GET_LEN && !strncmp("GET /", strRequest, GET_LEN)) {
        int i = GET_LEN;
        while( *(strRequest + i++) != ' ');
        strRequest[i-1] = '\0';

        // create strResponse
        char *strResponse = (char *)malloc(FILE_BUFFER_LEN * 2 * sizeof(char));
        size_t nResponseLen = 0;

        snprintf( strMove, 1024, "%s/%s", strRootDirectory, strRequest + GET_LEN);
        printf("We got the following request: %s\n", strMove);

        createResponseString(strMove, strResponse, &nResponseLen, pThreadArg);

        printf("Going to send response. nResponseLen = %ld\n", nResponseLen);

        // send strResponse back to client and then free it
        int sentBytes = send(fd_socket, strResponse, nResponseLen, 0);
        
        printf("Sent: %d\n", sentBytes);
        
        free(strResponse);
    }

    close(fd_socket);
    free(arg);


    return NULL;
}

// main function: takes the root file directory as first argument, from which the files are served by the server
int main(int argc, char *argv[]) {
    if( argc < 2) {
        printf("Usage: <%s> <root file directory> <optional port number>", argv[0]);
        return -1;
    }

	const char *str_port_number = STR_SERVER_PORT_NUMBER;
	if( argc == 3) {
		str_port_number  = (const char *) argv[2];
	}

    initialization();




    strRootDirectory = argv[1];
    printf("Going to use root directory to serve files: %s\n", strRootDirectory);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    getaddrinfo(NULL, str_port_number, &hints, &res);
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    // bind socket to SERVER_PORT_NUMBER
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind failed");
        return -1;
    }

    // listen for connections
    if (listen(sockfd, 10) < 0) {
        perror("listen failed");
        return -1;
    }

    printf("Server listening on port %s on localhost. Access it using localhost:%s/<filename>\n", str_port_number, str_port_number);
    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int fdClient;

        // accept client connection
        if ((fdClient = accept(sockfd, (struct sockaddr *)&clientAddr, &clientAddrLen)) < 0) {
            perror("accept failed");
            continue;
        }

        // create a new thread to handle client request
        pthread_t thread_id;

        struct threadArgStruct *pThreadArg = (struct threadArgStruct *) malloc( sizeof( struct threadArgStruct)) ; 
        pThreadArg->fd = fdClient;
        pThreadArg->gln = gMasterList;
        pThreadArg->masterWord = gMasterWord;

        pthread_create(&thread_id, NULL, requestHandlerFunction, (void *)pThreadArg);
        pthread_detach(thread_id);
    }


    teardown();

    close(sockfd);
    return 0;
}
