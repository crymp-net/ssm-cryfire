//================================================================================
// File:    Code/CryFire/AsyncTasks.h
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


#ifndef ASYNC_TASKS_INCLUDED
#define ASYNC_TASKS_INCLUDED


#include <windows.h>

#include "BlockingQueue.h"

#undef GetUserName       // windows.h defines some stupid macros, which overwrites Crysis methods names
#undef GetCommandLine


typedef unsigned int uint;
typedef void * (* func_t)(void *);

//----------------------------------------------------------------------------------------------------
class AsyncTasks {

  public:
  	
  	/* initializes queues for tasks and results and starts a new thread */
	static void initialize(uint queueSize);
	
	/* terminates threads and queues; if force = false, thread will be signaled
	   to quit after it finishes its current job, and queues deleted after that */
	static void terminate(bool force = false);
	
	/* adds a new task to the queue, thread will perform it as soon as it is free */
	static void addTask(func_t asyncTask, void * arg = NULL, func_t resultHandler = NULL, bool handleResImm = false);
	
	/* checks results of async operations and process them,
	   needs to be called regularly from game loop */
	static void onUpdate(float frameTime);
	
	/* PRIVATE!! This method has to be public because of implementation reasons,
	   but you shouldn't call it, if you do, program will freeze */
	static void run();


  protected:
  	
	static HANDLE  thread;
	static bool    running;
	struct Task {
		func_t asyncTask;
		void * arg;
		func_t resultHandler;
		bool   handleResImm;
		Task();
		Task(func_t task, void * arg, func_t handler, bool resImm);
  	};
	static BlockingQueue<Task> * tasks;
	struct Result {
		func_t resultHandler;
		void * arg;
		Result();
		Result(func_t handler, void * arg);
	};
	static FixedQueue<Result> * results;

};

#endif // ASYNC_TASKS_INCLUDED
