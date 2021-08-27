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
            "getwaitingtasks ( TaskListRequest )\n"

            "\nArguments:\n"
            "1. getwaitingtasks_request         (json object) A json object in the following spec\n"
            "     {\n"
            "       \"page\":\"page\"       (int, required) Requested page.\n"
            "       \"per_page\":\"per_page\"           (int, required) Results per page\n"
            "     }\n"
            "\n"

            "\nResult:\n"
            "code     (int) HTTP response code.\n"
            "pagination     (Pagination) Pagination information.\n"
            "tasks     ([TaskRecord]) List of tasks.\n"
        );

    LOCK(cs_main);

    const uint64_t page = request.params[0].get_int();
    const uint64_t per_page = request.params[1].get_int();

    std::string taskInfoServerAddress = gArgs.GetArg("-verificationserver", "localhost:50051");
    TaskListClient client(grpc::CreateChannel(taskInfoServerAddress, grpc::InsecureChannelCredentials()));
    client.TestGetWaitingTasks();

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("test", page));
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