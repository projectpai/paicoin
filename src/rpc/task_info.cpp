#include "rpc/server.h"
#include "task_info_client.h"

#include "util.h"
#include "utilstrencodings.h"
#include "validation.h"

#include <univalue.h>


UniValue getTasksList(const TaskListType& list_type, const JSONRPCRequest& request)
{
    std::string help_name;
    std::string description;
    switch (list_type)
    {
    case TaskListType::StartedTasks:
        help_name = "getstartedtasks";
        description = "Get the tasks that started.";
        break;
    case TaskListType::CompletedTasks:
        help_name = "getcompletedtasks";
        description = "Get the completed tasks.";
        break;
    default:
        help_name = "getwaitingtasks";
        description = "Get the pending tasks.";
        break;
    }

    if (request.fHelp || request.params.size() != 2)
    throw std::runtime_error(
        help_name + " <page> <per_page>\n"
        + description +
        "\nArguments:\n"
        "1. page         (numeric, required) Requested page.\n"
        "2. per_page       (numeric, required) Results per page.\n"


        "\nResult:\n"
        "code     (int) HTTP response code.\n"
        "pagination     (Pagination) Pagination information.\n"
        "tasks     ([TaskRecord]) List of tasks.\n"
    );

    uint64_t page = 5;
    uint64_t per_page = 20;

    if (!request.params[0].isNull() && !request.params[1].isNull())
    {
        page = request.params[0].get_int64();
        per_page = request.params[1].get_int64();
    }

    std::string taskInfoServerAddress = gArgs.GetArg("-verificationserver", "localhost:50051");
    TaskListClient client(grpc::CreateChannel(taskInfoServerAddress, grpc::InsecureChannelCredentials()));
    
    if(list_type == TaskListType::StartedTasks)
    {
        return client.GetStartedTasks(page, per_page);
    }
    else if (list_type == TaskListType::CompletedTasks)
    {
        return client.GetCompletedTasks(page, per_page);
    }

    return client.GetWaitingTasks(page, per_page);
}

UniValue getWaitingTasks(const JSONRPCRequest& request)
{
    return getTasksList(TaskListType::WaitingTasks, request);
}

UniValue getStartedTasks(const JSONRPCRequest& request)
{
    return getTasksList(TaskListType::StartedTasks, request);
}

UniValue getCompletedTasks(const JSONRPCRequest& request)
{
    return getTasksList(TaskListType::CompletedTasks, request);
}

UniValue getTaskDetails(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
    throw std::runtime_error(
        "gettaskdetails <task_id>\n"
        "Provides details about a specific task.\n"
        "\nArguments:\n"
        "1. task_id         (string, required) Task ID.\n"

        "\nResult:\n"
        "code        (int) HTTP response code.\n"
        "task_id     (string) The ID of the task.\n"
        "model_type  (string) Type of model used in the ML task.\n"
        "nodes_no  (numeric) Total number of nodes in the ML model.\n"
        "batch_size  (numeric) Batch size used by the ML task.\n"
        "optimizer  (string) Optimizer used by the ML task.\n"
        "created  (timestamp) Task creation time.\n"
        "dataset  (string) Dataset type.\n"
        "initializer  (string) Initializer type for the optimizer.\n"
        "loss_function  (string) Loss function.\n"
        "tau  (float) Quantization threshold for gradients.\n"
        "evaluation_metrics  (list) Evaluation metrics to decide upon best model.\n"
        "epochs_info  (object) Average values for metrics for each epoch.\n"
    );

    std::string task_id;
    if (!request.params[0].isNull())
    {
        task_id = request.params[0].get_str();
    }

    std::string taskInfoServerAddress = gArgs.GetArg("-verificationserver", "localhost:50051");
    TaskListClient client(grpc::CreateChannel(taskInfoServerAddress, grpc::InsecureChannelCredentials()));

    return client.GetTaskDetails(task_id);
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "task_info",             "getwaitingtasks",       &getWaitingTasks,       {"page","per_page"} },
    { "task_info",             "getstartedtasks",       &getStartedTasks,       {"page","per_page"} },
    { "task_info",             "getcompletedtasks",     &getCompletedTasks,     {"page","per_page"} },
    { "task_info",             "gettaskdetails",        &getTaskDetails,        {"task_id"} },
};

void RegisterTaskInfoRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}