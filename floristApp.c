/*  INCLUDES  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

/*  DEFINES  */
#define MAX_CLIENT	1024 
#define MAX_FLORIST	3 


/*  MUTEX  */
static pthread_mutex_t mtx[3] = {PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER};;


/* Struct for florists */
struct florists_t
{
	char name[20];
	int x, y;
	double speed;
	char flowers[MAX_FLORIST][20];
	int busy;
};

/* Struct for clients */
struct clients_t
{
	char name[20];
	int x, y;
	char flower[20];
};

/* Struct for central thread arguments */
struct central_args
{
	struct florists_t florists[MAX_FLORIST];
	struct clients_t clients[MAX_CLIENT];
	int numOfClients;
};

/* Struct for delivery thread arguments */
struct delivery_args
{
	struct florists_t florist;
	struct clients_t clients[MAX_CLIENT];
	int numOfClients;
	pthread_mutex_t mtx;
};

/* Signal handler function */
static void SingalHandler(int sig)
{
	printf("\n-- CTRL-C Catched --\n");
	exit(0);
}


/*  FUNTION PROTOTYPES  */
int getFromFile(char *filename, struct florists_t *fl, struct clients_t *cl);
int service_time(struct florists_t fl, struct clients_t cl);
static void *requestHandler(void *arg);
int bestOption(struct florists_t *fl, struct clients_t cl);
static void *makeDelivery(void *arg);


int main(int argc, char const *argv[])
{
	pthread_t t1;
	int s;

	struct florists_t florists[MAX_FLORIST];
	struct clients_t clients[MAX_CLIENT];
	struct central_args args1;

	int numOfClients;
	char filename[1024];

	// <SIGNAL HANDLING>
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = SingalHandler;
	if (sigaction(SIGINT, &sa, NULL) == -1)
	{
		perror("sigaction");
		return 1;
	}
	// </SIGNAL HANDLING>


	// <USAGE>
	if(argc != 2)
	{
		printf("USAGE: %s <filename>\n", argv[0]);
		exit(1);
	}
	// </USAGE>

	strcpy(filename,argv[1]);
	numOfClients = getFromFile(filename,florists,clients);

	srand(time(NULL)); //random init

	// <Fill thread arguments>
	for (int i = 0; i < numOfClients; ++i)
	{
		if(i < MAX_FLORIST)
			args1.florists[i] = florists[i];

		args1.clients[i] = clients[i];
	}
	args1.numOfClients = numOfClients;
	// </Fill thread arguments>

	// <Create thread>
	s = pthread_create(&t1, NULL, requestHandler, &args1);
	if (s != 0)
	{
		perror("pthread_create");
		exit(1);
	}
	// </Create thread>

	// <Thread terminate>
	pthread_join(t1,NULL);


	return 0;
}


/* This function read data in given filename and adds arrays */
int getFromFile(char *filename, struct florists_t *fl, struct clients_t *cl)
{
	FILE *file;
	size_t buffer_size = 80;
	char *buffer = malloc(buffer_size * sizeof(char));
	char c;
	int in1,in2;

	// <Fopen Error Check>
	if( (file = fopen(filename, "r")) == NULL )
	{
		perror("fopen");
		exit(1);
	}
	// </Fopen Error Check>

	// <Read florists data>
	for (int i = 0; i < MAX_FLORIST; ++i)
	{
		fscanf(file,"%s %c",buffer,&c);
		strcpy(fl[i].name,buffer);
		
		fscanf(file,"%d %c %d %c %lf %c %c",&fl[i].x,&c,&fl[i].y,&c,&fl[i].speed,&c,&c);
		
		fscanf(file,"%s",buffer);
		strtok(buffer,",");
		strcpy(fl[i].flowers[0],buffer);

		fscanf(file,"%s",buffer);
		strtok(buffer,",");
		strcpy(fl[i].flowers[1],buffer);

		fscanf(file,"%s",buffer);
		strcpy(fl[i].flowers[2],buffer);

		fl[i].busy = 0;
	}
	// </Read florists data>

	// <Read clients data>
	int i = 0;
	while(!feof(file))
	{
		fscanf(file,"%s %c",buffer,&c);
		strcpy(cl[i].name,buffer);

		fscanf(file,"%d %c %d %c %c",&cl[i].x,&c,&cl[i].y,&c,&c);

		fscanf(file,"%s",buffer);
		strcpy(cl[i].flower,buffer);

		i++;
	}
	// </Read clients data>

	fflush(stdout);

	free(buffer);
	fclose(file);

	printf("Florist application initializing from file: data.dat\n");

	return i;
}

/* This function calculates service time according to given coordinates and speed */
int service_time(struct florists_t fl, struct clients_t cl)
{
	// <Euclidean Distance>
	int difX = pow((fl.x - cl.x),2); 
	int difY = pow((fl.y - cl.y),2);
	// </Euclidean Distance>
	
	return sqrt(difX+difY)/fl.speed;
}

/* This function is the central thread that does request handling */
static void *requestHandler(void *arg)
{
	pthread_t t[MAX_FLORIST];
	int s;
	
	// <Incoming arguments>
	struct central_args *args = (struct central_args*)arg;
	int remainClients = args->numOfClients;

	// <Deliver threads arguments>
	struct clients_t requestQueue[MAX_FLORIST][MAX_CLIENT];
	int queueSize[MAX_FLORIST] = {0,0,0};
	struct delivery_args del_args[MAX_FLORIST];
	int best;

	// <Request handling according to bestOption>
	for (int i = 0; i < remainClients; ++i)
	{
		best = bestOption(args->florists,args->clients[i]);

		for (int j = 0; j < MAX_FLORIST; ++j)
		{
			if(best == j)
			{
				requestQueue[j][queueSize[j]] = args->clients[i];
				del_args[j].clients[queueSize[j]] = args->clients[i];
				queueSize[j]++;
			}
		}
	}
	// </Request handling according to bestOption>

	// <Filling delivery arguments>
	for (int i = 0; i < MAX_FLORIST; ++i)
	{
		del_args[i].florist = args->florists[i];
		del_args[i].numOfClients = queueSize[i];
	}
	// </Filling delivery arguments>	

	printf("%d florists have been created\n", MAX_FLORIST);
	printf("Processing requests\n");

	// <Creatind delivery threads>
	for (int i = 0; i < MAX_FLORIST; ++i)
	{
		del_args[i].mtx = mtx[i];
		s = pthread_create(&t[i], NULL, makeDelivery, &del_args[i]);
		if (s != 0)
		{
			perror("pthread_create");
			exit(1);
		}
	}
	// </Creatind delivery threads>

	// <Terminate threads and getting return values>
	void *res; //return value
	int totalTime[MAX_FLORIST];
	for (int i = 0; i < MAX_FLORIST; ++i)
	{
		pthread_join(t[i],&res);
		totalTime[i] = (int)res;
	}
	// </Terminate threads and getting return values>

	// <Print result informations>
	printf("All requests processed.\n");
	
	for (int i = 0; i < MAX_FLORIST; ++i)
	{
		printf("%s closing shop.\n", args->florists[i].name);
	}

	
	printf("Sale statistics for today:\n");
	printf("-------------------------------------------------\n");
	printf("Florist\t\t# of sales\tTotal time\n");
	printf("-------------------------------------------------\n");
	for (int i = 0; i < MAX_FLORIST; ++i)
	{
		printf("%s\t\t%d\t\t%dms \n", args->florists[i].name, queueSize[i], totalTime[i]);
	}
	printf("-------------------------------------------------\n");
	// </Print result informations>
}

/* This function calculates the best option for client */
int bestOption(struct florists_t *fl, struct clients_t cl)
{
	int time[MAX_FLORIST] = {-1,-1,-1};

	// <Compares offered service vs client request>
	for (int i = 0; i < MAX_FLORIST; ++i)
	{
		for (int j = 0; j < MAX_FLORIST; ++j)
		{
			if( strcmp(cl.flower,fl[i].flowers[j]) == 0 )
				time[i] = service_time(fl[i],cl);
		}
	}
	// </Compares offered service vs client request>

	int best = -1;
	int bestTime = -1;

	// <Finds best florist>
	for (int i = 0; i < MAX_FLORIST; ++i)
	{
		if(time[i] != -1)
			bestTime = time[i];
	}

	if(bestTime == -1)
		return -1;


	for (int i = 0; i < MAX_FLORIST; ++i)
	{
		if(time[i] <= bestTime && time[i] != -1)
		{
			best = i;
			bestTime = time[i];
		}
	}
	// </Finds best florist>

	return best; // index of fastest florist
}

/* This funciton makes deliveries from request queue for given florist */
static void *makeDelivery(void *arg)
{
	// <Incoming arguments>
	struct delivery_args *args = (struct delivery_args*)arg;
	int remainClients = args->numOfClients;
	
	int index = 0;
	int totalTime = 0;
	int currentServiceTime;
	int s;

	// <Makes requests from queue>
	while(remainClients != 0)
	{
		// <Mutex lock>
		s = pthread_mutex_lock(&args->mtx);
		if (s != 0)
		{
			perror("mutex_lock");
			exit(1);
		}
		// </Mutex lock>
		
		// <Calculate service time and increase total time>
		int prepareTime = ( rand() % 41 ) + 10;
		currentServiceTime = service_time(args->florist,args->clients[index]) + prepareTime;
		totalTime += currentServiceTime;
		usleep(currentServiceTime*1000); // if florist in delivery, next client waits
		// </Calculate service time and increase total time>
		
		// <Mutex unlock>
		s = pthread_mutex_unlock(&args->mtx);
		if (s != 0)
		{
			perror("mutex_unlock");
			exit(1);
		}
		// </Mutex unlock>

		printf("Florist %s has delivered a %s to %s in %dms\n", args->florist.name, args->clients[index].flower, args->clients[index].name, currentServiceTime);
		
		index++;
		remainClients--;
	}
	// </Makes requests from queue>

	return (void *) totalTime;
}
