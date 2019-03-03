/*
    Based on original assignment by: Dr. R. Bettati, PhD
    Department of Computer Science
    Texas A&M University
    Date  : 2013/01/31
 */


#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>

#include <sys/time.h>
#include <cassert>
#include <assert.h>

#include <cmath>
#include <numeric>
#include <algorithm>

#include <list>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "reqchannel.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
using namespace std;

struct ReqThreadArgs {
    int requests;  // number of requests for thread to make
    string request;  // string request that thread should make
    BoundedBuffer& reqBuffer;  // buffer to push to
    ReqThreadArgs(int reqs,string req,BoundedBuffer &reqBuff) : requests(reqs),request(req),reqBuffer(reqBuff) {}
};

struct WorkThreadArgs {
    BoundedBuffer& reqBuffer;  // buffer to pop from
    RequestChannel** reqChannels;
    BoundedBuffer* respBuffers;
    int w;
    int n;
    WorkThreadArgs(BoundedBuffer& reqBuff,RequestChannel** reqChan,BoundedBuffer* respBuffs,int ww,int nn) : reqBuffer(reqBuff),reqChannels(reqChan),respBuffers(respBuffs),w(ww),n(nn) {}
};

struct StatThreadArgs {
    BoundedBuffer& respBuffer;
    Histogram& histogram;
    string name;
    int responses;
    StatThreadArgs(string nm, int resps, BoundedBuffer& respBuff, Histogram& hist) : name(nm),responses(resps),respBuffer(respBuff),histogram(hist) {}
};

void* request_thread_function(void* arg) {
	/*
		Fill in this function.

		The loop body should require only a single line of code.
		The loop conditions should be somewhat intuitive.

		In both thread functions, the arg parameter
		will be used to pass parameters to the function.
		One of the parameters for the request thread
		function MUST be the name of the "patient" for whom
		the data requests are being pushed: you MAY NOT
		create 3 copies of this function, one for each "patient".
	 */

    ReqThreadArgs* rtas = (ReqThreadArgs*)arg;
	for (int i=0; i<rtas->requests; i++) {
        rtas->reqBuffer.push(rtas->request);
	}
}

void* worker_thread_function(void* arg) {
    /*
		Fill in this function. 

		Make sure it terminates only when, and not before,
		all the requests have been processed.

		Each thread must have its own dedicated
		RequestChannel. Make sure that if you
		construct a RequestChannel (or any object)
		using "new" that you "delete" it properly,
		and that you send a "quit" request for every
		RequestChannel you construct regardless of
		whether you used "new" for it.
     */

    WorkThreadArgs* wtas = (WorkThreadArgs*)arg;
    
    fd_set fdsOrig;
    FD_ZERO(&fdsOrig);
    string requests[wtas->w];
    int fdList[wtas->w];
    for (int i=0; i<wtas->w; i++) {
        fdList[i] = wtas->reqChannels[i]->read_fd();
        FD_SET(fdList[i],&fdsOrig);
    }
    for (int i=0; i<wtas->w; i++) {
        requests[i] = wtas->reqBuffer.pop();
        wtas->reqChannels[i]->cwrite(requests[i]);
    }
    int maxFd = wtas->reqChannels[0]->read_fd();
    for (int i=0; i<wtas->w; i++)
        if (wtas->reqChannels[i]->read_fd() > maxFd)
            maxFd = wtas->reqChannels[i]->read_fd();
    int numDone = 0;
    while (numDone<3*wtas->n) {
        fd_set fds = fdsOrig;
        select(maxFd+1,&fds,NULL,NULL,NULL);
        for (int i=0; i<wtas->w; i++) {
            if (FD_ISSET(fdList[i],&fds)) {
                string response = wtas->reqChannels[i]->cread();
                if (requests[i] == "data John Smith")
                    wtas->respBuffers[0].push(response);
                else if (requests[i] == "data Jane Smith")
                    wtas->respBuffers[1].push(response);
                else if (requests[i] == "data Joe Smith")
                    wtas->respBuffers[2].push(response);
                numDone++;
                requests[i] = wtas->reqBuffer.pop();
                wtas->reqChannels[i]->cwrite(requests[i]);
            }
        }
    }
}
  
void* stat_thread_function(void* arg) {
    /*
		Fill in this function. 

		There should 1 such thread for each person. Each stat thread 
        must consume from the respective statistics buffer and update
        the histogram. Since a thread only works on its own part of 
        histogram, does the Histogram class need to be thread-safe????

     */
     
    StatThreadArgs* stas = (StatThreadArgs*)arg;
    for (int i=0; i<stas->responses; i++) {
        string request = stas->name;
        string response = stas->respBuffer.pop();
        stas->histogram.update(request,response);
    }
}

Histogram hist;

void histDisplay(int s) {
    system("clear");
    hist.print();
    alarm(2);
    signal(SIGALRM, histDisplay);
}

/*--------------------------------------------------------------------------*/
/* MAIN FUNCTION */
/*--------------------------------------------------------------------------*/

int main(int argc, char * argv[]) {
    struct timespec start,end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    int n = 100; //default number of requests per "patient"
    int w = 1; //default number of worker threads
    int b = 30; // default capacity of the request buffer, you should change this default
    int opt = 0;
    
    alarm(2);
    signal(SIGALRM, histDisplay);
    
    while ((opt = getopt(argc, argv, "n:w:b:")) != -1) {
        switch (opt) {
            case 'n':
                n = atoi(optarg);
                break;
            case 'w':
                w = atoi(optarg); //This won't do a whole lot until you fill in the worker thread function
                break;
            case 'b':
                b = atoi (optarg);
                break;
		}
    }

    int pid = fork();
	if (pid == 0){
		execl("dataserver", (char*) NULL);
	}
	else {

        cout << "n == " << n << endl;
        cout << "w == " << w << endl;
        cout << "b == " << b << endl;

        RequestChannel *chan = new RequestChannel("control", RequestChannel::CLIENT_SIDE);
        BoundedBuffer requestBuffer(b);

        pthread_t reqThreadIds[3];  // thread ids in array
        string names[] = {"data John Smith", "data Jane Smith", "data Joe Smith"};
        
        RequestChannel* workerChannels[min(w,3*n)];
        pthread_t workThreadId;
        for (int i=0; i<min(w,3*n); i++) {
            chan->cwrite("newchannel");
            string s = chan->cread ();
            workerChannels[i] = new RequestChannel(s, RequestChannel::CLIENT_SIDE);
        }
        pthread_t statThreadIds[3];  // thread ids in array
        
        BoundedBuffer responseBuffers[] = {BoundedBuffer(ceil(b/3.0)),BoundedBuffer(ceil(b/3.0)),BoundedBuffer(ceil(b/3.0))};
        
        cout << "Creating request threads ... ";
        for (int i=0; i<3; i++)  // start all threads
            pthread_create (&reqThreadIds[i],0,request_thread_function,new ReqThreadArgs(n,names[i],requestBuffer));
        cout << "done" << endl;
        cout << "Creating worker thread ... ";
        pthread_create (&workThreadId,0,worker_thread_function,new WorkThreadArgs(requestBuffer,workerChannels,responseBuffers,min(w,3*n),n));
        cout << "done" << endl;
        cout << "Creating stat threads ... ";
        for (int i=0; i<3; i++)  // start all threads
            pthread_create (&statThreadIds[i],0,stat_thread_function,new StatThreadArgs(names[i],n,responseBuffers[i],hist));
        cout << "done" << endl;
        
        for (int i=0; i<3; i++)  // wait for request threads
            pthread_join (reqThreadIds[i],0);
        cout << "Done populating request buffer" << endl;
        
        cout << "Pushing quit requests ... ";
        for (int i=0; i<min(w,3*n); i++)
            requestBuffer.push("quit");
        cout << "done" << endl;
        
        pthread_join (workThreadId,0);  // wait for worker thread
        cout << "Done removing from request buffer" << endl;
        for (int i=0; i<3; i++)  // wait for stat threads
            pthread_join (statThreadIds[i],0);
        cout << "Done updating histogram" << endl;
        for (int i=0; i<min(w,3*n); i++)
            delete workerChannels[i];
        cout << "Done deleting worker channels" << endl;
        chan->cwrite ("quit");
        delete chan;
        cout << "All Done!!!" << endl; 
        
        histDisplay(SIGALRM);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    long musec = end.tv_sec*1000000+end.tv_nsec/1000 - (start.tv_sec*1000000+start.tv_nsec/1000);
    cout << "TIME ELAPSED: " << musec/1000000 << "s, " << musec%1000000 << "mus" << endl;
}
