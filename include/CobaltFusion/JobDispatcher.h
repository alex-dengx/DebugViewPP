// (C) Copyright Gert-Jan de Vos and Jan Wilmans 2013.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)

// Repository at: https://github.com/djeedjay/DebugViewPP/

#pragma once

#include <memory>
#include <string>
#include <deque>
#include <boost/noncopyable.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>

#pragma comment(lib, "CobaltFusion.lib")

namespace fusion {

// Two use cases are supported by JobDispatcher
// 
// 1) BackgroundDispatcher
//    any Queue'd jobs by the caller will then be executed on a background thread until stop is called.
//
// 2) OnDemandDispatcher
//	  call ExecuteQueuedJobs() to have the queued jobs executed on the caller thread
//     
	
typedef std::function<void()> Job;

class JobDispatcher
{
public:
	JobDispatcher();
	virtual ~JobDispatcher();

	// guarentees and all jobs queued before Flush() have been executed 
	virtual void Flush();

	// returns immediately after queueing
	virtual void Queue(Job job);

	virtual boost::signals2::connection SubscribeToExceptionEvent(std::function<void(std::exception_ptr)> function);

protected:
	bool HasJobs();
	void ClearJobs();
	Job NextJob();

	bool m_stop;
	std::deque<Job> m_jobQueue;
	boost::mutex m_executionMutex;
	boost::mutex m_stopMutex;
	boost::mutex m_queueMutex;
	boost::condition_variable m_condition;
	std::unique_ptr<boost::thread> m_thread;
	boost::signals2::signal<void(std::exception_ptr)> m_exceptionOccurredSignal;
};

class BackgroundDispatcher : public JobDispatcher
{
public:
	BackgroundDispatcher();
	virtual ~BackgroundDispatcher();

	// Run() returns after starting a thread that blocks waiting for jobs to execute until Stop is called
	void Run();
	void Stop();

private:
	std::unique_ptr<boost::thread> m_thread;

};

class OnDemandDispatcher : public JobDispatcher
{
public:
	OnDemandDispatcher();
	virtual ~OnDemandDispatcher();

	virtual void Queue(Job job);

	// check for jobs and execute all waiting jobs on the calling thread
	void ExecuteQueuedJobs();

	virtual boost::signals2::connection SubscribeToJobQueuedEvent(std::function<void()> function);

private:
	boost::signals2::signal<void()> m_jobQueuedSignal;
};

} // namespace fusion
