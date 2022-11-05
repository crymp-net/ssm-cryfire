//================================================================================
// File:    Code/CryFire/AsyncTasks.cpp
//                 ____                       ____
// Project: SSM  /\  _ `\                    /\  _`\   __
//               \ \ \/\_\    _  __   __  __ \ \ \_/  /\_\    _  __     ___
//                \ \ \/_/_  /\`'__\ /\ \/\ \ \ \  _\ \/_/   /\`'__\  /' __`\
//                 \ \ \_\ \ \ \ \_/ \ \ \_\ \ \ \ \/   /\`\ \ \ \_/ /\  \__/
//                  \ \____/  \ \_\   \/`____ \ \ \_\   \ \_\ \ \_\  \ \_____\
//                   \/___/    \/_/    `/___/\ \ \/_/    \/_/  \/_/   \/____ /
//                                        /\___/
//                                        \/__/
// Created on:  15.11.2014
// Last edited: 16.11.2014
//--------------------------------------------------------------------------------
// Description: Provides a thread to perform tasks asynchronously
//--------------------------------------------------------------------------------
// Authors:     Patrick Glatt (HipHipHurra)
//              Jan Broz (Youda008)
//================================================================================


#include "StdAfx.h"

#include "AsyncTasks.h"

#include "BlockingQueue.h"
#include "CryFire/Logging.h"

#include <windows.h>


//----------------------------------------------------------------------------------------------------
HANDLE AsyncTasks::thread;
bool AsyncTasks::running = false;
BlockingQueue<AsyncTasks::Task> * AsyncTasks::tasks = NULL;
FixedQueue<AsyncTasks::Result> * AsyncTasks::results = NULL;

//----------------------------------------------------------------------------------------------------
AsyncTasks::Task::Task() {}
AsyncTasks::Task::Task(func_t task, void * arg, func_t handler, bool resImm)
 : asyncTask(task), arg(arg), resultHandler(handler), handleResImm(resImm) {}
AsyncTasks::Result::Result() {}
AsyncTasks::Result::Result(func_t handler, void * arg)
 : resultHandler(handler), arg(arg) {}

#define NULL_TASK Task(NULL,NULL,NULL,false)

//----------------------------------------------------------------------------------------------------
// this wrapper is here, because CreateThread does not accept static methods
//void threadFunc() { AsyncTasks::run(); }

//----------------------------------------------------------------------------------------------------
void AsyncTasks::initialize(uint queueSize)
{
	if (running) // already initialized
		return;

	// initialize queues for sending tasks to the new thread and receiving results
	tasks = new BlockingQueue<Task>(queueSize);
	results = new FixedQueue<Result>(queueSize);
	// create a thread
	CF_Log( 2, "starting thread for performing asynchronous tasks" );
	thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)run, NULL, 0, NULL);
	running = true;
}

//----------------------------------------------------------------------------------------------------
void AsyncTasks::terminate(bool force)
{
	if (!running) // already terminated or never started
		return;

	CF_Log( 2, "terminating thread for asynchronous tasks");
	running = false;
	if (force) {
		TerminateThread(thread, 0);
		delete tasks;
		delete results;
	} else {
		// if we don't want to terminate thread in the middle of its work,
		// we set a signal for it to not continue after it finishes what it's doing
		tasks->push(NULL_TASK); // we must put in a special item to wake up
		                        // the sleeping thread waiting for something in queue,
		                        // when it finds it, it wakes and quits
	}	
}

//----------------------------------------------------------------------------------------------------
void AsyncTasks::addTask(func_t asyncTask, void * arg, func_t resultHandler, bool handleResImm)
{
	if (!running) // if not running, queue is not even allocated
		return;

	tasks->push(Task(asyncTask, arg, resultHandler, handleResImm));
}

//----------------------------------------------------------------------------------------------------
void AsyncTasks::run()
{
	Task task;
	void * result;

	while (running) {

		task = tasks->pop(); // here the thread will sleep until queue is not empty
		if (task.asyncTask == NULL) // someone signaled the end, so break and quit;
			break;

		// perform the assigned task and get result
		result = task.asyncTask(task.arg);

		// if a result handler is specified, process the result
		if (task.resultHandler != NULL)
			if (task.handleResImm) // handle result immediately
				task.resultHandler(result);
			else // schedule result handling on next update
				results->push(Result(task.resultHandler, result));

	}

	delete tasks; // delete the queue here, because nobody else knows correctly, WHEN to delete it
	results->push(Result(NULL,NULL)); // also signal game loop, that result queue can be deleted
}

//----------------------------------------------------------------------------------------------------
void AsyncTasks::onUpdate(float frameTime)
{
	Result result;

	if (!running)
		return;

	// check if there are some results to handle
	if (results->count() > 0) { // pop only if queue is NOT empty, we don't want to fall asleep here
		result = results->pop();
		if (result.resultHandler == NULL) { // end was signaled, 
			delete results; // only now we can safely delete queue with results
			return;
		}
		result.resultHandler(result.arg);
	}
}
