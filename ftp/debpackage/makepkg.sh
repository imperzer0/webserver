#!/bin/bash
pkgname=fineftp-server
pkgver=1.5.1
url="https://github.com/eclipse-ecal/fineftp-server/archive/v$pkgver.tar.gz"
makedepends=('libasio-dev')


pkgdir="$(pwd)/$pkgname"
srcdir="$(pwd)/src"


MAKEPKG_init_dirs() {
  echo "-> MAKEPKG_init_dirs($pkgname) in $(pwd)"

  rm -rfv "$srcdir" && echo "Removed old source directory" || exit 1;
  rm -rfv "$pkgdir" && echo "Removed old package directory" || exit 1;
  mkdir -pv "$srcdir";
  mkdir -pv "$pkgdir";

  cd "$srcdir" || exit 1;
}

MAKEPKG_download_sources() {
  echo "-> MAKEPKG_download_sources($pkgname:$srcdir) $(pwd)"

  curl -LO "$url" || exit 2;
  tar -xvf "v$pkgver.tar.gz" -C "$srcdir/." || exit 2;
}

MAKEPKG_copy_files() {
  echo "-> MAKEPKG_copy_files($pkgname) in $(pwd)"

  _srcprefix="../.."

  source=(
    "Findasio.cmake.patch"
    "ftp_event_handler.cpp"
    "ftp_event_handler.h"
    "ftp_session.cpp.patch"
  )

  for _src in ${source[*]}; do
    cp -rfv "$_srcprefix/$_src" "$srcdir";
  done

  patch --forward --strip=1 --input="Findasio.cmake.patch" "$pkgname-$pkgver/thirdparty/asio-module/Findasio.cmake"
  patch --forward --strip=1 --input="$srcdir/ftp_session.cpp.patch" "$srcdir/$pkgname-$pkgver/fineftp-server/src/ftp_session.cpp"
  cp -rfv "$srcdir/ftp_event_handler.cpp" "$srcdir/$pkgname-$pkgver/fineftp-server/src/"
  cp -rfv "$srcdir/ftp_event_handler.h" "$srcdir/$pkgname-$pkgver/fineftp-server/src/"
}

MAKEPKG_install_deps() {
  echo "-> MAKEPKG_install_deps($pkgname)"
  sudo apt install -y "${makedepends[*]}" || exit 10;
  sudo apt-mark auto "${makedepends[*]}";
}

MAKEPKG_build() {
  echo "-> MAKEPKG_build($pkgname) in $(pwd)"

  _cwd="$(pwd)"

  cd $pkgname-$pkgver || exit 5;
  mkdir -pv _build
  cd _build || exit 5;

  cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr || exit 5;
  make -j 8 || exit 6;

  cd "$_cwd" || exit 6;
}

MAKEPKG_package() {
  echo "-> MAKEPKG_package($pkgname-$pkgver) in $(pwd)"

  _cwd="$(pwd)"

  cd $pkgname-$pkgver || exit 7;
  cd _build || exit 7;

  DESTDIR="$pkgdir" make install || exit 7;

  cd "$_cwd" || exit 7;
}

MAKEPKG_create_control_file() {
  echo "-> MAKEPKG_create_control_file($pkgdir) in $(pwd)"
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

MAKEPKG_dpkg_build() {
  echo "-> MAKEPKG_dpkg_build($pkgname) in $(pwd)"

  cd ..
  dpkg-deb --verbose --root-owner-group --build "./$pkgname" || exit 8;
}


MAKEPKG_init_dirs;
MAKEPKG_download_sources;
MAKEPKG_copy_files;
MAKEPKG_install_deps;
MAKEPKG_build;
MAKEPKG_package;
MAKEPKG_create_control_file;
MAKEPKG_dpkg_build;
