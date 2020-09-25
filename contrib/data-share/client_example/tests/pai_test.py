# pai_test.py
#
# PAI Coin PAI Protocol test script
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

from pprint import pprint
from paicointxn import PAI

pai = PAI(version=0x10)

pack0 = pai.pack('revoke', '', 'MyPAI', 'voice')
print('Raw PAI pack[0]: {}'.format(pack0))
print('Assembled PAI pack[0]:')
print(pai)

pack1 = pai.pack('grant', '', 'MyPAI', 'avatar')
print('Raw PAI pack[1]: {}'.format(pack1))
print('Assembled PAI pack[1]:')
print(pai)

res = pai.unpack(pack1)
print('PAI unpack pack[1] result:')
pprint(res)
print('\nDisassembled PAI pack[1]:')
print(pai)

res = pai.unpack(pack0)
print('PAI unpack pack[0] result:')
pprint(res)
print('\nDisassembled PAI pack[0]:')
print(pai)

pack3 = PAI.assembly('grant', '', 'MyPAI', 'train behaviour', version=0x10)
print('Raw PAI pack[3]: {}'.format(pack3))
print('Assembled PAI pack[3]:')

res = PAI.disassembly(pack3, version=0x10)
print('PAI unpack pack[3] result:')
pprint(res)
