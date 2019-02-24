#!/usr/bin/python3
"""
MIT License

Copyright (c) 2019 Marcin Harasimczuk

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""
import sys

from PyQt5.QtCore import QObject
from PyQt5.QtCore import QThread
from PyQt5.QtCore import QMutex
from PyQt5.QtCore import pyqtSignal
from PyQt5.QtCore import pyqtSlot

from subprocess import Popen
from subprocess import PIPE

from enum import Enum

import unittest

import os

kDsoSearchPath = os.getenv("SNOOP_DSO_SEARCH_PATH", "")
kAddr2LineBin = os.getenv("SNOOP_ADDR2LINE_BIN", "addr2line")
kMemoryMode = os.getenv("SNOOP_MEMORY_MODE", "x86_64")

kArmBinOffset = 0x10000

def isDSO(filepath):
    # TODO Find better way?
    basename = os.path.basename(filepath)
    if basename.endswith(".so"):
        return True
    if ".so." in basename:
        return True
    return False

class PathHelper():
    def __init__(self, filename):
        self.basename = os.path.basename(filename)
        basepath = os.path.dirname(filename)
        self.paths = [basepath]

    def addPath(self, path):
        self.paths.append(path)

    def getFileName(self):
        for path in self.paths:
            filename = path + "/" + self.basename
            if os.path.isfile(filename):
                return filename
        print("Failed to find " + self.basename)
        return ""

class DecoderEntry():
    __slots__ = ["begin", "end", "decoder"]
    def __init__(self, begin, end, decoder):
        self.begin = begin
        self.end = end
        self.decoder = decoder

class DecoderQuery:
    __slots__ = ["inputs", "indexes"]
    def __init__(self, inputs, indexes):
        self.inputs = inputs
        self.indexes = indexes

class DecoderManager():
    def __init__(self, filename):
        self.snoopLibName = "libsnoop.so"
        self.filename = filename
        self.entries = []

        with open(filename, 'r') as memoryMap:
            for line in memoryMap:
                line = line.strip('\n').split(' ')
                # Check if executable memory
                if 'x' in line[1]:
                    self.makeEntry(line)

    def makeEntry(self, line):
        addr_range = line[0].split('-')
        if (os.path.basename(line[-1]) == self.snoopLibName):
            return
        helper = PathHelper(line[-1])
        for path in kDsoSearchPath.split(':'):
            helper.addPath(path)
        helper.addPath(kDsoSearchPath)
        helper.addPath(os.path.dirname(self.filename))
        filename = helper.getFileName()
        if (filename == ""):
            print("Failed to make entry")
            return
        decoder = Decoder(helper.getFileName())
        self.entries.append(
            DecoderEntry(int(addr_range[0],16), int(addr_range[1],16), decoder))

    def findEntryIdxIterative(self, value):
        for idx, entry in enumerate(self.entries):
            if (value >= entry.begin and value <= entry.end):
                return idx
        return -1

    def findEntryIdxBinarySearch(self, value):
        first = 0
        last = len(self.entries) - 1
        while (first <= last):
            current = first + (last - first) // 2
            if (value >= self.entries[current].begin and
                value <= self.entries[current].end):
                return current
            if (value > self.entries[current].end):
                first = current + 1
            else:
                last = current - 1
        return -1

    def findEntryIdx(self, value):
        return self.findEntryIdxBinarySearch(value)

    def decode(self, input_list):
        queries = [DecoderQuery([],[]) for i in range(len(self.entries))]
        for input_idx, input_addr in enumerate(input_list):
            input_value = int(input_addr, 16)
            entry_idx = self.findEntryIdx(input_value)
            if (entry_idx >= 0):
                offset = self.entries[entry_idx].begin
                if (kMemoryMode == "arm" and offset == kArmBinOffset):
                    offset = 0
                queries[entry_idx].inputs.append(hex(input_value - offset))
                queries[entry_idx].indexes.append(input_idx)
        for query_idx, query in enumerate(queries):
            output_list = self.entries[query_idx].decoder.decode(query.inputs)
            for idx, output in zip(query.indexes, output_list):
                input_list[idx] = output
        return input_list

    def debugPrint(self):
        print("DecoderManager(filename: " + self.filename + ")")
        for entry in self.entries:
            print("\tDecoderEntry(" + str(entry.begin) + "-" + str(entry.end) +
                  " -> "  + entry.decoder.getName())

    def close(self):
        for entry in self.entries:
            entry.decoder.close()
'''
Sync Interface

'''

class Decoder():
    def __init__(self, filename):
        self.filename = filename
        if isDSO(filename):
            command = [kAddr2LineBin, '--section=.text', '-f', '-C', '-e', filename]
        else:
            command = [kAddr2LineBin, '-f', '-C', '-e', filename]
        self.decoder = Popen(command, stdin=PIPE, stdout=PIPE)
        self.mutex = QMutex()

    def decode(self, input_list):
        self.mutex.lock()
        output_list = []
        for data in input_list:
            self.decoder.stdin.write(str.encode(data) + b'\n')
        self.decoder.stdin.flush()
        size = len(input_list)
        while size != 0:
            # Cutting away the \n
            output_list.append(self.decoder.stdout.readline()[:-1])
            self.decoder.stdout.readline()
            size -= 1
        self.mutex.unlock()
        return output_list

    def close(self):
        self.mutex.lock()
        self.decoder.stdin.close()
        self.decoder.wait()
        self.mutex.unlock()

    def getName(self):
        return self.filename


'''
Async Interface

'''

class DecoderQueryErrorCode(Enum):
    NO_ERROR = 0
    DECODE_ERROR = 1

class DecoderQueryError:
    __slots__ = ["error", "cookie"]
    def __init__(self, error, cookie):
        self.error = error
        self.cookie = cookie

class DecoderQueryAsyncResult:
    __slots__ = ["data", "cookie"]
    def __init__(self, data, cookie):
        self.data = data
        self.cookie = cookie

class DecoderQueryAsync(QThread):
    sig_completed = pyqtSignal(DecoderQueryAsyncResult, name="queryCompleted")
    sig_error = pyqtSignal(DecoderQueryError, name="queryError")

    def __init__(self, decoder, input_list, cookie):
        QThread.__init__(self)
        self.decoder = decoder
        self.input_list = input_list
        self.cookie = cookie

    def onCompleted(self, slot):
        self.sig_completed.connect(slot)

    def onError(self, slot):
        self.sig_error.connect(slot)

    def run(self):
        output_list = self.decoder.decode(self.input_list)
        self.sig_completed.emit(DecoderQueryAsyncResult(output_list, self.cookie))

'''
Unit Testing

'''
class DecoderQueryAsyncTestCase(QObject, unittest.TestCase):
    def __init__(self, test_case):
        QObject.__init__(self)
        unittest.TestCase.__init__(self, test_case)
        self.test_input_list_cookie = 1

    def setUp(self):
        filename_1 = "../tests/test_1"
        self.decoder_1 = Decoder(filename_1)

    def test_input_list(self):
        input_list = [hex(7753), hex(6772)]
        query = DecoderQueryAsync(self.decoder_1, input_list, self.test_input_list_cookie)
        query.onCompleted(self.onCompleted)
        query.run()

    def tearDown(self):
         self.decoder_1.close()

    @pyqtSlot(DecoderQueryAsyncResult, name="queryCompleted")
    def onCompleted(self, result):
        print(sys._getframe().f_code.co_name)
        print("\t data: " + str(result.data))
        print("\t cookie: " + str(result.cookie))
        if (result.cookie == self.test_input_list_cookie):
            self.assertEqual(result.data[0], b'std::thread::_State*& std::__get_helper<0ul, std::thread::_State*, std::default_delete<std::thread::_State> >(std::_Tuple_impl<0ul, std::thread::_State*, std::default_delete<std::thread::_State> >&)\n')
            self.assertEqual(result.data[1], b'std::thread::_Invoker<std::tuple<int (*)()> >::_Invoker(std::thread::_Invoker<std::tuple<int (*)()> >&&)\n')

if __name__ == '__main__':
    suite = unittest.TestSuite()
    suite.addTest(DecoderQueryAsyncTestCase('test_input_list'))
    runner = unittest.TextTestRunner()
    runner.run(suite)
