#!/bin/bash

pkgname="webserver"
pkgver="2.11"
makedepends=('cmake' 'git' 'gcc' 'make' 'openssl' 'curl' 'libcurl4-openssl-dev')

mongoose_version="7.12"

pkgdir="$(pwd)/$pkgname"
srcdir="$(pwd)/src"


mksrc() {
  echo "-> mksrc($pkgname) in $(pwd)"

  rm -rfv src && echo "Removed old source directory" || exit 1;
  mkdir -pv src;
  cd src || exit 1;
}

copy_files() {
  _srcprefix="../.."
  echo "-> copy_files($pkgname:$_srcprefix->$srcdir) in $(pwd)"

  cp -rfv "$_srcprefix/CMakeLists.txt" "$srcdir/.";

  cp -rfv "$_srcprefix/sources/" "$srcdir/.";
  cp -rfv "$_srcprefix/resources/" "$srcdir/.";
  cp -rfv "$_srcprefix/ftp/" "$srcdir/.";


  external=()
  file=()
  sha512sums=()

  external=(${external[@]} "https://raw.githubusercontent.com/cesanta/mongoose/$mongoose_version/mongoose.c")
  file=(${file[@]} "mongoose.c")
  sha512sums=(${sha512sums[@]} "55011c1328abcfa91897b4b29aa5e1ab6c5f5f351442b8fc247ee9d9751dbbce1ab1892f9f996f140a18719a49412f45aad6c6bdbf7eed3348cb01982293daea")

  external=(${external[@]} "https://raw.githubusercontent.com/cesanta/mongoose/$mongoose_version/mongoose.h")
  file=(${file[@]} "mongoose.h")
  sha512sums=(${sha512sums[@]} "d13e87f326c285ce3aa7e837edb826d38f8bb73c72ac572af44f42447e9456f820f6d42e2248a33c18a3813b51f9e76ecfc99bba077d99a8f61d3b0b99b5cecd")

  external=(${external[@]} "https://raw.githubusercontent.com/imperzer0/strscan/2f263154679e67ed44aa7fc4ae65829547e8290b/strscan.c")
  file=(${file[@]} "strscan.c")
  sha512sums=(${sha512sums[@]} "0f62f4fb922325e53713d7013b709d65d50a2c94a440258b666d357bf35af2f5727b2014fbf955e15f169acc8e444eca84b309419886bbc98d0b563c4b795bc6")

  external=(${external[@]} "https://raw.githubusercontent.com/imperzer0/strscan/2f263154679e67ed44aa7fc4ae65829547e8290b/strscan.h")
  file=(${file[@]} "strscan.h")
  sha512sums=(${sha512sums[@]} "eb472a2ad6997f107d5f7db6f311294f34066f08fd6d7bf8c6be7f11322d8f26f50c678c6445c1f7643b32b3a28cfcd5576acc9b1af25ef33f41e3764165090d")

  for i in ${!external[@]}; do
    _file="$srcdir/${file[i]}"
    curl -L ${external[i]} -o $_file;
    echo -n "Checking [$_file] sha512sum: "
    echo "${sha512sums[i]} $_file" | sha512sum --check || exit 2;
  done

  source=()
  sha512sums=()

  source=(${source[@]} "resources/favicon.ico")
  sha512sums=(${sha512sums[@]} "72850225ffda45dff7f87645e80512ddce302fec175df7adb0e6d8d91f5f5a2131250fe91510b35f19cf05c1d229aa9eb8f71594c918555acb0418f3c2975cff")

  source=(${source[@]} "resources/CascadiaMono.woff")
  sha512sums=(${sha512sums[@]} "180d3248b16d5d3ed3aca598eb67e7edb8ec8553c21edafe96d9d268989c0d7896a615c7e67527d1fca913308e1b24a363a59c7826b7e341012e853736db4fa6")

  for i in ${!source[@]}; do
    echo -n "Checking sha512sum: ";
    echo "${sha512sums[i]} $srcdir/${source[i]}" | sha512sum --check || exit 2;
  done
}

prepare() {
  echo "-> prepare($pkgname) in $(pwd)"

  _cwd="$(pwd)"

  cd "ftp/debpkg" || exit 3;

  echo " >> Executing makepkg.sh in [$(pwd)]..."
  bash "makepkg.sh" && echo "Success" || exit 3;

  sudo dpkg -i "fineftp-server.deb";

  sudo apt-mark auto fineftp-server

  cd "$_cwd" || exit 3;
}

install_deps() {
  echo "-> install_deps($pkgname)"

  sudo apt install ${makedepends[@]} || exit 10;
  sudo apt-mark auto ${makedepends[@]};
}

build() {
  echo "-> build($pkgname[v$pkgver]) in $(pwd)"

  _package_version=" ("$pkgver")"
  cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
    -DPACKAGE_VERSION="$_package_version" -DAPPNAME="$pkgname" . || exit 4;
  make -j 8 || exit 5;
}

package() {
  echo "-> package($pkgname) in $(pwd)"

  rm -rfv "${pkgdir:?}/usr/bin/$pkgname" || exit 6;
  install -Dm755 "$srcdir/$pkgname" "${pkgdir:?}/usr/bin/$pkgname" || exit 6;
}

dpkg_build() {
  echo "-> dpkg_build($pkgname) in $(pwd)"

  cd ..
  dpkg-deb --build "./$pkgname" || exit 8;
}


mksrc;
copy_files;
prepare;
install_deps;
build;
package;
dpkg_build;
