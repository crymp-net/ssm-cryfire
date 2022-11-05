//================================================================================
// File:    Code/CryFire/BlockingQueue.h
//                 ____                       ____
// Project: SSM  /\  _ `\                    /\  _`\   __
//               \ \ \/\_\    _  __   __  __ \ \ \_/  /\_\    _  __     ___
//                \ \ \/_/_  /\`'__\ /\ \/\ \ \ \  _\ \/_/   /\`'__\  /' __`\
//                 \ \ \_\ \ \ \ \_/ \ \ \_\ \ \ \ \/   /\`\ \ \ \_/ /\  \__/
//                  \ \____/  \ \_\   \/`____ \ \ \_\   \ \_\ \ \_\  \ \_____\
//                   \/___/    \/_/    `/___/\ \ \/_/    \/_/  \/_/   \/____ /
//                                        /\___/
//                                        \/__/
// Created on:  24.6.2014
// Last edited: 27.6.2014
//--------------------------------------------------------------------------------
// Description: Queue for multi-threaded tasks based on producer-consumer style.
//              When queue is full, thread requesting push will be blocked until there is space.
//              When queue is empty, thread requesting pop will be blocked until there is item to pop.
//              Queue has fixed size specified in constructor and cannot be resized later
//--------------------------------------------------------------------------------
// Authors:     Patrick Glatt (HipHipHurra)
//              Jan Broz (Youda008)
//================================================================================


#ifndef BLOCKING_QUEUE_INCLUDED
#define BLOCKING_QUEUE_INCLUDED


#include <windows.h>
#include <climits>


typedef unsigned int uint;


//----------------------------------------------------------------------------------------------------
template<typename elem_t>
class FixedQueue {

  public:

	FixedQueue(uint length);
	~FixedQueue();

	       void           push(const elem_t & elem);
	       elem_t         pop();
	inline const elem_t & top() const;
	inline uint           count() const;

  protected:

	elem_t * _data;
	uint     _length;
	uint     _count;
	uint     _first;
	uint     _last;

};

//----------------------------------------------------------------------------------------------------
template<typename elem_t>
class BlockingQueue : public FixedQueue<elem_t> {

  public:

	BlockingQueue(uint length);
	~BlockingQueue();

	       void           push(const elem_t & elem);
	       elem_t         pop();
	inline const elem_t & top() const;
	inline uint           count() const;

  protected:

	HANDLE   _mutex;    // mutual exclusion lock for accessing the queue
	HANDLE   _free_sem; // semaphore indicating number of free places in the queue
	HANDLE   _used_sem; // semaphore indicating number of used places in the queue

};

//----------------------------------------------------------------------------------------------------
template<typename elem_t>
FixedQueue<elem_t>::FixedQueue(uint length) {

	_data = new elem_t [length];
	_length = length;
	_count  =  0;
	_first  =  0;
	_last   = -1;

}

//----------------------------------------------------------------------------------------------------
template<typename elem_t>
FixedQueue<elem_t>::~FixedQueue() {

	delete [] _data;

}

//----------------------------------------------------------------------------------------------------
template<typename elem_t>
void FixedQueue<elem_t>::push(const elem_t & elem) {

	if (++_last == _length)
		_last = 0;
	_data[_last] = elem;
	_count++;

}

//----------------------------------------------------------------------------------------------------
template<typename elem_t>
elem_t FixedQueue<elem_t>::pop() {

	uint first = _first;
	if (++_first == _length)
		_first = 0;
	_count--;
	return _data[first];

}

//----------------------------------------------------------------------------------------------------
template<typename elem_t>
const elem_t & FixedQueue<elem_t>::top() const {

	return _data[_first];

}

//----------------------------------------------------------------------------------------------------
template<typename elem_t>
uint FixedQueue<elem_t>::count() const {

	return _count;

}

//----------------------------------------------------------------------------------------------------
template<typename elem_t>
BlockingQueue<elem_t>::BlockingQueue(uint length)

 : FixedQueue(length) {

	_mutex    = CreateMutex(NULL, false, NULL);
	_free_sem = CreateSemaphore(NULL, _length, LONG_MAX, NULL);
	_used_sem = CreateSemaphore(NULL, _count,  LONG_MAX, NULL);

}

//----------------------------------------------------------------------------------------------------
template<typename elem_t>
BlockingQueue<elem_t>::~BlockingQueue() {

	CloseHandle(_mutex);
	CloseHandle(_free_sem);
	CloseHandle(_used_sem);

}

//----------------------------------------------------------------------------------------------------
template<typename elem_t>
void BlockingQueue<elem_t>::push(const elem_t & elem) {

	WaitForSingleObject(_free_sem, INFINITE);
	WaitForSingleObject(_mutex, INFINITE);
	FixedQueue::push(elem);
	ReleaseMutex(_mutex);
	ReleaseSemaphore(_used_sem, 1, NULL);

}

//----------------------------------------------------------------------------------------------------
template<typename elem_t>
elem_t BlockingQueue<elem_t>::pop() {

	WaitForSingleObject(_used_sem, INFINITE);
	WaitForSingleObject(_mutex, INFINITE);
	elem_t temp = FixedQueue::pop();
	ReleaseMutex(_mutex);
	ReleaseSemaphore(_free_sem, 1, NULL);
	return temp;

}

//----------------------------------------------------------------------------------------------------
template<typename elem_t>
const elem_t & BlockingQueue<elem_t>::top() const {

	WaitForSingleObject(_mutex, INFINITE);
	return FixedQueue::top();
	ReleaseMutex(_mutex);

}

//----------------------------------------------------------------------------------------------------
template<typename elem_t>
uint BlockingQueue<elem_t>::count() const {

	WaitForSingleObject(_mutex, INFINITE);
	return FixedQueue::count();
	ReleaseMutex(_mutex);

}

#endif // BLOCKING_QUEUE_INCLUDED
