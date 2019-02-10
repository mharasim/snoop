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
import os
import logging

from PyQt5 import QtGui
from PyQt5 import QtCore
from PyQt5 import QtWidgets
from PyQt5 import uic

from PyQt5.QtCore import pyqtSignal
from PyQt5.QtCore import pyqtSlot

from decoder import DecoderManager

logging.basicConfig(
    format="%(asctime)-15s [%(levelname)s] %(funcName)s: %(message)s",
    level=logging.DEBUG)

def dec(entry):
    ret = "??"
    try: ret = entry.decode("utf-8")
    except AttributeError: logging.debug("(%s) decode failure")
    return ret

class SnoopFile():
    def __init__(self):
        self.pos = 0
        # Number of bytes in address
        self.entry_size = 8
        # TODO
        self.cache = {}
        self.me = self.__class__.__name__

    def open(self, filename):
        logging.debug("(%s) %s", self.me, filename)

        self.snoop_file = open(filename, "rb")
        self.snoop_file.seek(0, 2)
        self.size = int(self.snoop_file.tell() / self.entry_size)
        self.snoop_file.seek(0, 1)

        mapFileName = self.mapFileForSnoop(filename)

        self.decoder_manager = DecoderManager(mapFileName)
        self.decoder_manager.debugPrint()

    def seek(self, pos):
        logging.debug("(%s) %d", self.me, pos)
        assert pos <= self.size, "out of bounds seek pos"
        self.snoop_file.seek(pos * self.entry_size)
        self.pos = pos

    def readToMatch(self, phrase, amount):
        logging.debug("(%s) %s pos %d size %d", self.me, phrase, self.pos, self.size)
        old_pos = self.pos

        # Search from current pos to EOF
        old_dec_out = []
        while (self.pos < self.size):
            dec_out = self.read(amount)
            for out in dec_out:
                if phrase in dec(out):
                    if len(dec_out) < amount:
                        return old_dec_out[len(dec_out):] + dec_out, self.pos
                    return dec_out, self.pos
            old_dec_out = dec_out

        # Search from beginning to current pos
        self.seek(0)
        while (self.pos < old_pos):
            dec_out = self.read(amount)
            for out in dec_out:
                if phrase in dec(out):
                    return dec_out, self.pos

        # Not found - seek to initial pos
        self.seek(old_pos)
        return [], self.pos

    def read(self, size):
        logging.debug("(%s) pos=%d size=%u total_size=%u", self.me, self.pos, size, self.size)
        entry_size = self.entry_size
        raw_in = self.snoop_file.read(size * entry_size)
        raw_len = len(raw_in)
        dec_in = []
        assert raw_len % entry_size == 0, "malformed snoop file read"
        for cnt in [i * entry_size for i in range(raw_len // entry_size)]:
            entry = raw_in[cnt : cnt + entry_size - 1][::-1].hex()
            dec_in.append(entry)
        dec_out = self.decoder_manager.decode(dec_in)
        self.pos += raw_len // entry_size
        return dec_out

    def close(self):
        logging.debug("(%s)", self.me)
        self.snoop_file.close()
        self.decoder_manager.close()

    def mapFileForSnoop(self, filename):
        logging.debug("(%s) %s", self.me, filename)

        path = os.path.dirname(filename)
        pid_str = os.path.basename(filename).split('.')[0].split('_')[-1]
        mapFileName = path + "/" + pid_str + ".map"
        return mapFileName


class SlidingView():
    def __init__(self, filename, widget, size):
        self.me = self.__class__.__name__
        logging.debug("(%s) %s", self.me, filename)

        self.snoop = SnoopFile()
        self.widget = widget
        self.size = size
        self.pos = 0

        self.snoop.open(filename)
        self.place(0)

    def place(self, pos):
        logging.debug("(%s) %d", self.me, pos)
        self.snoop.seek(pos)
        entries = self.snoop.read(self.size)
        for entry in entries:
            self.widget.addItem(dec(entry))
        self.pos = pos

    def adjust(self, amount):
        logging.debug("(%s) amount=%d pos=%d", self.me, amount, self.pos)
        entries = []
        adjust_up = amount < 0
        amount = abs(amount)
        if (adjust_up):
            if (self.pos == 0):
                return False
            if (self.pos - amount < 0):
                amount = self.pos
            self.pos -= amount
            self.snoop.seek(self.pos)
            entries = self.snoop.read(amount)
            self.snoop.seek(self.pos + self.size)
        else:
            if (self.pos + self.size == self.snoop.size):
                return False
            if (self.pos + self.size + amount >= self.snoop.size):
                amount = self.snoop.size - self.pos - self.size
            self.pos += amount
            entries = self.snoop.read(amount)

        for idx, entry in enumerate(entries):
            if (adjust_up):
                self.widget.takeItem(self.widget.count() - 1)
                self.widget.insertItem(idx, dec(entry))
            else:
                self.widget.takeItem(0)
                self.widget.addItem(dec(entry))

        logging.debug("(%s) -> amount=%d pos=%d", self.me, amount, self.pos)
        return True

    def search(self, phrase):
        logging.debug("(%s) %s", self.me, phrase)

        items = self.widget.findItems(phrase, QtCore.Qt.MatchContains)
        next_item_row = None
        for item in items:
            candidate_item_row = self.widget.row(item)
            if candidate_item_row > self.widget.currentRow():
                next_item_row = candidate_item_row
                break

        if (next_item_row  == None):
            entries, pos = self.snoop.readToMatch(phrase, self.size)
            if (len(entries) == 0):
                logging.info("(%s) %s - not found", self.me, phrase)
                return False
            self.pos = pos - self.size
            self.widget.clear()
            for entry in entries:
                self.widget.addItem(dec(entry))
            items = self.widget.findItems(phrase, QtCore.Qt.MatchContains)
            if (len(items) == 0):
                logging.error("(%s) %s - not found but read matched (should not happen)", self.me, phrase)
                return False
            self.widget.setCurrentItem(items[0])
        else:
            self.widget.setCurrentRow(next_item_row)

        return True

    def close(self):
        self.widget.clear()
        self.snoop.close()

class SnoopWindow(QtWidgets.QMainWindow):
    def __init__(self):
        self.me = self.__class__.__name__
        logging.debug("(%s)", self.me)
        QtWidgets.QMainWindow.__init__(self)
        uic.loadUi('SnoopWindow.ui', self)
        self.current_view = None
        self.scroll_offset = 10
        self.size = 200
        self.ignore_slider = False
        self.last_search = ""

    def slider(self):
        return self.listWidget.verticalScrollBar()

    def onSliderMin(self):
        logging.debug("(%s)", self.me)
        self.ignore_slider = True
        if (self.current_view.adjust(-self.scroll_offset)):
            self.slider().setValue(self.slider().minimum() + 1)
        self.ignore_slider = False

    def onSliderMax(self):
        logging.debug("(%s)", self.me)
        self.ignore_slider = True
        if (self.current_view.adjust(self.scroll_offset)):
            self.slider().setValue(self.slider().maximum() - 1)
        self.ignore_slider = False

    def loadSnoop(self, filename):
        if (self.current_view != None):
            self.current_view.close()
        self.current_view = SlidingView(filename, self.listWidget, self.size)

    def onLoadSnoop(self):
        filename = QtWidgets.QFileDialog.getOpenFileName(self, self.tr("File dialog"),
                self.tr(""), self.tr("Snoop files (*.snoop)"))
        if filename[0]:
            self.loadSnoop(filename[0])

    def onClose(self):
        if (self.current_view != None):
            self.current_view.close()
        sys.exit()

    def onSearchSnoop(self):
        if (self.current_view == None):
            self.onLoadSnoop()
        if (self.current_view == None):
            return
        text, ok = QtWidgets.QInputDialog.getText(self, self.tr("Snoop search dialog"),
                self.tr("Search phrase (in current snoop):"), text=self.last_search)
        if ok:
            self.ignore_slider = True
            self.current_view.search(str(text))
            self.last_search = str(text)
            self.ignore_slider = False

    @pyqtSlot(int, name="onSliderChanged")
    def onSliderChanged(self, value):
        if (self.ignore_slider):
            return
        if (value == self.slider().minimum()):
            self.onSliderMin()
        if (value == self.slider().maximum()):
            self.onSliderMax()

    @pyqtSlot()
    def onSliderPressed(self):
        logging.debug("(%s)", self.me)

    def setup(self):
        self.load_snoop = QtWidgets.QAction(self.tr("Load snoop"), self.menuFile)
        self.load_snoop.triggered.connect(self.onLoadSnoop)
        self.load_snoop.setShortcut(QtGui.QKeySequence(self.tr("Ctrl+L")))

        self.search_snoop = QtWidgets.QAction(self.tr("Search snoop"), self.menuFile)
        self.search_snoop.triggered.connect(self.onSearchSnoop)
        self.search_snoop.setShortcut(QtGui.QKeySequence(self.tr("Ctrl+F")))

        self.close_action = QtWidgets.QAction(self.tr("Close"), self.menuFile)
        self.close_action.triggered.connect(self.onClose)
        self.close_action.setShortcut(QtGui.QKeySequence(self.tr("Ctrl+Q")))

        self.menuFile.addAction(self.load_snoop)
        self.menuFile.addAction(self.search_snoop)
        self.menuFile.addAction(self.close_action)

        self.listWidget.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOn)
        self.slider().valueChanged.connect(self.onSliderChanged)
        self.slider().sliderPressed.connect(self.onSliderPressed)

if __name__ == '__main__':
    app = QtWidgets.QApplication(sys.argv)
    window = SnoopWindow()
    window.show()
    window.setup()
    sys.exit(app.exec_())

