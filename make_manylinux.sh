#!/bin/sh
set -e

ver=$(echo $1 | sed -e s/\\.// -e 's/^\([0-9]\)/cp\1/')
actual_ver=$(ls /opt/python/ | grep $ver | head -1)
if [ -z "$actual_ver" ]; then
    echo "Version not found"
    exit 1
fi
/opt/python/$actual_ver/bin/python setup.py bdist_wheel --dist-dir /tmp
pushd /tmp
auditwheel show /tmp/*.whl
auditwheel repair /tmp/*.whl
popd
mkdir -p ./dist/
mv /tmp/wheelhouse/*.whl ./dist/
