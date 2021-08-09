# Prepare build directory.
rm -rf debuildir
mkdir debuildir
cp "${PROJECT}-${VERSION}.tar.gz" "debuildir/${PROJECT}_${VERSION}.orig.tar.gz"
cd debuildir
tar xzf "${PROJECT}_${VERSION}.orig.tar.gz"
cd ..
cp -r `dirname $0`/../../../packaging/clib/21.10/debian "debuildir/${PROJECT}-${VERSION}/"
sed -e "s/@VERSION@/${VERSION}/g" -e "s/@RELEASE@/${RELEASE}/g" < "debuildir/${PROJECT}-${VERSION}/debian/changelog.in" > "debuildir/${PROJECT}-${VERSION}/debian/changelog"

# Launch debuild.
containerid=`docker create registry.centreon.com/mon-build-dependencies-21.10:debian10-armhf sh -c "cd /usr/local/src/debuildir/${PROJECT}-${VERSION} && export CC=arm-linux-gnueabihf-gcc && export CXX=arm-linux-gnueabihf-g++ && dpkg-buildpackage -us -uc -d -aarmhf"`
docker cp debuildir "$containerid:/usr/local/src/debuildir"
docker start -a "$containerid"

# Send package to repository.
rm -rf debuildir
docker cp "$containerid:/usr/local/src/debuildir" .
put_internal_debs "21.10" "buster" debuildir/*.deb

# Stop container.
docker stop "$containerid"
docker rm "$containerid"
