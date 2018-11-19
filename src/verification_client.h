#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <grpcpp/grpcpp.h>

#include "verifier.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using pai::pouw::verification::Request;
using pai::pouw::verification::Response;
using pai::pouw::verification::Verifier;
using pai::pouw::verification::Response_ReturnCode;
using pai::pouw::verification::Response_ReturnCode_GENERAL_ERROR;

class VerificationClient {
 public:
  VerificationClient(std::shared_ptr<Channel> channel)
      : stub_(Verifier::NewStub(channel)) {}

  std::pair<Response_ReturnCode, std::string> Verify(const std::string& msg_id, const std::string& model_hash, const std::string& msg_next_id) {

    Request request;
    request.set_msg_id(msg_id);
    request.set_model_hash(model_hash);
    request.set_msg_next_id(msg_next_id);

    Response response;
    ClientContext context;

    // The actual RPC.
    Status status = stub_->Verify(&context, request, &response);

    // Act upon its status.
    if (status.ok()) {
      return std::make_pair(response.code(), response.description());
    }
    else {
      std::cout << status.error_code() << ": " << status.error_message() << std::endl;
      return std::make_pair(Response_ReturnCode_GENERAL_ERROR, "RPC failed");
    }
  }

 private:
  std::unique_ptr<Verifier::Stub> stub_;
};
