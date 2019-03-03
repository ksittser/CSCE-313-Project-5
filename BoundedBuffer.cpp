#include "BoundedBuffer.h"
#include <string>
#include <queue>
using namespace std;

BoundedBuffer::BoundedBuffer(int _cap) {
    cap=_cap;
	pthread_mutex_init(&m, NULL);
    pthread_cond_init(&nonempty, NULL);
    pthread_cond_init(&nonfull, NULL);
}

BoundedBuffer::~BoundedBuffer() {
	pthread_mutex_destroy(&m);
    pthread_cond_destroy(&nonempty);
    pthread_cond_destroy(&nonfull);
}

int BoundedBuffer::size() {
	return q.size();
}

int BoundedBuffer::capacity() {
    return cap;
}

void BoundedBuffer::push(string str) {
	/*
	Is this function thread-safe??? Does this automatically wait for the pop() to make room 
	when the buffer if full to capacity???
	*/
    pthread_mutex_lock(&m);
    while (q.size() >= cap)
        pthread_cond_wait(&nonfull, &m);
	q.push(str);
    pthread_cond_signal(&nonempty);
    pthread_mutex_unlock(&m);
}

string BoundedBuffer::pop() {
	/*
	Is this function thread-safe??? Does this automatically wait for the push() to make data available???
	*/
    pthread_mutex_lock(&m);
    while (q.size() <= 0)
        pthread_cond_wait(&nonempty, &m);
    string s = q.front();
	q.pop();
    pthread_cond_signal(&nonfull);
    pthread_mutex_unlock(&m);
	return s;
}
