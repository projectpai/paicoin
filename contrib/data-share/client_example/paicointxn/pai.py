"""
PAI Coin PAI Protocol over OP_RETURN metadata
"""
# pai.py
#
# PAI Coin - PAI Protocol over OP_RETURN metadata
#
# Copyright (c) ObEN, Inc. - https://oben.me/
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# PAI Protocol format
#
# SOP      - Start of PAI (delimiter): 1 Byte = 0x92 crc8('PAI')
# Ver      - Version: 4 bits [0:15]
# Rev      - Revision: 4 bits [0:15]
# Res      - Reserved: 0xFFFFFFFFFFFF
# OP       - Operation: [0:255] operations: NoOperation (NOP) = 0xFF
# StM      - Storage Method: [0:255] storage methods: NoStorage (NST) = 0xFF
# Op[1:2]L - Operand[1:2]Length: [0:17] first operand, [0:15] second operand
# Op[1:2]D - Operand[1:2]Data
# CRC32    - Cyclic redundancy check 32 bits
#
# Note:
# Variable length is used for operands size 0x00 - 0x41 (up to 65 Bytes)
#
# First operand size: 0x00 to 0x41
# First operand data: up to 65 bytes
#
# Second operand size: 0x0 to 0x40
# Second operand data: up to 64 bytes (1 byte 'lost' because of the second
# operand size padding)
#
# PAYLOAD_MIN_LENGTH = OP + StM = 0x02 (2 bytes)
# PAYLOAD_MAX_LENGTH = OP + StM + Op1L + Op1D + Op2L + Op2D = 0x44 (68 bytes)
#
#  ____________________________________________________________________________
# |      PAI Header     |              PAI Payload             |  PAI Checksum |
# |---------------------+--------------------------------------+---------------|
# | SOP | Ver:Rev | Res | OP | StM | Op1L | Op1D | Op2L | Op2D |    CRC32      |
# |_____|_________|_____|____|_____|______|______|______|______|_______________|

from struct import pack
from binascii import crc32, hexlify
from unpacker import PAIcoinBuffer, hex2bin, bin2hex

SOP = 0x92

VERSION = {'v1': 0x1, 'v2': 0x2, }
REVISION = {'rev0': 0x0, 'rev1': 0x1, }

OPERATIONS = {'grant':          0x00,
              'revoke':         0x01,
              'add_tracker':    0x02,
              'remove_tracker': 0x03,
              '':               0xFF,
              'nop':            0xFF}

STORAGE = {'': 0xFF, 'nst': 0xFF}

HEADER_LENGTH = 8
PAYLOAD_MAX_LENGTH = 0x44


class Header(object):
    def __init__(self, version=0x10):
        self.sop = SOP
        self.version = version
        self.reserved = (3 * (0xFFFF, ))
        self._data = b''

    def __str__(self):
        reserved = ''.join(map(hex, self.reserved)).replace('0x', '')
        ret = hex(self.sop), hex(self.version), reserved

        return 'Header:\n' \
               '\tSOP[{}],\n' \
               '\tversion[{}],\n' \
               '\treserved[0x{}]\n'.format(*ret)

    @property
    def data(self):
        return pack('BBHHH', self.sop, self.version, *self.reserved)


class Payload(object):
    def __init__(self):
        self.op = b''
        self.stm = b''
        self.operand1 = b''
        self.op1_length = 0
        self.operand2 = b''
        self.op2_length = 0
        self._data = b''

    def __str__(self):
        ret = hex(int(self.op)), hex(int(self.stm)),\
              hex(self.op1_length), hexlify(self.operand1),\
              hex(self.op2_length), hexlify(self.operand2)

        return 'Payload:\n' \
               '\tOperation[{}],\n' \
               '\tStorageMethod[{}],\n' \
               '\tOperand1 length[{}],\n' \
               '\tOperand1 data[0x{}],\n' \
               '\tOperand2 length[{}],\n' \
               '\tOperand2 data [0x{}]\n'.format(*ret)

    def clear(self):
        self.op = b''
        self.stm = b''
        self.operand1 = b''
        self.op1_length = 0
        self.operand2 = b''
        self.op2_length = 0
        self._data = b''

    @property
    def data(self):
        return self._data

    @data.setter
    def data(self, value):
        self.op = value[0]
        self.stm = value[1]
        self.operand1 = value[2]
        self.operand2 = value[3]

        if self.operand1:
            if len(self.operand1) <= PAYLOAD_MAX_LENGTH - 3:
                self.op1_length = len(self.operand1)
            else:
                msg = 'Wrong operand1 length: {}'.format(len(self.operand1))
                raise self.Error(msg)

        if self.operand2:
            if len(self.operand2) <= PAYLOAD_MAX_LENGTH - self.op1_length - 4:
                self.op2_length = len(self.operand2)
            else:
                msg = 'Wrong operand2 length: {}'.format(len(self.operand2))
                raise self.Error(msg)

        self._data = pack('BB', self.op, self.stm)

        if not self.operand1:
            self._data += chr(self.op1_length)
        else:
            self._data += chr(self.op1_length) + self.operand1

        if not self.operand2:
            if self.op1_length != PAYLOAD_MAX_LENGTH - 3:
                self._data += chr(self.op2_length)
        else:
            self._data += chr(self.op2_length) + self.operand2

    class Error(Exception):
        pass


class PAI(object):
    def __init__(self, version=0x10):
        self._header = Header(version)
        self._payload = Payload()
        self._crc = b''
        self._data = b''

    def __str__(self):
        checksum = 'Checksum:\n' \
                   '\tcrc32[{}]\n'.format(hex(int(self._crc)))

        return str(self._header) + str(self._payload) + checksum

    @property
    def header(self):
        return self._header.data

    def header_check(self, buff):
        sop = buff.shift_unpack(1, 'B')
        if sop != self._header.sop:
            raise self.Error('Start of PAI magic key is missing')

        ver = buff.shift_unpack(1, 'B')
        if ver != self._header.version:
            raise self.Error('Protocol version not supported')

        reserved = buff.shift_unpack(6, 'HHH')
        res_len = len(reserved)

        if res_len != 3:
            raise self.Error('Corrupted header')
        else:
            for idx in xrange(res_len):
                if reserved[idx] != self._header.reserved[idx]:
                    raise self.Error('Corrupted header')

    @property
    def payload(self):
        return self._payload.data

    def payload_extract(self, buff):
        op = buff.shift_unpack(1, 'B')
        stm = buff.shift_unpack(1, 'B')
        operand1_length = buff.shift_unpack(1, '<B')
        operand1 = buff.shift(operand1_length)
        operand2_length = buff.shift_unpack(1, 'B')
        operand2 = buff.shift(operand2_length)

        try:
            self._payload.data = (op, stm, operand1, operand2)
        except self._payload.Error:
            raise self.Error('Corrupted PAI payload')

    @property
    def pai_crc(self):
        return crc32(self._header.data + self._payload.data) & 0xFFFFFFFF

    @property
    def checksum(self):
        self._crc = self.pai_crc

        return pack('>L', self._crc)

    def checksum_match(self, buff):
        if buff.remaining() == 4:
            self._crc = buff.shift_unpack(4, '>L')

            if self._crc != self.pai_crc:
                raise self.Error('Checksum error')

    def pack(self, op, stm, operand1=None, operand2=None):
        self._payload.clear()
        self._payload.data = (OPERATIONS[op], STORAGE[stm], operand1, operand2)
        self._data = self.header + self.payload + self.checksum

        return bin2hex(self._data)

    @classmethod
    def assembly(cls, op, stm, operand1=None, operand2=None, version=0x10):
        pai = cls(version)

        return pai.pack(op, stm, operand1, operand2)

    def unpack(self, pai):
        self._payload.clear()
        pai = hex2bin(pai)
        buff = self._Buffer(pai)

        self.header_check(buff)
        self.payload_extract(buff)
        self.checksum_match(buff)

        pai = self._payload
        res = {'operation': pai.op,
               'storageMethod': pai.stm,
               'operand1': pai.operand1,
               'operand2': pai.operand2}

        return res

    @classmethod
    def disassembly(cls, stream, version=0x10):
        pai = cls(version)

        return pai.unpack(stream)

    class _Buffer(PAIcoinBuffer):
        pass

    class Error(Exception):
        pass
