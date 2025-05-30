#!/bin/bash
pkgname=fineftp-server
pkgver=1.3.5
url="https://github.com/eclipse-ecal/fineftp-server/archive/v$pkgver.tar.gz"
makedepends=('libasio-dev')


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

download_sources() {
  echo "-> download_sources($pkgname:$srcdir) $(pwd)"

  curl -LO "$url" || exit 2;
  tar -xvf "v$pkgver.tar.gz" -C "$srcdir/." || exit 2;
}

copy_files() {
  echo "-> copy_files($pkgname) in $(pwd)"

  _srcprefix="../.."

  source=(
    "Findasio.cmake.patch"
    "custom_event_handler.hpp"
    "ftp_event_handler.h"
    "ftp_session.cpp.patch"
  )

  for _src in ${source[*]}; do
    cp -rfv "$_srcprefix/$_src" "$srcdir";
  done

  sha512sums=(
    '5a157af2c9cf573c2649ffecc99edba86383985c5adaba2ad318098c2709e907147c8ce5c359d423b04e60f237e37c3f81d59daaa0a4a7146245e668aa801865'
    '5e27098d89d601bc9c05f7ff0478e269dc70db04e030dfae7223c1881a6a618f5aafcdd0861550f51c2dda52ee314f8c5dc7676538dc0021c719d5a7a5b65eec'
    '6851b6a62b8d13e9d2ee96d83e28077df9df00d0b41fb4655856ba53b10c039969356f2a8e25f84875d58f4fdcc8f82be923f9570dca63ac3eb61027ac491546'
    'd2b4ee65af0cefacec0558943102276730fd14db4cfd0095579809aaa3136dd3e7dbb634ff7847bad09ee205c06d0b6b64b0d9345155eb8619960e1f8d3b7636'
  )

  for i in ${!source[*]}; do
    echo -n "Checking sha512sum: ";
    echo "${sha512sums[i]} $srcdir/${source[i]}" | sha512sum --check || exit 3;
  done

  patch --forward --strip=1 --input="Findasio.cmake.patch" "$pkgname-$pkgver/cmake/Findasio.cmake"
  patch --forward --strip=1 --input="$srcdir/ftp_session.cpp.patch" "$srcdir/$pkgname-$pkgver/fineftp-server/src/ftp_session.cpp"
  cp -rfv "$srcdir/custom_event_handler.hpp" "$srcdir/$pkgname-$pkgver/fineftp-server/src/"
  cp -rfv "$srcdir/ftp_event_handler.h" "$srcdir/$pkgname-$pkgver/fineftp-server/src/"
}

install_deps() {
  echo "-> install_deps($pkgname)"
  sudo apt install -y "${makedepends[*]}" || exit 10;
  sudo apt-mark auto "${makedepends[*]}";
}

build() {
  echo "-> build($pkgname) in $(pwd)"

  _cwd="$(pwd)"

  cd $pkgname-$pkgver || exit 5;
  mkdir -pv _build
  cd _build || exit 5;

  cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr || exit 5;
  make -j 8 || exit 6;

  cd "$_cwd" || exit 6;
}

package() {
  echo "-> package($pkgname-$pkgver) in $(pwd)"

  _cwd="$(pwd)"

  cd $pkgname-$pkgver || exit 7;
  cd _build || exit 7;

  DESTDIR="$pkgdir" make install || exit 7;

  cd "$_cwd" || exit 7;
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
Depends: libasio-dev
Description: 📦 C++ FTP Server Library for Windows 🪟, Linux 🐧 & more 💾
EOF
}

dpkg_build() {
  echo "-> dpkg_build($pkgname) in $(pwd)"

  cd ..
  dpkg-deb --verbose --build "./$pkgname" || exit 8;
}


init_dirs;
download_sources;
copy_files;
install_deps;
build;
package;
create_control_file;
dpkg_build;
