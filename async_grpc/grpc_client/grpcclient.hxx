#ifndef _GRPC_CLIENT_hxx
#define _GRPC_CLIENT_hxx


#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>
#include <string>
#include <list>

#include <grpc++/grpc++.h>
#include <grpc/support/log.h>
#include <thread>

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;


class ClientCallMethod;

class callbackHandler
{
public:
	virtual void onMessage(ClientCallMethod * cm) = 0;

protected:

private:
};


class ClientCallMethod
{
public:
	ClientCallMethod(callbackHandler * handler, const std::string & methodName) :
		mCallbackHandler(handler), mMethodName(methodName)
	{}

	virtual ~ClientCallMethod() {}

	const std::string & callMethodName() { return mMethodName; }
	callbackHandler * cbHandler() { return mCallbackHandler; }

protected:

private:
	
	std::string mMethodName;
	callbackHandler * mCallbackHandler;
};



template<typename TREQUEST, typename TREPLY>
class ClientCallData : public ClientCallMethod
{
public:

	ClientCallData(callbackHandler * handler, const std::string & methodName, uint32_t delalineSeconds) :ClientCallMethod(handler, methodName)
	{
		std::chrono::system_clock::time_point deadline =
			std::chrono::system_clock::now() + std::chrono::seconds(delalineSeconds);

		mContext.set_deadline(deadline);
	}

	Status & rpcStatus() { return mRpcStatus; }

	TREPLY & reply() { return mReply; }
	ClientContext & context() { return mContext; }
	std::unique_ptr<ClientAsyncResponseReader<TREPLY>> & responderReader() { return mResponseReader; }

private:

	TREPLY mReply;
	ClientContext mContext;

	Status mRpcStatus;
	std::unique_ptr<ClientAsyncResponseReader<TREPLY>> mResponseReader;
};


class GrpcClient
{
public:
	GrpcClient() :mShutdown(true)
	{}
	~GrpcClient()
	{
		shutdown();
		for (auto & it : mThreads)
		{
			it->join();
		}
	}

	void shutdown()
	{
		if (!mShutdown)
		{
			mCompletionQueue.Shutdown();
			mShutdown = false;
		}
	}


	bool run(uint32_t threads)
	{
		if (threads == 0)
		{
			return false;
		}

		mShutdown = false;
		for (int i = 0; i < threads; ++i)
		{
			std::shared_ptr<std::thread> t = std::shared_ptr<std::thread>(new std::thread(&GrpcClient::asyncCompleteRpc, this));
			mThreads.push_back(t);
		}

		return true;
	}

	CompletionQueue & cq() { return mCompletionQueue; }


protected:

	CompletionQueue mCompletionQueue;

private:

	void asyncCompleteRpc()
	{
		void* got_tag;
		bool ok = false;
		while (mCompletionQueue.Next(&got_tag, &ok))
		{
			if (!ok)
			{
				continue;
			}

			ClientCallMethod * cm = static_cast<ClientCallMethod*>(got_tag);
			process(cm);
		}

/*
		void* got_tag;
		bool ok = false;

		for (;;) 
		{
			auto r =
				mCompletionQueue.AsyncNext(&got_tag, &ok, gpr_time_0(GPR_CLOCK_REALTIME));
			if (r == CompletionQueue::TIMEOUT) continue;
			if (r == CompletionQueue::GOT_EVENT)
			{
				ClientCallMethod * cm = static_cast<ClientCallMethod*>(got_tag);
				process(cm);
			}
			else
			{
				gpr_log(GPR_ERROR, "unexpected result from AsyncNext");
				abort();
			}
		}
*/

	}

	virtual void process(ClientCallMethod * cm)
	{
		callbackHandler * cb = cm->cbHandler();
		cb->onMessage(cm);
	}

private:

	bool mShutdown;
	std::list<std::shared_ptr<std::thread>> mThreads;
};




template<typename TSERVICE, typename TSERVICESUB>
class GrpcConnection : public callbackHandler
{
public:
	GrpcConnection(const std::string & serverAddr, uint32_t deadline, CompletionQueue * cq, const std::string & targetDomain, const std::string & sslCert) :
		mDeadline(deadline), mCompletionQueue(cq)
	{
		grpc::SslCredentialsOptions sslOpts;
		if (!sslCert.empty())
		{	
			if (getFileContents(sslCert, sslOpts.pem_root_certs))
			{
				grpc::ChannelArguments channelArgs;
				channelArgs.SetSslTargetNameOverride(targetDomain);
				mStub = TSERVICE::NewStub(grpc::CreateCustomChannel(
					serverAddr, grpc::SslCredentials(sslOpts), channelArgs));
			}
			else
			{
				GPR_ASSERT(0);
			}
		}
		else
		{
			mStub = TSERVICE::NewStub(grpc::CreateChannel(
				serverAddr, grpc::InsecureChannelCredentials()));
		}
	}



protected:

	uint32_t mDeadline;
	CompletionQueue * mCompletionQueue;
	std::unique_ptr<TSERVICESUB> mStub;

private:

	bool getFileContents(const std::string & fileName, std::string & contents)
	{
		try
		{
			std::ifstream in(fileName.c_str(), std::ios::in);
			if (in)
			{
				std::ostringstream t;
				t << in.rdbuf();
				in.close();

				contents = t.str();
				return true;
			}
		}
		catch (...)
		{

		}

		return false;
	}

};



#endif

