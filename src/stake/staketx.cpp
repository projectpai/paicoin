#include "primitives/transaction.h"
#include "stake/staketx.h"
#include "script/standard.h"

#include <sstream>

#define REASON(arg) do { std::ostringstream _reason; _reason << arg; reason = _reason.str(); } while(false)

bool IsPaymentTxout(const CTransaction& tx, uint32_t txoutIndex)
{
    if (txoutIndex >= tx.vout.size())
        return false;

    const CScript& scr = tx.vout[txoutIndex].scriptPubKey;
    return !scr.IsUnspendable();
}

bool IsDataTxout(const CTransaction& tx, uint32_t txoutIndex)
{
    if (txoutIndex >= tx.vout.size())
        return false;

    const CScript& scr = tx.vout[txoutIndex].scriptPubKey;
    return scr.size() > 0 && scr[0] == OP_RETURN;
}

bool IsStructuredDataTxout(const CTransaction& tx, uint32_t txoutIndex)
{
    if (txoutIndex >= tx.vout.size())
        return false;

    const CScript& scr = tx.vout[txoutIndex].scriptPubKey;
    return scr.size() >= 3 && scr[0] == OP_RETURN && scr[1] == OP_STRUCT;   // scr[2] (version) must be present
}

//bool ValidateTxDeclStructure(const CTransaction& tx, ETxClass eTxClassExpected, unsigned numExpectedItems, unsigned expectedItemSizes[], std::string& reason)
//{
//    std::vector<std::vector<unsigned char> > items;
//    if (!ValidateDataTxoutStructure(tx, txdeclOutputIndex, numExpectedItems, expectedItemSizes, &items, reason))
//        return false;
//
//    if (CScriptNum(items[0], false).getint() != eTxClassExpected)   // returned items begin with txClass, hence index 0
//        return false;
//
//    return true;
//}

bool ValidateDataTxoutStructure(const CTransaction& tx, uint32_t txoutIndex,
                       unsigned numExpectedItems, unsigned expectedItemSizes[],
                       std::vector<std::vector<unsigned char> > *pRetItems, std::string& reason)
{
    bool isData = IsDataTxout(tx, txoutIndex);
    bool isStruct = IsStructuredDataTxout(tx, txoutIndex);
    if (!isData && !isStruct) {
        REASON("output " << txoutIndex << " not a data output");
        return false;
    }
    int offsetItems = isStruct ? txClassIndex : 0; // skip version, dataClass and stakeDataClass

    const CScript& script = tx.vout[txoutIndex].scriptPubKey;
    txnouttype scriptType;
    std::vector<std::vector<unsigned char> > items;
    if (!Solver(script, scriptType, items)) {
        REASON("couldn't parse output " << txoutIndex);
        return false;
    }

    std::vector<std::vector<unsigned char> > localItems;
    auto& resultItems = pRetItems ? *pRetItems : localItems;
    std::copy(items.begin() + offsetItems, items.end(), std::back_inserter(resultItems));

    if (resultItems.size() != numExpectedItems) {
        REASON("in output " << txoutIndex << " expected " << numExpectedItems << " data items, found " << resultItems.size());
        return false;
    }

    // following OP_RETURN header, there must exist pieces of data whose number and sizes correspond to the expected dataSizes
    for (unsigned i=0; i<numExpectedItems; i++) {
        if (expectedItemSizes[i] == 0) // zero indicates a variable item size, so we skip size comparison in that case
            continue;
        if (resultItems[i].size() != expectedItemSizes[i]) {
            REASON("in output " << txoutIndex << ", data item " << i
                         << " expected size was " << expectedItemSizes[i]
                         << " bytes, found " << resultItems[i].size() << " bytes");
            return false;
        }
    }

    return true;
}

// ======================================================================

CScript GetScriptForPayForTaskDecl(const PayForTaskData& data)
{
    return GetScriptForStructuredData(CLASS_Staking)
        << STAKE_TxDeclaration
        << TX_PayForTask
        << data.version
        << data.payment.best_model_payment_amount

        << ToByteVector(data.dataset.sha256)
        << data.dataset.format
        << data.dataset.training_set_size
        << data.dataset.test_set_size
        << ToByteVector(data.dataset.source.features)
        << ToByteVector(data.dataset.source.labels)

        << data.validation.strategy.method
        << data.validation.strategy.size

        << data.optimizer.type
        << data.optimizer.optimizer_initialization_parameters.learning_rate
        << data.optimizer.optimizer_initialization_parameters.momentum
        << data.optimizer.tau
        << data.optimizer.epochs
        << data.optimizer.batch_size
        << data.optimizer.initializer.name
        << data.optimizer.initializer.parameters.magnitude
    ;
}

// ======================================================================

bool ParseStakeData(const CTransaction& tx, uint32_t txoutIndex, EStakeDataClass eStakeData, unsigned minItems, std::vector<std::vector<unsigned char> >& items)
{
    if (tx.vout.size() <= txoutIndex || !IsStructuredDataTxout(tx, txoutIndex))
        return false;

    const CScript& script = tx.vout[txoutIndex].scriptPubKey;
    txnouttype scriptType;
    if (!Solver(script, scriptType, items) || scriptType != TX_STRUCT_DATA)
        return false;

    if (items.size() < 3 || items.size() < minItems)     // structVersion, dataClass, stakeDataClass
        return false;

    int structVersion = CScriptNum(items[structVersionIndex], false).getint();
    if (structVersion != 1)
        return false;

    EDataClass eDataClass = (EDataClass) CScriptNum(items[dataClassIndex], false).getint();
    if (eDataClass != CLASS_Staking)
        return false;

    EStakeDataClass eStakeDataClass = (EStakeDataClass) CScriptNum(items[stakeDataClassIndex], false).getint();
    if (eStakeDataClass != eStakeData)
        return false;

    return true;
}

//ETxClass ParseTxClass(const CTransaction& tx)
//{
//    int minItems = 4;   // structHeaderVersion, dataClass, stakeDataClass, txClass
//    std::vector<std::vector<unsigned char> > items;
//    if (!ParseStakeData(tx, txdeclOutputIndex, STAKE_TxDeclaration, minItems, items))
//        return TX_Regular;
//
//    return (ETxClass) CScriptNum(items[txClassIndex], false).getint();
//}

bool IsStakeTx(ETxClass eTxClass)
{
    return eTxClass == TX_PayForTask;
}

void Validate(const PayForTaskData & data)
{
    if (data.version != 1)
        throw std::runtime_error("Unsupported version");

    if (data.dataset.format == PayForTaskData::Dataset::Format::unknown)
        throw std::runtime_error("Unsupported  data format");

    if (data.dataset.source.features.size() > PAY_FOR_TASK_URL_LEN_MAX)
        throw std::runtime_error("Dataset source features URL too long");

    if (data.dataset.source.labels.size() > PAY_FOR_TASK_URL_LEN_MAX)
        throw std::runtime_error("Dataset source labels URL too long");

    if (data.validation.strategy.method == PayForTaskData::Validation::Strategy::Method::unknown)
        throw std::runtime_error("Unsupported validation strategy method");

    if (data.optimizer.type == PayForTaskData::Optimizer::Type::unknown)
        throw std::runtime_error("Unsupported optimizer type");

    if (data.optimizer.initializer.name == PayForTaskData::Optimizer::Initializer::Name::unknown)
        throw std::runtime_error("Unsupported optimizer initializer name");
}
