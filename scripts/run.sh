# MIT License
#
# Copyright (c) 2019 Marcin Harasimczuk
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

#!/bin/bash
echo "Running testapps..."
export LD_PRELOAD="${LIBSNOOP_PATH:-../libsnoop.so}"
export TESTS_DIR="${TESTS_PATH:-.}"
export OUTPUT_DIR="${OUTPUT_PATH:-snoops}"
echo "LD_PRELOAD=$LD_PRELOAD"
echo "TESTS_DIR=$TESTS_DIR"

for test_bin in $(ls $TESTS_DIR/test_*); do
    echo "Running >>> $test_bin"
    ./$test_bin
    echo "Done... <<< $test_bin"
done

echo "Running testapps... done"
echo "Use snooper app to inspect .snoop files"
echo "gl'n'hf..."
