#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define READ 0
#define WRITE 1
#define smallBuffer 20
#define MAXTOPICS 100
#define MAXENTRIES 100
#define inputError "Positive integers only."

//VARIABLES & STRUCTS
/*-------------------------------------------------------*/

int pubs, subs, tops;
int serverPid;
pthread_mutex_t mutex;

char pubMessageToChild[smallBuffer];
char pubMessageToParent[smallBuffer];
char subMessageToChild[smallBuffer];
char subMessageToParent[smallBuffer];

struct topicEntry {
	char pubID[smallBuffer];
	int topic;
	char article;
};

struct topicQueue {
	struct topicEntry entries[MAXENTRIES];
	struct topicEntry head;
	struct topicEntry tail;
	int length;
};

struct topicQueue topQueues[MAXTOPICS];

int topicHasEntry(int t) {
	if (topQueues[t].length > 0) {
		return 1;
	} else {
		return 0;
	}
}

void enqueue(int t, struct topicEntry ent) {
	if (topQueues[t].length < MAXENTRIES) {
		int index = topQueues[t].length;
		topQueues[t].entries[index] = ent;
		topQueues[t].length++;
	} else {
		printf("Topic queue full of entries");
	}
}

struct connectionRecord {
	char id[smallBuffer];
	char type[smallBuffer];
	int topic;
	int child[2];
	int parent[2];
};

void* publisher(void* record) {
	pthread_mutex_lock(&mutex);
	struct connectionRecord* pubRecord;
	pubRecord = (struct connectionRecord*) record;
	printf("%s publishes for topic %d\n", pubRecord->id, pubRecord->topic);
	struct topicEntry entry;
	strcpy(entry.pubID, pubRecord->id);
	entry.article = 'A' + (rand() % 26);
	entry.topic = pubRecord->topic;
	enqueue(pubRecord->topic, entry);
	printf("%s publishes topic %d article %c\n", pubRecord->id, pubRecord->topic, entry.article);
	printf("%s\n", "");
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
}

void* subscriber(void* record) {
	pthread_mutex_lock(&mutex);
	struct connectionRecord* subRecord;
	subRecord = (struct connectionRecord*) record;
	printf("%s interested in topic %d\n", subRecord->id, subRecord->topic);
	int n;
	if (topicHasEntry(subRecord->topic)) {
		for (n = 0; n < topQueues[subRecord->topic].length; n++) {
			printf("%s reads topic %d article %c\n", subRecord->id, subRecord->topic, topQueues[subRecord->topic].entries[n].article);
		}
	} else {
		printf("nothing published in topic %d\n", subRecord->topic);
	}
	printf("%s\n", "");
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
}

//VARIABLES & STRUCTS ^
/*-------------------------------------------------------*/

//MAIN
int main(int argc, char** argv) {

	srand(time(NULL));

//GETTING INPUT
/*-------------------------------------------------------*/

	printf("Number of publishers: ");
	if (scanf("%d", &pubs) == 1) {
		if (pubs < 1) {
			printf("%s\n", inputError);
			return 1;
		}
	} else { printf("%s\n", inputError); return 1; }

	printf("Number of subscribers: ");
	if (scanf("%d", &subs) == 1) {
		if (subs < 1) {
			printf("%s\n", inputError);
			return 1;
		}
	} else { printf("%s\n", inputError); return 1; }

	printf("Number of topics: ");
	if (scanf("%d", &tops) == 1) {
		if (tops < 1) {
			printf("%s\n", inputError);
			return 1;
		}
	} else { printf("%s\n", inputError); return 1; }

	printf("%s\n", "");
//GETTING INPUT ^
/*-------------------------------------------------------*/

pthread_mutex_init(&mutex, 0);

//INSTANTIATE STRUCTS/PIDS
struct connectionRecord pubRecords[pubs];
struct connectionRecord subRecords[subs];
int pubPid[pubs];
int subPid[subs];
pthread_t pubThreads[pubs];
pthread_t subThreads[subs];
int puberr;
int suberr;

//FORK SERVER & PUBS / SUBS
/*-------------------------------------------------------*/

	serverPid = fork();
	if (serverPid == -1) { perror("server fork"); return 1; }

	if (serverPid == 0) { //Server Process
		//printf("Server here. PID: %d\n", getpid());
		srand(time(NULL));

		int i;
		for (i = 0; i < pubs; i++) {
			if (pipe(pubRecords[i].child) == -1 || pipe(pubRecords[i].parent) == -1) {
				perror("pub piping");
				exit(EXIT_FAILURE);
			}
			pubPid[i] = fork();
			if (pubPid[i] == -1) { perror("pub fork"); return 1; }

			if (pubPid[i] == 0) { //Publisher Process
				write(pubRecords[i].child[WRITE], "pub connect", sizeof("pub connect"));
				while (read(pubRecords[i].parent[READ], pubMessageToChild, sizeof(pubMessageToChild))) {
					if (strcmp(pubMessageToChild, "pub accept") == 0) {
						write(pubRecords[i].child[WRITE], "pub request topic", sizeof("pub request topic"));
					}
					if (strcmp(pubMessageToChild, "pub topic assigned") == 0) {
						write(pubRecords[i].child[WRITE], "pub end", sizeof("pub end"));
						break;
					}
				}
				return;
			} else { //Server Process
				while (read(pubRecords[i].child[READ], pubMessageToParent, sizeof(pubMessageToParent))) {
					int prnd = rand() % tops + 1;
					if (strcmp(pubMessageToParent, "pub connect") == 0) {
						write(pubRecords[i].parent[WRITE], "pub accept", sizeof("pub accept"));
						sprintf(pubRecords[i].id, "pub%d", i+1);
						strcpy(pubRecords[i].type, "publisher");
					}
					if (strcmp(pubMessageToParent, "pub request topic") == 0) {
						pubRecords[i].topic = prnd;
						write(pubRecords[i].parent[WRITE], "pub topic assigned", sizeof("pub topic assigned"));
					}
					if (strcmp(pubMessageToParent, "pub end") == 0) {
						puberr = pthread_create(&pubThreads[i], NULL, publisher, (void*) &pubRecords[i]);
						if (puberr) {
							printf("pub thread error");
						} else {
							pthread_join(pubThreads[i], 0);
						}
						break;
					}
				}
			}
		}

		int j;
		for (j = 0; j < subs; j++) {
			if (pipe(subRecords[j].child) == -1 || pipe(subRecords[j].parent) == -1) {
				perror("sub piping");
				exit(EXIT_FAILURE);
			}
			subPid[j] = fork();
			if (subPid[j] == -1) { perror("sub fork"); return 1; }

			if (subPid[j] == 0) { //Subscriber Process
				write(subRecords[j].child[WRITE], "sub connect", sizeof("sub connect"));
				while (read(subRecords[j].parent[READ], subMessageToChild, sizeof(subMessageToChild))) {
					if (strcmp(subMessageToChild, "sub accept") == 0) {
						write(subRecords[j].child[WRITE], "sub request topic", sizeof("sub request topic"));
					}
					if (strcmp(subMessageToChild, "sub topic assigned") == 0) {
						write(subRecords[j].child[WRITE], "sub end", sizeof("sub end"));
						break;
					}
				}
				return;
			} else { //Server Process
				while (read(subRecords[j].child[READ], subMessageToParent, sizeof(subMessageToParent))) {
					int srnd = rand() % tops + 1;
					if (strcmp(subMessageToParent, "sub connect") == 0) {
						write(subRecords[j].parent[WRITE], "sub accept", sizeof("sub accept"));
						sprintf(subRecords[j].id, "sub%d", j+1);
						strcpy(subRecords[j].type, "subscriber");
					}
					if (strcmp(subMessageToParent, "sub request topic") == 0) {
					  subRecords[j].topic = srnd;
						write(subRecords[j].parent[WRITE], "sub topic assigned", sizeof("sub topic assigned"));
					}
					if (strcmp(subMessageToParent, "sub end") == 0) {
						suberr = pthread_create(&subThreads[j], NULL, subscriber, (void*) &subRecords[j]);
						if (suberr) {
							printf("sub thread error");
						} else {
							pthread_join(subThreads[j], 0);
						}
						break;
					}
				}
			}
		}

	} else {
		wait(NULL);
	}

//FORK SERVER & PUBS / SUBS ^
/*-------------------------------------------------------*/

	pthread_exit(NULL);
	return 0;

}
