#!/bin/bash
pkgname="webserver"
pkgver="2.11"
makedepends=('cmake' 'git' 'gcc' 'make' 'openssl' 'curl' 'libcurl4-openssl-dev')

pkgdir="$(pwd)/$pkgname"
srcdir="$(pwd)/src"


init_dirs() {
  echo "-> init_dirs($pkgname) in $(pwd)"

  rm -rfv "$srcdir" && echo "Removed old source directory" || exit 1;
  rm -rfv "$pkgdir" && echo "Removed old package directory" || exit 1;
  mkdir -pv "$srcdir";
  mkdir -pv "$pkgdir";

  cd "$srcdir" || exit 1;
}

copy_files() {
  _srcprefix="../.."
  echo "-> copy_files($pkgname:$_srcprefix->$srcdir) in $(pwd)"

  cp -rfv "$_srcprefix/CMakeLists.txt" "$srcdir/.";

  cp -rfv "$_srcprefix/sources/" "$srcdir/.";
  cp -rfv "$_srcprefix/resources/" "$srcdir/.";
  cp -rfv "$_srcprefix/ftp/" "$srcdir/.";
  cp -rfv "$_srcprefix/mongoose/" "$srcdir/.";
  cp -rfv "$_srcprefix/strscan/" "$srcdir/.";



  source=()
  sha512sums=()

  source=(${source[*]} "resources/favicon.ico")
  sha512sums=(${sha512sums[*]} "72850225ffda45dff7f87645e80512ddce302fec175df7adb0e6d8d91f5f5a2131250fe91510b35f19cf05c1d229aa9eb8f71594c918555acb0418f3c2975cff")

  source=(${source[*]} "resources/CascadiaMono.woff")
  sha512sums=(${sha512sums[*]} "180d3248b16d5d3ed3aca598eb67e7edb8ec8553c21edafe96d9d268989c0d7896a615c7e67527d1fca913308e1b24a363a59c7826b7e341012e853736db4fa6")

  for i in ${!source[*]}; do
    echo -n "Checking sha512sum: ";
    echo "${sha512sums[i]} $srcdir/${source[i]}" | sha512sum --check || exit 2;
  done
}

prepare() {
  echo "-> prepare($pkgname) in $(pwd)"

  _cwd="$(pwd)"

  cd "ftp/debpackage" || exit 3;

  echo " >> Executing makepkg.sh in [$(pwd)]..."
  bash "makepkg.sh" && echo "Success" || exit 3;

  sudo dpkg -i "fineftp-server.deb";

  sudo apt-mark auto fineftp-server

  cd "$_cwd" || exit 3;
}

install_deps() {
  echo "-> install_deps($pkgname)"

  sudo apt install -y ${makedepends[*]} || exit 10;
  sudo apt-mark auto ${makedepends[*]};
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

create_control_file() {
  mkdir -p "$pkgdir/DEBIAN"
  cat <<EOF > "$pkgdir/DEBIAN/control"
Package: $pkgname
Version: $pkgver
Essential: no
Priority: optional
Maintainer: imperzer0 <dmytroperets@gmail.com>
Architecture: $(dpkg --print-architecture)
Depends: openssl, gcc, curl, libcurl4-openssl-dev, fineftp-server
Description: Lightweight web server written in c++ for linux with ftp support
EOF
}

dpkg_build() {
  echo "-> dpkg_build($pkgname) in $(pwd)"

  cd ..
  dpkg-deb --verbose --build "./$pkgname" || exit 8;
}


init_dirs;
copy_files;
prepare;
install_deps;
build;
package;
create_control_file;
dpkg_build;


if [ $# -ge 1 ] && [ "$1" == "-i" ]; then
  echo "[Finishing up] Installing package {./$pkgname.deb}..."
  sudo dpkg -i "./$pkgname.deb"
fi