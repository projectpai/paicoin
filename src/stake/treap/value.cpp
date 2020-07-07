#include "stake/treap/value.h"

Value::Value(uint32_t height, bool missed, bool revoked, bool spent, bool expired) :
    height(height),
    missed(missed),
    revoked(revoked),
    spent(spent),
    expired(expired)
{
}

Value::Value(uint32_t height) :
    height(height),
    missed(false),
    revoked(false),
    spent(false),
    expired(false)
{
}

bool operator==(const Value& lhs, const Value& rhs)
{
    return lhs.height  == rhs.height
        && lhs.missed  == rhs.missed
        && lhs.revoked == rhs.revoked
        && lhs.spent   == rhs.spent
        && lhs.expired == rhs.expired;
}