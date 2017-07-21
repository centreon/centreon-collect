# Prepare build directory.
rm -rf debuildir
mkdir debuildir
cp "${PROJECT}-${VERSION}.tar.gz" "debuildir/${PROJECT}_${VERSION}.orig.tar.gz"
cd debuildir
tar xzf "${PROJECT}_${VERSION}.orig.tar.gz"
cp -r "../packaging-${PROJECT}/debian/3.5" "${PROJECT}-${VERSION}/debian"
cp -r "../packaging-${PROJECT}/src/dev" "${PROJECT}-${VERSION}/debian/src"
sed -e "s/@VERSION@/${VERSION}/g" -e "s/@RELEASE@/${RELEASE}/g" < "${PROJECT}-${VERSION}/debian/changelog.in" > "${PROJECT}-${VERSION}/debian/changelog"
cd ..

# Launch debuild.
containerid=`docker create ci.int.centreon.com:5000/mon-build-dependencies:debian9 sh -c "cd /usr/local/src/debuildir/${PROJECT}-${VERSION} && debuild -us -uc -i"`
docker cp debuildir "$containerid:/usr/local/src/debuildir"
docker start -a "$containerid"

# Send package to repository.
docker cp "$containerid:/usr/local/src/debuildir/${PROJECT}_${VERSION}-${RELEASE}_amd64.deb" .
put_internal_debs "3.5" "stretch" *.deb

# Stop container.
docker stop "$containerid"
docker rm "$containerid"
