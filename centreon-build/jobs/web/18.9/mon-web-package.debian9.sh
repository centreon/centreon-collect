# Prepare build directory.
rm -rf debuildir
mkdir debuildir
cd debuildir
cp "../centreon-${VERSION}.tar.gz" "centreon_${VERSION}.orig.tar.gz"
tar xzf "centreon_${VERSION}.orig.tar.gz"
cp -r "../packaging-${PROJECT}/debian/18.9" "centreon-${VERSION}/debian"
cp -r "../packaging-${PROJECT}/src/18.9" "centreon-${VERSION}/debian/src"
sed -e "s/@VERSION@/${VERSION}/g" -e "s/@RELEASE@/${RELEASE}.debian9/g" < "centreon-${VERSION}/debian/changelog.in" > "centreon-${VERSION}/debian/changelog"
cd ..

# Launch debuild.
containerid=`docker create ci.int.centreon.com:5000/mon-build-dependencies-18.9:debian9 sh -c "cd /usr/local/src/debuildir/centreon-${VERSION} && debuild -us -uc -i"`
docker cp debuildir "$containerid:/usr/local/src/debuildir"
docker start -a "$containerid"

# Send package to repository.
rm -rf debuildir
docker cp "$containerid:/usr/local/src/debuildir" .
put_internal_debs "18.9" "stretch" debuildir/*.deb

# Stop container.
docker stop "$containerid"
docker rm "$containerid"
