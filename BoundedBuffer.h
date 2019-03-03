#ifndef BoundedBuffer_h
#define BoundedBuffer_h

#include <stdio.h>
#include <queue>
#include <string>
using namespace std;

class BoundedBuffer {
private:
	queue<string> q;
    pthread_mutex_t m;
    pthread_cond_t nonfull;
    pthread_cond_t nonempty;
    int cap;
public:
    BoundedBuffer(int);
	~BoundedBuffer();
	int size();
    int capacity();
    void push (string);
    string pop();
};

#endif /* BoundedBuffer_ */
