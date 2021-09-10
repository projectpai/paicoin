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

using pai::pouw::task_info::TaskDetailsRequest;
using pai::pouw::task_info::TaskDetailsResponse;

using pai::pouw::task_info::TaskIDRequest;
using pai::pouw::task_info::TaskIDResponse;

using pai::pouw::task_info::TaskInfo;
using pai::pouw::task_info::HTTPReturnCode;


enum class TaskListType { WaitingTasks, StartedTasks, CompletedTasks };


class TaskListClient {
public:
    TaskListClient(std::shared_ptr<Channel> channel)
    : stub_(TaskInfo::NewStub(channel)) {}

    UniValue GetWaitingTasks(uint64_t page, uint64_t per_page) {
        return GetTaskList(TaskListType::WaitingTasks, page, per_page);
    }

    UniValue GetStartedTasks(uint64_t page, uint64_t per_page) {
        return GetTaskList(TaskListType::StartedTasks, page, per_page);
    }

    UniValue GetCompletedTasks(uint64_t page, uint64_t per_page) {
        return GetTaskList(TaskListType::CompletedTasks, page, per_page);
    }

    UniValue GetTaskDetails(std::string task_id) {
        
        UniValue task_obj(UniValue::VOBJ);
        TaskDetailsRequest request;
        request.set_task_id(task_id);

        TaskDetailsResponse response;
        ClientContext context;

        Status status = stub_->GetTaskDetails(&context, request, &response);

        if (!status.ok()) {
            std::cout << status.error_code() << ": " << status.error_message()
            << std::endl;
            task_obj.push_back(Pair("error_code", status.error_code()));
            task_obj.push_back(Pair("error_message", status.error_message()));
            return task_obj;
        }

        auto code = response.code();
        if(code != HTTPReturnCode::OK)
        {
            task_obj.push_back(Pair("error_code", code));
            task_obj.push_back(Pair("error_message", "Task details unavailable."));
            return task_obj;
        }

        task_obj.push_back(Pair("task_id", response.task_id()));
        task_obj.push_back(Pair("model_type", response.model_type()));
        task_obj.push_back(Pair("nodes_no", response.nodes_no()));
        task_obj.push_back(Pair("batch_size", (uint64_t)response.batch_size()));
        task_obj.push_back(Pair("optimizer", response.optimizer()));
        task_obj.push_back(Pair("created", google::protobuf::util::TimeUtil::ToString(response.created())));
        task_obj.push_back(Pair("dataset", response.dataset()));
        task_obj.push_back(Pair("initializer", response.initializer()));
        task_obj.push_back(Pair("loss_function", response.loss_function()));
        task_obj.push_back(Pair("epochs", (uint64_t)response.epochs()));
        task_obj.push_back(Pair("tau", (float)response.tau()));

        auto evaluation_metrics = response.evaluation_metrics();
        UniValue evaluation_metrics_obj(UniValue::VARR);
        for (auto evaluation_metric : evaluation_metrics)
        {
            UniValue tmpVal(UniValue::VSTR, evaluation_metric);
            evaluation_metrics_obj.push_back(tmpVal);
        }
        
        task_obj.push_back(Pair("evaluation_metrics", evaluation_metrics_obj));

        return task_obj;
    }

    std::string GetTaskId(const std::string& msg_id)
    {
        TaskIDRequest request;
        request.set_msg_id(msg_id);

        TaskIDResponse response;
        ClientContext context;

        Status status = stub_->GetTaskID(&context, request, &response);
        if (!status.ok() || response.code() != HTTPReturnCode::OK)
        {
            return std::string("unavailable");
        }

        return response.task_id();
            
    }

protected:

    UniValue GetTaskList(const TaskListType& task_list_type, uint64_t page, uint64_t per_page)
    {

        UniValue obj(UniValue::VOBJ);

        TaskListRequest request;
        request.set_page(page);
        request.set_per_page(per_page);

        TaskListResponse response;
        ClientContext context;

        Status status;
        switch (task_list_type)
        {
        case TaskListType::StartedTasks:
            status = stub_->GetStartedTasks(&context, request, &response);
            break;
        case TaskListType::CompletedTasks:
            status = stub_->GetCompletedTasks(&context, request, &response);
            break;
        default:
            status = stub_->GetWaitingTasks(&context, request, &response);
            break;
        }

        // Act upon its status.
        if (!status.ok()) {
            std::cout << status.error_code() << ": " << status.error_message()
            << std::endl;
            obj.push_back(Pair("error_code", status.error_code()));
            obj.push_back(Pair("error_message", status.error_message()));
        }
        else
        {
            auto code = response.code();
            auto pagination = response.pagination();
            auto tasks = response.tasks();

            obj.push_back(Pair("code", code));

            UniValue pagination_obj(UniValue::VOBJ);
            pagination_obj.push_back(Pair("page", (uint64_t)pagination.page()));
            pagination_obj.push_back(Pair("per_page", (uint64_t)pagination.per_page()));
            pagination_obj.push_back(Pair("page_count", (uint64_t)pagination.page_count()));
            pagination_obj.push_back(Pair("total_count", (uint64_t)pagination.total_count()));

            UniValue navigation_obj(UniValue::VOBJ);
            navigation_obj.push_back(Pair("self", pagination.navigation().self()));
            navigation_obj.push_back(Pair("first", pagination.navigation().first()));
            navigation_obj.push_back(Pair("previous", pagination.navigation().previous()));
            navigation_obj.push_back(Pair("next", pagination.navigation().next()));
            navigation_obj.push_back(Pair("last", pagination.navigation().last()));

            pagination_obj.push_back(Pair("navigation", navigation_obj));
            obj.push_back(Pair("pagination", pagination_obj));

            UniValue task_list_obj(UniValue::VARR);
            for (auto task : tasks) {
                UniValue task_obj(UniValue::VOBJ);
                task_obj.push_back(Pair("task_id", task.task_id()));
                task_obj.push_back(Pair("model_type", task.model_type()));
                task_obj.push_back(Pair("nodes_no", task.nodes_no()));
                task_obj.push_back(Pair("batch_size", (uint64_t)task.batch_size()));
                task_obj.push_back(Pair("optimizer", task.optimizer()));
                task_obj.push_back(Pair("created", google::protobuf::util::TimeUtil::ToString(task.created())));
                task_list_obj.push_back(task_obj);
            }
            obj.push_back(Pair("tasks", task_list_obj));
        }

        return obj;
    }

private:
    std::unique_ptr<TaskInfo::Stub> stub_;
};
