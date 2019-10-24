#ifndef PAICOIN_STAKE_STAKETX_H
#define PAICOIN_STAKE_STAKETX_H

#include <string>
#include "script/pai_data_classifier.h"

class CTransaction;
class CScript;

// ======================================================================
// classification and structural validation of transaction outputs;
// these functions are general purpose and unrelated to staking transactions; they could be moved elsewhere

bool IsPaymentTxout(const CTransaction& tx, uint32_t txoutIndex);
bool IsDataTxout(const CTransaction& tx, uint32_t txoutIndex);
bool IsStructuredDataTxout(const CTransaction& tx, uint32_t txoutIndex);

bool ValidateDataTxoutStructure(const CTransaction& tx, uint32_t txoutIndex,
                                unsigned numExpectedItems, unsigned expectedItemSizes[],
                                std::vector<std::vector<unsigned char> > *pRetItems, std::string& reason);

// ======================================================================
// creation of data scripts for staking transactions

enum EStakeDataClass {         // these values must not be changed (they are stored in scripts), so only appending is allowed
    STAKE_TxDeclaration,
    STAKE_TicketContribution
};

const size_t PAY_FOR_TASK_URL_LEN_MAX = 64;

struct PayForTaskData {
    int version;

    struct {
        CAmount best_model_payment_amount;
    } payment;

    struct Dataset {
        uint256 sha256;
        enum Format {
            unknown,
            MNIST
        } format;
        float training_set_size;
        float test_set_size;
        struct {
            std::string features;
            std::string labels;
        } source;
    } dataset;

    struct Validation {
        struct Strategy {
            enum Method {
                unknown,
                Holdout
            } method;
            float size;
        } strategy;
    } validation;

    struct Optimizer {
        enum Type {
            unknown,
            SGD
        } type;
        struct {
            float learning_rate;
            float momentum;
        } optimizer_initialization_parameters;
        float tau;
        int epochs;
        int batch_size;
        struct Initializer {
            enum Name {
                unknown,
                Xavier
            } name;
            struct {
                float magnitude;
            } parameters;
        } initializer;
    } optimizer;

};

CScript GetScriptForPayForTaskDecl(const PayForTaskData& data);

// ======================================================================
// classification and validation of staking transactions

static const uint32_t BUY_TICKET_MAX_INPUTS = 64;

// indices of inputs in stake transactions

// indices of outputs in stake transactions

// indices of data items in structured OP_RETURN outputs of stake transactions
const uint32_t structVersionIndex = 0;
const uint32_t dataClassIndex = 1;
const uint32_t stakeDataClassIndex = 2;
//---
const uint32_t txClassIndex = 3;
const uint32_t txVersionIndex = 4;
//---

enum ETxClass {         // these values must not be changed (they are stored in scripts), so only appending is allowed
    TX_PayForTask
};

//ETxClass ParseTxClass(const CTransaction& tx);
//bool IsStakeTx(ETxClass txClass);

//bool ValidateStakeTxDeclStructure(const CTransaction& tx, ETxClass eTxClassExpected, unsigned numExpectedItems, unsigned expectedItemSizes[], std::string& reason);

void Validate(const PayForTaskData & data);

#endif //PAICOIN_STAKE_STAKETX_H
