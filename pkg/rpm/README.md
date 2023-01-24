# Build RPM RapidDisk Packages

Prepare your build environment.
```console
# mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
# echo '%_topdir %(echo $HOME)/rpmbuild' > ~/.rpmmacros
```

In your distribution, ensure that the `rpm-build` packages is already installed. The package name
may vary from distribution to distribution.

Copy the appropriate spec file into the ~/rpmbuild/SPECS directory and rename it to rapiddisk.spec.
Then copy the compressed tarball of rapiddisk into the ~/rpmbuild/SOURCES directory.

Run the following command:
```console
# rpmbuild -bb ~/rpmbuild/SPECS/rapiddisk.spec
```

If the build succeeds, the output rpm files will be placed in the ~/rpmbuild/RPMS directory and
under the target architecture (i.e x86_64).
