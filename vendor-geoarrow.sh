
rm -rf src/vendored/geoarrow

GEOARROW_C_REF="62b71c603bfea1ccd89a9f90784635d6aeb3945d"

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
