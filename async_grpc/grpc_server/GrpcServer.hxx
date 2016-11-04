#ifndef _GRPC_SERVER_hxx
#define _GRPC_SERVER_hxx



#include <fstream>
#include <sstream>
#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpc++/grpc++.h>
#include <grpc/support/log.h>


using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;



class ServerCallMethod
{
public:
	enum CallStatus { CREATE, PROCESS, FINISH };

public:
	ServerCallMethod(const std::string & methodName):
		mMethodName(methodName)
	{}

	virtual ~ServerCallMethod(){}

	const std::string & callMethodName() { return mMethodName; }
	virtual CallStatus & callStatus() { return mStatus; }

protected:

private:
	std::string mMethodName;
	CallStatus mStatus;
};



template<typename TREQUEST, typename TREPLY>
class ServerCallData : public ServerCallMethod
{
public:
	ServerCallData(const std::string & methodName):ServerCallMethod(methodName), mResponder(&mContext)
	{

	}

	ServerContext & context() { return mContext; }
	TREQUEST & request() { return mRequest; }
	TREPLY & reply() { return mReply; }

	ServerAsyncResponseWriter<TREPLY> & responder() { return mResponder; }

private:

	ServerContext mContext;
	TREQUEST mRequest;
	TREPLY mReply;
	ServerAsyncResponseWriter<TREPLY> mResponder;
};



template<typename TSERVICE>
class GrpcServer
{
public:
	GrpcServer();
	virtual ~GrpcServer();

	void shutdown();
	bool isShutdown();
	bool run(const std::string & sslKey, const std::string & sslCert,
		const std::string & listenAddr, uint32_t threads = 1);

	virtual void ready() = 0;
	virtual void process(ServerCallMethod * cm) = 0;

protected:
	
	std::unique_ptr<ServerCompletionQueue> mCompletionQueue;
	std::unique_ptr<Server> mServer;
	TSERVICE mService;

private:

	// This can be run in multiple threads if needed.
	void handleRpcs();

	bool getFileContents(const std::string & fileName, std::string & contents);

private:

	bool mShutdown;
	std::list<std::shared_ptr<std::thread>> mThreads;
};


template<typename TSERVICE>
GrpcServer<TSERVICE>::GrpcServer()
	:mShutdown(true)
{

}


template<typename TSERVICE>
GrpcServer<TSERVICE>::~GrpcServer()
{
	shutdown();
	for (auto & it : mThreads)
	{
		it->join();
	}
}


template<typename TSERVICE>
void GrpcServer<TSERVICE>::shutdown()
{
	if (!mShutdown)
	{
		mServer->Shutdown();
		mCompletionQueue->Shutdown();

		mShutdown = true;
	}
}

template<typename TSERVICE>
bool GrpcServer<TSERVICE>::isShutdown()
{
	return mShutdown;
}


template<typename TSERVICE>
bool GrpcServer<TSERVICE>::run(const std::string & sslKey, const std::string & sslCert,
	const std::string & listenAddr, uint32_t threads /* = 1 */)
{
	if (listenAddr.empty() || threads == 0)
	{
		return false;
	}

	ServerBuilder builder;
	if (!sslCert.empty() && !sslKey.empty())
	{
		std::string keyContents;
		std::string certContents;
		getFileContents(sslCert, certContents);
		getFileContents(sslKey, keyContents);

		if (certContents.empty() || keyContents.empty())
		{
			return false;
		}

		grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp = { keyContents, certContents };
		grpc::SslServerCredentialsOptions ssl_opts;
		ssl_opts.pem_root_certs = "";
		ssl_opts.pem_key_cert_pairs.push_back(pkcp);

		builder.AddListeningPort(listenAddr, grpc::SslServerCredentials(ssl_opts));
	}
	else
	{
		builder.AddListeningPort(listenAddr, grpc::InsecureServerCredentials());
	}

	builder.RegisterService(&mService);
	mCompletionQueue = builder.AddCompletionQueue();
	mServer = builder.BuildAndStart();
	std::cout << "Server listening on " << listenAddr << std::endl;

	mShutdown = false;
	ready();

	for (int i = 0; i < threads; ++i)
	{
		std::shared_ptr<std::thread> t = std::shared_ptr<std::thread>(new std::thread(&GrpcServer<TSERVICE>::handleRpcs, this));
		mThreads.push_back(t);
	}

	return true;
}


template<typename TSERVICE>
bool GrpcServer<TSERVICE>::getFileContents(const std::string & fileName, std::string & contents)
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


template<typename TSERVICE>
void GrpcServer<TSERVICE>::handleRpcs()
{
	void* tag;
	bool ok = false;

	while (mCompletionQueue->Next(&tag, &ok))
	{
		if (!ok)
		{
			continue;
		}

		ServerCallMethod * cm = static_cast<ServerCallMethod *>(tag);
		process(cm);
	}
}


#endif
