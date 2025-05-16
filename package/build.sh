#!/bin/bash

# Build Debian package
if command -v dpkg-buildpackage &> /dev/null; then
    echo "Building Debian package..."
    dpkg-buildpackage -us -uc
    echo "Debian package created: ../ngterm_1.0_amd64.deb"
fi

# Build RPM package
if command -v rpmbuild &> /dev/null; then
    echo "Building RPM package..."
    cd rpm
    rpmbuild -ba ngterm.spec
    echo "RPM package created: ~/rpmbuild/RPMS/x86_64/ngterm-1.0-1.x86_64.rpm"
fi
