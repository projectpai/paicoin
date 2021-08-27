#include "rpc/server.h"
#include "task_info_client.h"

#include "util.h"
#include "utilstrencodings.h"
#include "validation.h"

#include <univalue.h>


UniValue getWaitingTasks(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "getwaitingtasks <page> <per_page>\n"
            "\nGet the pending tasks.\n"
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
    client.TestGetWaitingTasks();

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("page", page));
    obj.push_back(Pair("per_page", per_page));
    return obj;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "task_info",             "getwaitingtasks",       &getWaitingTasks,       {"page","per_page"} },
};

void RegisterTaskInfoRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}