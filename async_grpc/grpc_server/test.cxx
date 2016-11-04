#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpc++/grpc++.h>
#include <grpc/support/log.h>

#include "helloworld.grpc.pb.h"

#include "GrpcServer.hxx"

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using helloworld::HelloRequest;
using helloworld::HelloReply;
using helloworld::HiRequest;
using helloworld::HiReply;
using helloworld::Greeter;



template<typename TSERVICE>
class MyGrpcServer : public GrpcServer<TSERVICE>
{
public:

	MyGrpcServer()
	{}

	virtual ~MyGrpcServer() {}

	virtual void ready()
	{
		ServerCallData<HelloRequest, HelloReply> * helloCd = new ServerCallData<HelloRequest, HelloReply>("HelloRequest");
		helloCd->callStatus() = ServerCallMethod::PROCESS;
		mService.RequestSayHello(&(helloCd->context()), &(helloCd->request()), &(helloCd->responder()), mCompletionQueue.get(), mCompletionQueue.get(), helloCd);


		ServerCallData<HiRequest, HiReply> * HiCd = new ServerCallData<HiRequest, HiReply>("HiRequest");
		HiCd->callStatus() = ServerCallMethod::PROCESS;
		mService.RequestSayHi(&(HiCd->context()), &(HiCd->request()), &(HiCd->responder()), mCompletionQueue.get(), mCompletionQueue.get(), HiCd);
	}

	virtual void process(ServerCallMethod * cm)
	{
		ServerCallData<HelloRequest, HelloReply> * helloCd = dynamic_cast<ServerCallData<HelloRequest, HelloReply> *>(cm);
		ServerCallData<HiRequest, HiReply> * hiCd = dynamic_cast<ServerCallData<HiRequest, HiReply> *>(cm);
		if (cm->callStatus() == ServerCallMethod::PROCESS)
		{
			if (helloCd)
			{
				// Issue a new receive operation
				ServerCallData<HelloRequest, HelloReply> * newHelloCd = new ServerCallData<HelloRequest, HelloReply>("HelloRequest");
				newHelloCd->callStatus() = ServerCallMethod::PROCESS;
				mService.RequestSayHello(&(newHelloCd->context()), &(newHelloCd->request()), &(newHelloCd->responder()), mCompletionQueue.get(), mCompletionQueue.get(), newHelloCd);

				
				// The actual processing.
				helloCd->callStatus() = ServerCallMethod::FINISH;
				std::string prefix("Hello ");
				helloCd->reply().set_message(prefix + helloCd->request().name());
				helloCd->responder().Finish(helloCd->reply(), Status::OK, helloCd);
			}

			
			if (hiCd)
			{
				// Issue a new receive operation
				ServerCallData<HiRequest, HiReply> * newHiCd = new ServerCallData<HiRequest, HiReply>("HiRequest");
				newHiCd->callStatus() = ServerCallMethod::PROCESS;
				mService.RequestSayHi(&(newHiCd->context()), &(newHiCd->request()), &(newHiCd->responder()), mCompletionQueue.get(), mCompletionQueue.get(), newHiCd);


				// The actual processing.
				hiCd->callStatus() = ServerCallMethod::FINISH;
				std::string prefix("Hi ");
				hiCd->reply().set_message(prefix + hiCd->request().name());
				hiCd->responder().Finish(hiCd->reply(), Status::OK, hiCd);
			}
		}
		else if (cm->callStatus() == ServerCallMethod::FINISH)
		{
			delete helloCd;
			delete hiCd;
		}
		else
		{
			GPR_ASSERT(false);
		}
	}

protected:

private:


};



int main(int argc, char** argv)
{
	MyGrpcServer<Greeter::AsyncService> s1;
	std::string server_address1("192.168.0.28:50051");
	s1.run("../certs/key_test.com.pem",
		"../certs/cert_test.com.pem",
		server_address1, 
		4);



	MyGrpcServer<Greeter::AsyncService> s2;
	std::string server_address2("192.168.0.28:50052");
	s2.run("../certs/key_test.com.pem",
		"../certs/cert_test.com.pem",
		server_address2,
		4);

	std::cout << "\nPress any key to quit." << std::endl;
	std::cin.get();

	s1.shutdown();
	s2.shutdown();

	return 0;
}

