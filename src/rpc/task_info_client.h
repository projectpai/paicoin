#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <grpcpp/grpcpp.h>
#include <google/protobuf/util/time_util.h>

#include "task_info.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using pai::pouw::task_info::TaskListRequest;
using pai::pouw::task_info::TaskListResponse;
using pai::pouw::task_info::TaskInfo;


class TaskListClient {
public:
    TaskListClient(std::shared_ptr<Channel> channel)
    : stub_(TaskInfo::NewStub(channel)) {}

    void TestGetWaitingTasks() {

        TaskListRequest request;
        request.set_page(1);
        request.set_per_page(20);

        TaskListResponse response;
        ClientContext context;

        // The actual RPC.
        Status status = stub_->GetWaitingTasks(&context, request, &response);

        // Act upon its status.
        if (status.ok()) {
            auto code = response.code();
            auto pagination = response.pagination();
            auto tasks = response.tasks();
            std::cout << "Code: " << code << std::endl;
            std::cout << std::endl;
            std::cout << "Pagination:" << std::endl;
            std::cout << "- page: " << pagination.page() << std::endl;
            std::cout << "- per_page: " << pagination.per_page() << std::endl;
            std::cout << "- page_count: " << pagination.page_count() << std::endl;
            std::cout << "- total_count: " << pagination.total_count() << std::endl;
            std::cout << std::endl;
            std::cout << "- Navigation:" << std::endl;
            std::cout << "  - self: " << pagination.navigation().self() << std::endl;
            std::cout << "  - first: " << pagination.navigation().first() << std::endl;
            std::cout << "  - previous: " << pagination.navigation().previous() << std::endl;
            std::cout << "  - next: " << pagination.navigation().next() << std::endl;
            std::cout << "  - last: " << pagination.navigation().last() << std::endl;
            std::cout << std::endl;
            std::cout << "Tasks: " << std::endl;
            for (auto task : tasks) {
                std::cout << "- task_id: " << task.task_id() << std::endl;
                std::cout << "  - model_type: " << task.model_type() << std::endl;
                std::cout << "  - nodes_no: " << task.nodes_no() << std::endl;
                std::cout << "  - batch_size: " << task.batch_size() << std::endl;
                std::cout << "  - optimizer: " << task.optimizer() << std::endl;
                std::cout << "  - created: " << google::protobuf::util::TimeUtil::ToString(task.created()) << std::endl;
                std::cout << std::endl;
            }
        } else {
            std::cout << status.error_code() << ": " << status.error_message()
            << std::endl;
        }
    }

private:
    std::unique_ptr<TaskInfo::Stub> stub_;
};
