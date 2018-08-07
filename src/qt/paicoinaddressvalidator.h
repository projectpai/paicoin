// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_QT_PAICOINADDRESSVALIDATOR_H
#define PAICOIN_QT_PAICOINADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class PAIcoinAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit PAIcoinAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

/** PAI Coin address widget validator, checks for a valid paicoin address.
 */
class PAIcoinAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit PAIcoinAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

#endif // PAICOIN_QT_PAICOINADDRESSVALIDATOR_H
