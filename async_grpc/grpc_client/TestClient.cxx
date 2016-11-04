#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpc++/grpc++.h>
#include <grpc/support/log.h>

#include "helloworld.grpc.pb.h"

#include "grpcclient.hxx"

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


template<typename TSERVICE, typename TSERVICESUB>
class MyConnection : public GrpcConnection<TSERVICE, TSERVICESUB>
{
public:
	MyConnection(const std::string & serverAddr, uint32_t deadline, CompletionQueue * cq, 
		const std::string & targetDomain, const std::string & sslCert):
		GrpcConnection(serverAddr, deadline, cq, targetDomain, sslCert)
	{

	}


	void sayHello(const std::string& user)
	{
		HelloRequest request;
		request.set_name(user);

		ClientCallData<HelloRequest, HelloReply> * call = new ClientCallData<HelloRequest, HelloReply>(this, "HelloRequest", mDeadline);

		call->responderReader() = mStub->AsyncSayHello(&call->context(), request, mCompletionQueue);
		call->responderReader()->Finish(&call->reply(), &call->rpcStatus(), (void*)call);
	}

	void sayHi(const std::string& user)
	{
		HiRequest request;
		request.set_name(user);

		ClientCallData<HiRequest, HiReply> * call = new ClientCallData<HiRequest, HiReply>(this, "HiRequest", mDeadline);

		call->responderReader() = mStub->AsyncSayHi(&call->context(), request, mCompletionQueue);
		call->responderReader()->Finish(&call->reply(), &call->rpcStatus(), (void*)call);
	}



public:

	virtual void onMessage(ClientCallMethod * cm)
	{
		ClientCallData<HelloRequest, HelloReply> * helloCd = dynamic_cast<ClientCallData<HelloRequest, HelloReply> *>(cm);
		if (helloCd)
		{
			if (helloCd->rpcStatus().ok())
				std::cout << "Greeter received: " << helloCd->reply().message() << std::endl;
			else
				std::cout << "RPC failed" << std::endl;

			delete helloCd;

			return;
		}

		ClientCallData<HiRequest, HiReply> * hiCd = dynamic_cast<ClientCallData<HiRequest, HiReply> *>(cm);
		if (hiCd)
		{
			if (hiCd->rpcStatus().ok())
				std::cout << "Greeter received: " << hiCd->reply().message() << std::endl;
			else
				std::cout << "RPC failed" << std::endl;

			delete hiCd;

			return;
		}
	}

protected:

private:


};




int main()
{
	GrpcClient client;
	client.run(1);

	MyConnection<Greeter, Greeter::Stub> conn1("192.168.0.28:50051", 3, &(client.cq()), 
												"test.com", "../certs/cert_test.com.pem");

	conn1.sayHello("Hello from conn1");
	conn1.sayHi("Hi from conn1");


	MyConnection<Greeter, Greeter::Stub> conn2("192.168.0.28:50052", 3, &(client.cq()),
		"test.com", "../certs/cert_test.com.pem");

	conn2.sayHello("Hello from conn2");
	conn2.sayHi("Hi from conn2");



	std::cout << "Press any key to quit." << std::endl;
	std::cin.get();

	client.shutdown();

	return 0;
}

