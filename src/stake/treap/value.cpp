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

Value::Value(const Value& other) :
    height(other.height),
    missed(other.missed),
    revoked(other.revoked),
    spent(other.spent),
    expired(other.expired)
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