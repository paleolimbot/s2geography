
rm -rf src/vendored/geoarrow

GEOARROW_C_REF="b677e8478bd24bb2d6534d23dc289a7619f9afa9"

curl -L \
    "https://github.com/geoarrow/geoarrow-c/archive/${GEOARROW_C_REF}.zip" \
    -o geoarrow.zip

unzip -d . geoarrow.zip

CMAKE_DIR=$(find . -name "geoarrow-c-*")

mkdir geoarrow-cmake
pushd geoarrow-cmake
cmake "../${CMAKE_DIR}" \
  -DGEOARROW_BUNDLE=ON -DGEOARROW_USE_RYU=ON -DGEOARROW_USE_FAST_FLOAT=ON \
  -DGEOARROW_NAMESPACE=S2GeographyGeoArrow
cmake --build .
cmake --install . --prefix=../src/vendored/geoarrow
popd

rm geoarrow.zip
rm -rf geoarrow-c-*
rm -rf geoarrow-cmake
