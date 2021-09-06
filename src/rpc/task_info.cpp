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

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "task_info",             "getwaitingtasks",       &getWaitingTasks,       {"page","per_page"} },
    { "task_info",             "getstartedtasks",       &getStartedTasks,       {"page","per_page"} },
    { "task_info",             "getcompletedtasks",     &getCompletedTasks,     {"page","per_page"} },
};

void RegisterTaskInfoRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}