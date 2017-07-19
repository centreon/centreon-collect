# Prepare build directory.
rm -rf debuildir
mkdir debuildir
cp "$PROJECT-$VERSION.tar.gz" "debuildir/$PROJECT_$VERSION.orig.tar.gz"
cd debuildir
tar xzf "$PROJECT_$VERSION.orig.tar.gz"
cp -r "../packaging-$PROJECT/debian" "$PROJECT-$VERSION/"
sed -e "s/@VERSION@/$VERSION/g" -e "s/@RELEASE@/$RELEASE/g" < "$PROJECT-$VERSION/debian/changelog.in" > "$PROJECT-$VERSION/debian/changelog"
cd ..

# Launch debuild.
containerid=`docker create ci.int.centreon.com:5000/mon-build-dependencies:debian9 sh -c "cd /usr/local/src/debuildir && debuild -us -uc -i"`
docker cp debuildir "$containerid:/usr/local/src/debuildir"
docker start "$containerid"

# Send package to repository.
docker cp "$containerid:/usr/local/src/debuildir/$PROJECT_$VERSION-$RELEASE_amd64.deb" .
# XXX

# Stop container.
docker stop "$containerid"
docker rm "$containerid"
